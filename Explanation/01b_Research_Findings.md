# 01b · Research Findings (Phase 2): StateHandoff

**Phase:** 2 (Research), completed 2026-11-21 · **Engine:** UE 5.8.4 (CL 58902052), read from `C:\Program Files\Epic Games\UE_5.8\Engine`

**Method:**
- Every engine claim cites a path relative to `Engine/` plus a line number in 5.8.4. Plugin paths use the short forms of [Phase2_Raw_Notes/README.md](Phase2_Raw_Notes/README.md): `MG/` = `Plugins/Runtime/MassGameplay/Source/`, `MAI/` = `Plugins/AI/MassAI/Source/`, `IA/` = `Plugins/Runtime/InstancedActors/Source/InstancedActors/`, `ME/` = `Source/Runtime/MassEntity/`.
- The claims that drive design decisions were opened and re-read before being written here; they are marked ✔.
- Claims drawn from following code flow rather than a direct statement are marked **[inferred]**. Things still to be measured are marked **[to measure]**. Anything I could not confirm is marked **[unverified]**.
- Web sources were opened unless marked (snippet only). The raw notes keep every quote: [A](Phase2_Raw_Notes/A_representation_internals.md) representation internals, [B](Phase2_Raw_Notes/B_translators_and_hooks.md) translators and hooks, [C](Phase2_Raw_Notes/C_framework_and_measurement.md) framework and measurement, [D](Phase2_Raw_Notes/D_existing_work.md) existing work.

**Why this phase matters.** Phase 1 assumed that "nothing hands off non-spatial state". Phase 2 found that this is **too strong**: the engine has several hooks, and Epic published a worked hand-off pattern in 2026. What is still missing is narrower and more precise, and the design and the baselines changed because of it. Section 6 states the revised gap.

---

## 1. What Phase 2 changed

| # | Finding | Consequence for StateHandoff |
|---|---|---|
| 1 | **The shipped translators move only spatial data** ✔. Transform, location, orientation, velocity, capsule transform; Mover's (Experimental) add desired movement and mover input. A `UMassTranslator_BehaviorTree` exists but its `Execute` is empty and it is not auto-registered: it syncs nothing ✔ (`MG/MassActors/Public/Translators/MassTranslators_BehaviorTree.h:32`, `.cpp:12`; full table in [B](Phase2_Raw_Notes/B_translators_and_hooks.md) B2) | The README's claim holds, with that precision added. A translator is a **per-frame processor**, not an event at representation change (`MG/MassSpawner/Public/MassTranslator.h:28–60`) ✔. Copying gameplay state with translators becomes baseline **B2** |
| 2 | **Mass-native state already survives a switch.** Mass StateTree instance data lives in `UMassStateTreeSubsystem`, referenced by a handle in an entity fragment, freed only when the entity's fragment is removed ✔ (`MAI/MassAIBehavior/Public/MassStateTreeFragments.h:11–22`, `MassStateTreeSubsystem.h:91`, `Private/MassStateTreeProcessors.cpp:103–109`). Mass Smart Object claims and interaction cooldowns live in `FMassSmartObjectUserFragment` ✔ (`MG/MassSmartObjects/Public/MassSmartObjectFragments.h:23–36`) | **Scope narrowed.** The project does not claim to fix StateTree or Mass Smart Object state. The problem is state owned by **actor-side** systems (components, GAS, AI controllers, perception, actor-made claims). Epic: GAS state across "LOD changes" is an open issue (D1 in [D](Phase2_Raw_Notes/D_existing_work.md)) |
| 3 | **Actor lifetime is decoupled from the representation.** The representation commits inside the processor; release is *queued* (`MassRepresentationProcessor.cpp:495–496`) and processed at the **end** of the PrePhysics phase (`MassActorSpawnerSubsystem.cpp:585–595`) ✔. Destruction is **wall-clock budgeted** (0.5 ms); actors past the budget are deactivated and destroyed on a later frame ✔ (`:509–543`; `MG/MassSimulation/Public/MassSimulationSettings.h:27`) | Ownership is moved at the **representation commit**, in a processor that runs `ExecuteAfter Representation.VisualizationProcessing` (the group the engine header tells consumers to wait for, `MassRepresentationTypes.h:28–30`) ✔. Hand-off tied to `EndPlay` is hypothesis **H1** |
| 4 | **Pooled actors get neither `EndPlay` nor `BeginPlay`.** Release to pool calls `PrepareForPooling` + `SetActorHiddenInGame(true)` only; retrieval calls `PrepareForGame` ✔ (`MassActorSpawnerSubsystem.cpp:119–140, 235–266`). Pooling is on by default (`mass.spawning.actorpooling`, `:24, :31`) for actors that implement `IMassActorPoolableInterface` | The hand-off may not depend on `BeginPlay`/`EndPlay`. Pooling is a **workload parameter** (off/on), and the lifecycle baseline B1 is made **pool-aware** (it also hooks `PrepareForPooling`/`PrepareForGame`), so it is not a strawman |
| 5 | **"Disabled" is not "gone".** With the default `bKeepLowResActors = true` (`MassRepresentationFragments.h:198`) and one template for both resolutions, the actor survives ISM periods, disabled ✔ (`MassRepresentationProcessor.cpp:168–198`). Disabling turns off the **actor** tick and collision only (`MassRepresentationActorManagement.cpp:30–45`); **component ticks keep running** ✔ (`Source/Runtime/Engine/Private/Actor.cpp:1754–1760`) | A disabled actor can still change its own copy. The contract says the entity owns state while the actor is disabled, and re-enabling is a bind (restore) even though no spawn happens. Hypothesis **H2** |
| 6 | **Three release paths:** queued (processor), immediate before a template swap (`OnPreActorSpawn`, `MassRepresentationActorManagement.cpp:72–89`) ✔, immediate on entity destruction (fragment destructor, `MassRepresentationProcessor.cpp:668`) ✔. `ReleaseAnyActorOrCancelAnySpawning` is `static`, not virtual ✔ (`MassRepresentationActorManagement.h:122, 136`) | The design covers all three: the commit processor (queued), an `OnPreActorSpawn` override that captures first (template swap), and an `OnEndPlay` safety net (anything else). Each is a separate fault-injection target (SC2) |
| 7 | **`BeginPlay` runs before the actor is mapped to its entity.** `SpawnActor` is non-deferred (`MassActorSpawnerSubsystem.cpp:334`), and the mapping is stored afterwards in `OnPostActorSpawn` (`MassRepresentationActorManagement.cpp:108`) ✔. Epic's sample retries its entity lookup on the next tick for this reason (D2) | Nothing in our system runs in `BeginPlay`. Binding happens at the commit, when the mapping exists. The first-frame window becomes a documented mistake in `07` |
| 8 | **Spawn timing is not deterministic, even with `-deterministic`.** Spawning, destruction and entity compaction are wall-clock time slices (`MassActorSpawnerSubsystem.cpp:363–371, 509–515`; `MassSimulationSettings.h:23, 27, 36–40`) ✔ | The checker is **event-driven**: it judges every transition when it happens, and divergence is normalised **per 1,000 transitions**, with the transition count recorded per run. Runs are never compared frame by frame |
| 9 | **The engine's own hooks are scattered and conditional**: `OnPostActorSpawn` (not called on re-enable), trait object initializers and the "entity detaching" delegate (only for actors with a `UMassAgentComponent`), pool callbacks (only when pooling), Instanced Actors' per-component hooks ✔ (table B4 in [B](Phase2_Raw_Notes/B_translators_and_hooks.md)) | They are the building blocks. StateHandoff uses two of them (a subclass of `UMassRepresentationActorManagement` for the template swap, `AActor::OnEndPlay` for the safety net) and adds the commit-time processor |
| 10 | **Epic published a hand-off pattern in 2026.** PersistenceLab (UE 5.8; talk "The Player Was Here", Unreal Fest Chicago 2026): per actor class, hydrate in `BeginPlay` (entity lookup with retries), dehydrate in `EndPlay`, health/nav/state fragments, a "hydrated" tag. "This code has not been battle tested in a shipped title." No license file, so no code is reused (D2) | **Repositioned**, see §6. This pattern is baseline **B1**, made pool-aware. The project credits it explicitly and measures it, rather than claiming the hand-off idea is new |
| 11 | **Epic staff, 13 Feb 2026:** "I do not believe we have any example of moving data from the Actor back to Mass when destroying the Actor" (D1) | Confirms that the actor→entity direction had no official example before PersistenceLab, and that nothing checks it |
| 12 | **No Iris path for Mass entities in 5.8.4.** `MassReplication` has no Iris code and replicates through `FFastArraySerializer` bubble actors ✔ (`MG/MassReplication/MassReplication.Build.cs:13–29`; `Public/MassClientBubbleSerializerBase.h:23`) | Single-player scope confirmed. Replicated hand-off goes to `12_Future_Scalability.md` |
| 13 | **`UMassVisualizationTrait` is soft-deprecated in 5.8** ("Use MassStationaryVisualizationTrait or MassMovableVisualizationTrait", `MassVisualizationTrait.h:18–19`) ✔. The visualization, visualization-LOD and LOD-collector processors are **not auto-registered** (`MassRepresentationProcessor.cpp:54`, `MassVisualizationLODProcessor.cpp:13`, `MG/MassLOD/Private/MassLODCollectorProcessor.cpp:14`); MassCrowd subclasses them with a tag filter and turns registration on ✔ (`Plugins/AI/MassCrowd/Source/MassCrowd/Private/MassCrowdVisualizationProcessor.cpp:24`) | The workload uses `UMassMovableVisualizationTrait` and three small processor subclasses filtered by its own tag, the same way MassCrowd does |
| 14 | **The default `LODMaxCount` is {50, 100, 500, ∞}** ✔ (`MassRepresentationFragments.h:313`): at most about 150 actors at once, whatever N is | The workload sets its own LOD counts and distances, fixed by calibration in Phase 5, so a run produces thousands of actor transitions |

**Measurement-plan changes:**

| # | Finding | Consequence |
|---|---|---|
| M-1 | `MassInsights` is an **Unreal Insights** plugin (`EditorAndProgram` modules for the UnrealInsights program, `Plugins/MassInsights/MassInsights.uplugin:14–31`). It analyses the `MassChannel` trace: phases, chunk execution, archetype moves (`MassEntityMoved`) (`ME/Private/MassEntityTrace.cpp:22–118`). Trace needs `WITH_MASSENTITY_DEBUG`, which is **on in Development, off in Test/Shipping** (`ME/Public/MassEntityTrace.h:10–12`; `Source/Runtime/Mass/MassCore/Public/Mass/EntityMacros.h:8`) ✔ | MassInsights is used for drill-downs into flip frames (where did the time go), not for correctness. Packaged **Development** (the headline build, as in the portfolio) keeps the Mass trace available |
| M-2 | Memory per entity is printed by `mass.LogArchetypes` ("Bytes / Entity", chunk counts) ✔ (`ME/Private/MassDebugger.cpp:312`; `MassArchetypeData.cpp:1404`); actor and component memory by `obj list class=` (`Source/Runtime/Engine/Private/UnrealEngine.cpp:9607, 10004`) | "Memory per agent per representation" = archetype bytes per entity + actor/component KB per live actor + ISM per-instance data, recorded once per config |
| M-3 | Mass publishes CSV stats `MassActors/NumSpawned` and `NumPooled` ✔ (`MassActorSpawnerSubsystem.cpp:581–582`) | Recorded alongside our own `StateHandoff` CSV category |
| M-4 | `-deterministic` also sets Mass's `IsDeterministic()` (`FApp::bUseFixedSeed`) ✔ (`Source/Runtime/Launch/Private/LaunchEngineLoop.cpp:2456–2461`; `MG/MassCommon/Private/MassCommonUtils.cpp:38–41`), which removes the random first-update spread of culled chunks (`MassRepresentationProcessor.cpp:583, 590`) | Fixed-step runs, as in the portfolio protocol; the remaining non-determinism is finding 8 |

---

## 2. Representation switching (evidence)

| Fact | Evidence (5.8.4) |
|---|---|
| `EMassRepresentationType { HighResSpawnedActor, LowResSpawnedActor, SkinnedMeshInstance, StaticMeshInstance, None }` | `MG/MassRepresentation/Public/MassRepresentationTypes.h:35–43` ✔ |
| Default `LODRepresentation = { HighRes, LowRes, StaticMeshInstance, None }`; `bKeepLowResActors = true`; `bKeepActorExtraFrame = false`; `bWaitForActorVisualReadiness = false` | `MassRepresentationFragments.h:173, 198, 202, 211` ✔ |
| `CurrentRepresentation`, `PrevRepresentation`, `IsWaitingForActorVisualReadiness()` are readable per entity | `MassRepresentationFragments.h:59, 62, 109` ✔ |
| Virtual hooks on `UMassRepresentationActorManagement`: `GetOrSpawnActor`, `SetActorEnabled`, `TeleportActor`, `IsActorReadyForRepresentation`, `OnPreActorSpawn`, `OnPostActorSpawn` | `MassRepresentationActorManagement.h:47, 58, 67, 94, 102, 111` ✔ |
| The management class is chosen per entity type through `FMassRepresentationParameters::RepresentationActorManagementClass` | `MassRepresentationFragments.h:168–169` ✔ |
| Actor wanted and a matching actor exists → the **same actor is re-enabled**, no spawn, no `OnPostActorSpawn` | `MassRepresentationProcessor.cpp:233–264, 296–299` ✔ |
| Actor destroyed behind Mass's back → `CurrentRepresentation = None`, a new actor will be spawned | `MassRepresentationProcessor.cpp:157–165` ✔ |
| Readiness wait: the actor exists but stays disabled while the mesh is shown, up to 255 frames by default | `MassRepresentationProcessor.cpp:38–45, 284–293, 386–442` |
| Spawning happens at the start of PrePhysics (1.5 ms budget), destruction at the end (0.5 ms budget) | `MassActorSpawnerSubsystem.cpp:571–595`; `MassSimulationSettings.h:23, 27` ✔ |
| PrePhysics Mass phase = `TG_PrePhysics` | `ME/Private/MassProcessingPhaseManager.cpp:46` ✔ |

**One switch, in time order (Actor → ISM, default settings, pooling on):**

```text
TG_PrePhysics, Mass PrePhysics phase
  1. OnPrePhysicsPhaseStarted: pending spawns (budgeted)                MassActorSpawnerSubsystem.cpp:571
  2. LOD processors: new LOD for the entity
  3. Representation.VisualizationProcessing: CurrentRepresentation = ISM, actor DISABLED (kept: same template)
                                                                        MassRepresentationProcessor.cpp:168-198, 331-350
  4. ── StateHandoff commit processor runs here (ExecuteAfter VisualizationProcessing) ──
  5. OnPrePhysicsPhaseFinished: queued releases pooled/destroyed (budgeted)   MassActorSpawnerSubsystem.cpp:585
Later frames: an actor kept disabled keeps ticking its components (Actor.cpp:1754-1760)
```

## 3. Hooks and Mass-native state (evidence)

| Fact | Evidence (5.8.4) |
|---|---|
| Translators are processors with required tags; direction enum incl. `InitializationOnly` | `MG/MassSpawner/Public/MassTranslator.h:12–21, 28–60` ✔ |
| Trait object initializers run `ActorToMass` on agent creation and `MassToActor` on puppet init; puppet unregistration removes fragments without copying back | `MG/MassActors/Private/MassAgentSubsystem.cpp:346–353, 394–399, 196–219` ✔ |
| "Entity detaching" notification (agent components only) | `MassAgentComponent.cpp:329–336`; `MassAgentSubsystem.cpp:439–441` |
| Pool callbacks `CanBePooled`, `PrepareForPooling`, `PrepareForGame` | `MG/MassActors/Public/MassActorPoolableInterface.h:22–29` ✔ |
| `AActor::OnEndPlay` delegate | `Source/Runtime/Engine/Classes/GameFramework/Actor.h:2339` ✔ |
| Instanced Actors per-component hooks; persistent fragments "not automatically persisted (yet)" | `IA/Public/InstancedActorsComponent.h:41, 48, 102, 138–149` ✔ |
| Mass StateTree can bind only to fragments, shared fragments and world subsystems | `MAI/MassAIBehavior/Private/MassStateTreeExecutionContext.cpp:39, 58, 77, 96` ✔ |

## 4. Framework, build and measurement (evidence)

| Fact | Evidence (5.8.4) |
|---|---|
| `MassEntity.uplugin` removed; core = `Source/Runtime/MassEntity` + `Source/Runtime/Mass/{MassCore, MassSignals, MassEngine, MassDeveloper}` | directory listing; `ME/MassEntity.Build.cs:19` |
| `MassGameplay` plugin: Experimental; 16 modules; depends on ZoneGraph, PoseSearch, ZoneGraphAnnotations, SmartObjects, StateTree, DataValidation | `Plugins/Runtime/MassGameplay/MassGameplay.uplugin:16, 18–84, 86–111` ✔ |
| `MassAI` (Experimental) holds the Mass StateTree integration, module `MassAIBehavior` | `Plugins/AI/MassAI/MassAI.uplugin:16, 37` ✔ |
| `InstancedActors`: Experimental | `Plugins/Runtime/InstancedActors/InstancedActors.uplugin:16` ✔ |
| `MassInsights`: Beta, enabled by default, Insights-program modules only | `Plugins/MassInsights/MassInsights.uplugin:10, 12, 14–31` ✔ |
| Launcher engine ships precompiled game-target objects for the Mass plugins | `…/MassGameplay/Intermediate/Build/Win64/x64/UnrealGame/Development/*/…precompiled` (listing); **confirmed by the Phase 4 package (`09` F-4)** |
| `UMassProcessor` defaults: PrePhysics, auto-registered, not game-thread, `Server \| Standalone` | `ME/Public/MassProcessor.h:244, 252, 258`; `ME/Private/MassProcessor.cpp:138` ✔ |
| Code-built entity configs: `FMassEntityConfig::AddTrait`, `GetOrCreateEntityTemplate`; `UMassSpawnerSubsystem::SpawnEntities` | `MG/MassSpawner/Public/MassEntityConfigAsset.h:37, 61`; `MassSpawnerSubsystem.h:33` |
| Template build context accepts fragments and tags by `UScriptStruct` (`AddFragment(FConstStructView)`, `AddTag(const UScriptStruct&)`), so a trait can add fragment types chosen by data | `MG/MassSpawner/Public/MassEntityTemplateRegistry.h:94–99, 109–120` |
| Fixed step and CSV capture flags (as in the portfolio protocol) | `LaunchEngineLoop.cpp:2456–2461`; `UnrealEngine.cpp:3015–3023`; `CsvProfiler.cpp:4309, 5078, 5081` ✔ |

## 5. Existing work, updated

| Solution | What it moves | When | Covers pooling / kept-disabled / deferred destruction? | Ownership stated? | Checked or measured? | Evidence |
|---|---|---|---|---|---|---|
| Engine translators | Spatial data only | Every frame | n/a | Implicit (direction flag) | No | B2 |
| Trait object initializers | Whatever the trait writes | Agent creation / puppet init (one-shot) | Agent-component actors only | No | No | B3–B4 |
| CitySample-style `OnPostActorSpawn` | Entity → actor at spawn | Spawn or pool retrieval | Not on re-enable of a kept actor | No | No | D3 |
| **Epic PersistenceLab** (UE 5.8, 2026) | GAS health, nav target, state id, per actor class | `BeginPlay` (with retry) / `EndPlay` | Not shown in the files read | A "hydrated" tag marks actor ownership | No ("not battle tested") | D2 |
| Instanced Actors component hooks | Per component, hand-written | Spawn of an instanced actor; save/load | Stationary instances | No | No | B4 |
| Fab: Scalable Modular Behaviour | "Zero-Stutter Mass/Actor Syncing" (which state is not stated) | not stated | not stated | not stated | Animation timings only | D4 |

## 6. The gap, stated conservatively (revised)

**What Phase 1 said:** health, AI memory, cooldowns and reservations are hand-written per project with nothing checking them (gap: strong).

**What Phase 2 found:**
- The hand-off **idea and hooks are not new.** The engine has hooks (finding 9), and Epic published a per-actor-class hydrate/dehydrate pattern for UE 5.8 (finding 10). We credit both and use the pattern as baseline B1.
- **Mass-native state is already safe** (finding 2), so the problem is limited to state owned by actor-side systems, which is still the common case for GAS- and actor-based AI (D1).
- **Among the public sources reviewed, none provides:**
  1. an explicit, per-field **ownership contract** whose hand-off is tied to the **representation commit** rather than to actor lifetime. That matters because Mass decouples the two (findings 3–5);
  2. a **generic** hand-off: declare a state field once, with no per-actor-class code;
  3. an **automated invariant checker** that classifies and counts divergence across thousands of transitions;
  4. a **controlled measurement** of the existing patterns: correctness, transition cost, flip spikes and memory.

**Gap strength after Phase 2:** **medium** for the hand-off mechanism itself (a careful developer can follow Epic's pattern); **strong** for the contract, the checker and the measurement. If the measurements show that the pool-aware lifecycle pattern (B1) never diverges, the project says so: its value is then the checker and the evidence, not a fix (SC3 in `01` §6.4).

Studio-internal systems aren't public, and a web search can't prove absence. This is a statement about what was reviewed, never "nobody has done this".

## 7. Hypothesis status after Phase 2

| # | Hypothesis | Status |
|---|---|---|
| H1 | Hand-off tied to actor lifetime runs at the wrong moment | **Supported by code** (queued, budgeted destruction; no `EndPlay` for pooled actors) **[inferred]** **[to measure]** |
| H2 | Kept-disabled actors keep a second live copy | **Supported by code** (only the actor tick is disabled) **[inferred]** **[to measure]** |
| H3 | Per-frame mirroring has a loss window and a per-frame cost that scales with actors, not transitions | **Supported by code** (translators are per-frame processors); one user report of about 3 ms per frame for a transform translator at about 500 entities (D1, their conditions) **[to measure]** |
| H4 | Pooled actors carry the previous entity's state | **Supported by code** (`PrepareForGame`, no `BeginPlay`) **[inferred]** **[to measure]** |
| H5 | LOD flips concentrate transitions in single frames | **Partly supported:** spawns are budgeted (1.5 ms), but disable/release decisions are made for every entity in the frame its LOD changes **[to measure]** |

## 8. Carried into Phase 3

- Commit-time hand-off in a game-thread processor that runs `ExecuteAfter UE::Mass::ProcessorGroupNames::VisualizationProcessing`, in the PrePhysics phase, before queued releases are processed.
- An `OnPreActorSpawn` override that captures before the immediate template-swap release, and an `AActor::OnEndPlay` safety net.
- Binding rule: an entity is bound to an actor only when `CurrentRepresentation` is an actor type, `FMassActorFragment` holds a valid actor, and the entity is not waiting for visual readiness.
- Nothing in `BeginPlay`; never trust `EndPlay` or pool callbacks as the hand-off point.
- The workload: `UMassMovableVisualizationTrait`, its own LOD counts, three small processor subclasses (visualization, visualization LOD, LOD collector) as in MassCrowd, entities built from a code-made `FMassEntityConfig`.
- Baselines: B0 stock, **B1 Epic's lifecycle pattern made pool-aware**, B2 per-frame mirror + push on spawn, B3 entity-owned (Epic's "keep the entity" advice).
- Event-driven checker with per-1,000-transition normalisation; pooling as a workload parameter; transition counts recorded per run.

**Raw research notes**, with every quote, are in [Phase2_Raw_Notes/](Phase2_Raw_Notes/README.md) as an audit trail. This file is authoritative: everything the design depends on is restated above with its evidence.
