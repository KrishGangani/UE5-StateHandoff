# A · Representation internals (UE 5.8.4)

Short forms as in [README](README.md): `MG/` = `Plugins/Runtime/MassGameplay/Source/`.

## A1. The representation types and parameters

| Fact | Evidence |
|---|---|
| `EMassRepresentationType` = `HighResSpawnedActor, LowResSpawnedActor, SkinnedMeshInstance, StaticMeshInstance, None`. `SkinnedMeshInstance` is the 5.8 skinned-instance path (`MassUpdateSkinnedMeshProcessor`) | `MG/MassRepresentation/Public/MassRepresentationTypes.h:35–43` |
| Processor groups `Representation` and its sub-group `Representation.VisualizationProcessing`. Header comment: "Consumers that depend on CurrentRepresentation being finalised should ExecuteAfter this." | `MassRepresentationTypes.h:25–31` |
| Per-entity state: `CurrentRepresentation`, `PrevRepresentation`, the high/low template indices, `ActorSpawnRequestHandle`, and a private `bIsWaitingForActorVisualReadiness` with a public getter | `MG/MassRepresentation/Public/MassRepresentationFragments.h:51–130` (fields :59, :62, :65, :68, :92; getter :109) |
| `FMassRepresentationParameters` (const shared fragment): `RepresentationActorManagementClass` (:168–169); default `LODRepresentation = { HighResSpawnedActor, LowResSpawnedActor, StaticMeshInstance, None }` (:173); `bForceActorRepresentationForExternalActors` (:194); **`bKeepLowResActors = true`** (:198) "LowRes actors will be kept around, disabled, whilst StaticMeshInstance representation is active"; `bKeepActorExtraFrame = false` (:202); `bWaitForActorVisualReadiness = false` (:211); `bSpreadFirstVisualizationUpdate` (:215) | `MassRepresentationFragments.h:160–253` |
| If no management class is set, the base class CDO is used | `MassRepresentationFragments.h:290–295` |
| `FMassVisualizationLODParameters`: `BaseLODDistance = {0, 1000, 2500, 10000}` (:305), `VisibleLODDistance = {0, 2000, 4000, 15000}` (:307), `BufferHysteresisOnDistancePercentage = 10` (:309), **`LODMaxCount = {50, 100, 500, MAX_int32}`** (:313) | `MassRepresentationFragments.h:298–330` |

**Note:** with the default `LODMaxCount`, at most 50 entities are High LOD and 100 Medium LOD at once, so at most about 150 actors exist however large N is. The workload must set its own counts (Phase 5 calibration) to get enough actor transitions per run.

## A2. Actor management: the class games can subclass

`MG/MassRepresentation/Public/MassRepresentationActorManagement.h`

| Member | Line | Virtual? |
|---|---|---|
| `GetSpawnPriority` | :33 | yes |
| `GetOrSpawnActor` | :47–49 | yes |
| `SetActorEnabled` | :58 | yes |
| `TeleportActor` | :67 | yes |
| `IsActorReadyForRepresentation` (comment :69–93 describes the readiness wait and the frame cap) | :94 | yes |
| `OnPreActorSpawn` | :102 | yes |
| `OnPostActorSpawn` | :111 | yes |
| `ReleaseAnyActorOrCancelAnySpawning` (two overloads) | :122, :136–137 | **no: `static`** |

`MG/MassRepresentation/Private/MassRepresentationActorManagement.cpp`

| Behaviour | Lines |
|---|---|
| `SetActorEnabled` toggles only the **actor** tick and (deferred) collision | :30–45 |
| `IsActorReadyForRepresentation` returns `true` | :58–61 |
| `OnPreActorSpawn` **releases any existing actor immediately** (`bImmediate = true`) before the new one spawns. Comment: reset first, because the release "might move the entity to a new archetype" | :63–90 (release :82–88) |
| `OnPostActorSpawn` stores the spawned actor in `FMassActorFragment` and updates the actor→entity map (`SetAndUpdateHandleMap`, :108) | :92–117 |
| `ReleaseAnyActorOrCancelAnySpawning` resets `FMassActorFragment` (:140) **before** releasing the actor (:143–150) | :129–152 |

`Source/Runtime/Engine/Private/Actor.cpp:1754–1760`: `AActor::SetActorTickEnabled` only calls `PrimaryActorTick.SetTickFunctionEnable`. **Component ticks are untouched**, so a "disabled" Mass actor's components keep ticking.

## A3. The representation processor (one entity, one frame)

`MG/MassRepresentation/Private/MassRepresentationProcessor.cpp`

| Step | Lines |
|---|---|
| cvars: `ai.massrepresentation.AllowKeepActorExtraFrame` (1), `ai.massrepresentation.AllowWaitForActorVisualReadiness` (1), `ai.massrepresentation.MaxActorVisualReadinessWaitFrames` (255) | :26–45 |
| `UMassRepresentationProcessor` is abstract-ish: `bAutoRegisterWithProcessingPhases = false`, group `Representation`, after `LOD` | :51–58 |
| `PrevRepresentation = CurrentRepresentation` unless waiting for readiness | :117–122 |
| Wanted type = `LODRepresentation[LOD]` | :125 |
| No actor where collision isn't loaded (`IsCollisionLoaded`) → default representation | :130–135 |
| Actor unexpectedly destroyed while the representation says actor → `CurrentRepresentation = None`, so a new actor spawns later | :157–165 |
| `DisableActorForISM`: disable the actor; release the high-res actor only if high ≠ low template **or** `!bKeepLowResActors`; release the low-res one only if `!bKeepLowResActors` | :168–198 |
| Actor wanted: reuse the current actor if it matches the template and no spawn is pending, else `GetOrSpawnActor` | :212–264 |
| Teleport the (re)activated actor to the entity transform, then enable it, **or defer** enabling while `IsActorReadyForRepresentation` is false or the one-frame readiness delay applies | :266–305 |
| Async spawn pending and no actor → fall back to the default representation | :306–315 |
| ISM wanted: `DisableActorForISM` immediately, or one frame later with `bKeepActorExtraFrame` | :331–350, :443–449 |
| `None` wanted: release both templates (queued, not immediate) | :367–380 |
| Waiting for readiness: re-poll each frame, force-enable at the frame cap with a warning | :386–442 |
| `ReleaseActorOrCancelSpawning` calls `ReleaseTemplateActorOrCancelSpawning` **without** `bImmediate` (default false), then resets the fragment and **defers** `RemoveHandleForActor` through the command buffer | :481–516 (call :495–496; defer :502–511) |
| `UMassVisualizationProcessor` runs in `Representation.VisualizationProcessing` | :521–524 |
| Culled-by-distance chunks update only every `NotVisibleUpdateRate` (0.5 s); the first-update spread uses `FMath::RandRange` unless `IsDeterministic()` | :556–599 (random :583, :590) |
| `UMassRepresentationFragmentDestructor` (observer on fragment removal) releases actors with `bImmediate = true` when the entity is destroyed | :632–671 |

## A4. From "release" to the actor actually going away

`MG/MassRepresentation/Private/MassRepresentationSubsystem.cpp:300–366`: `ReleaseTemplateActorOrCancelSpawning` → `ReleaseTemplateActorInternal` → `ActorSpawnerSubsystem->DestroyActor(Actor, bImmediate)` (:365).

`MG/MassActors/Private/MassActorSpawnerSubsystem.cpp`

| Behaviour | Lines |
|---|---|
| Pooling cvar `mass.spawning.actorpooling` (alias `ai.mass.actorpooling`, deprecated), default **true** | :22–34 |
| `DestroyActor`: unregister the actor's `UMassAgentComponent` **immediately**; then, if immediate, pool or destroy now; otherwise **queue** in `ActorsToDestroy` | :90–117 |
| `ReleaseActorToPool`: only if pooling is on and the actor implements `IMassActorPoolableInterface` and `CanBePooled()`. It calls `PrepareForPooling` and `SetActorHiddenInGame(true)`. **No `EndPlay`, no tick or collision change here** | :119–140 (:127, :132–133) |
| `SpawnOrRetrieveFromPool`: a pooled actor is un-hidden, moved, `PrepareForGame` is called; **no `BeginPlay`** | :235–266 (:252–255) |
| `SpawnActor` uses `World->SpawnActor` (not deferred), so the actor's `BeginPlay` runs **inside** the spawn, before the post-spawn delegate | :314–334 |
| Order inside one request: pre-spawn delegate (:444–447) → spawn or pool (:449) → post-spawn delegate (:480–482) | :432–498 |
| Spawning loop is **time-sliced on wall-clock time** (`FPlatformTime::Seconds() < TimeSliceEnd`) | :357–371 |
| Destruction loop is time-sliced too (except on servers). Actors that miss the budget are **deactivated** (collision off, hidden, tick off) and destroyed on a later frame | :500–544 (:509, :515, :527–543) |
| Spawning runs at the **start** of the PrePhysics Mass phase, destruction at its **end**. Comments: the representation processor "relies on actor spawns being processed before it runs" and on "actor destruction processing after it runs" | :571–595 |
| CSV stats `MassActors/NumSpawned`, `MassActors/NumPooled` | :20, :581–582 |

`MG/MassActors/Public/MassActorSpawnerSubsystem.h`: `ReleaseActorToPool` is `virtual` (:302); `bActorPoolingEnabled = true` (:319); `SpawnOrRetrieveFromPool`/`SpawnActor` are virtual (:272, :281). `MG/MassActors/Public/MassActorPoolableInterface.h:22–29`: `CanBePooled`, `PrepareForPooling`, `PrepareForGame` (BlueprintNativeEvent).

`MG/MassSimulation/Public/MassSimulationSettings.h`: `DesiredActorSpawningTimeSlicePerTick = 0.0015` s (:23) and `DesiredActorDestructionTimeSlicePerTick = 0.0005` s (:27), in `config = Mass` (:15). Entity compaction is also wall-clock budgeted (5 ms, :35–40).

## A5. What these facts imply (reasoning, not yet measured)

1. **Actor lifetime ≠ representation.** The representation commits inside the processor (A3), but the actor is pooled or destroyed at the end of the phase or, past the destruction budget, on a later frame (A4). Ownership tied to `BeginPlay`/`EndPlay` is therefore tied to the wrong clock **[inferred]**.
2. **Pooling skips `BeginPlay` and `EndPlay`.** Hand-off code that lives only in those two functions never runs for pooled actors (A4) **[inferred from code]**.
3. **A "disabled" actor still exists, and its components still tick** (A2, `Actor.cpp:1754–1760`), so it can keep changing its own copy of state while the entity is on ISM **[inferred]**.
4. **Three different release paths** exist: queued (processor, A3 :495), immediate before a template swap (`OnPreActorSpawn`, A2), immediate on entity destruction (fragment destructor, A3 :668). A hand-off must cover all three.
5. **`ReleaseAnyActorOrCancelAnySpawning` is static.** A game can't intercept "release" by subclassing; it can intercept spawn (pre/post), enable/disable and readiness.
6. **Spawn timing is not deterministic across runs** even with `-deterministic`, because the spawn and destroy budgets are wall-clock slices (A4). Transition counts must be measured per run, and correctness must be judged per transition, not by frame number.
