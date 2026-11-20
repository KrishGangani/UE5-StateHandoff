# C · Framework layout and measurement tools (UE 5.8.4)

Short forms as in [README](README.md).

## C1. Plugins and modules on disk

| Item | Status in 5.8.4 | Evidence |
|---|---|---|
| `MassEntity.uplugin` | **Gone.** No such file under `Engine/Plugins` (search). The core is the engine module `Source/Runtime/MassEntity` plus `Source/Runtime/Mass/{MassCore, MassSignals, MassEngine, MassDeveloper}` | directory listing; `ME/MassEntity.Build.cs:13–19` depends on `MassCore` |
| `MassGameplay` plugin | `IsExperimentalVersion: true` (:16), `EnabledByDefault: false` (:13). Modules: MassCommon, MassActors, MassEQS, MassSpawner, MassSmartObjects, MassSimulation, MassLOD, MassMovement, MassCharacterTrajectory, MassReplication, MassRepresentation, MassGameplayDebug (Runtime); MassGameplayEditor, MassMovementEditor (Editor); MassGameplayExternalTraits; MassGameplayTestSuite (UncookedOnly) (:18–84). Plugin dependencies: ZoneGraph, **PoseSearch**, ZoneGraphAnnotations, SmartObjects, StateTree, DataValidation (:86–111) | `Plugins/Runtime/MassGameplay/MassGameplay.uplugin` |
| `MassAI` plugin | Experimental (:16). Modules include `MassAIBehavior` (the Mass StateTree integration, :37), MassNavigation, MassZoneGraphNavigation, MassAIReplication | `Plugins/AI/MassAI/MassAI.uplugin` |
| `InstancedActors` plugin | Experimental (:16); depends on MassGameplay, DataRegistry, GameFeatures (:32–45) | `Plugins/Runtime/InstancedActors/InstancedActors.uplugin` |
| `MassInsights` plugin | `IsBetaVersion: true` (:12), `EnabledByDefault: true` (:10). Its two modules are `EditorAndProgram`, allow-listed for the **UnrealInsights** program (:14–31). It is an Insights analysis/UI plugin, not something the game loads | `Plugins/MassInsights/MassInsights.uplugin` |
| `StateTree` plugin | No Beta/Experimental flag (`IsBetaVersion: false`, :15) | `Plugins/Runtime/StateTree/StateTree.uplugin` |
| Prebuilt game-target objects | e.g. `Plugins/Runtime/MassGameplay/Intermediate/Build/Win64/x64/UnrealGame/Development/MassRepresentation/MassRepresentation.precompiled`; also present for InstancedActors and MassAI | directory listing. **Confirmed by the Phase 4 package** (`09`, F-4) |

## C2. Processors, phases, registration

| Fact | Evidence |
|---|---|
| `UMassProcessor` defaults: `ProcessingPhase = PrePhysics` (:244), `bAutoRegisterWithProcessingPhases = true` (:252), `bRequiresGameThreadExecution = false` (:258); `ExecutionFlags = Server \| Standalone` | `ME/Public/MassProcessor.h`; `ME/Private/MassProcessor.cpp:138` |
| `UMassProcessor` is `config = Mass, defaultconfig`, so processor flags can also be set in `DefaultMass.ini` | `MassProcessor.h:78` |
| Phase enum `PrePhysics, StartPhysics, DuringPhysics, EndPhysics, PostPhysics, FrameEnd` | `ME/Public/MassProcessingTypes.h:173–182` |
| The PrePhysics Mass phase runs in **`TG_PrePhysics`** | `ME/Private/MassProcessingPhaseManager.cpp:46` |
| Group names: `UpdateWorldFromMass, SyncWorldToMass, Behavior, Tasks, Avoidance, ApplyForces, Movement` | `MG/MassCommon/Public/MassCommonTypes.h:18–27` |
| `LODCollector`, `LOD` | `MG/MassLOD/Public/MassLODTypes.h:31–32` |
| The visualization and visualization-LOD processors are **not auto-registered** (`MassRepresentationProcessor.cpp:54`, `MassVisualizationLODProcessor.cpp:13`), nor is the LOD collector (`MG/MassLOD/Private/MassLODCollectorProcessor.cpp:14`). A game subclasses them with a tag filter and sets `bAutoRegisterWithProcessingPhases = true`, as MassCrowd does | `Plugins/AI/MassCrowd/Source/MassCrowd/Private/MassCrowdVisualizationProcessor.cpp:20–35`; `MassCrowdVisualizationLODProcessor.cpp:35–55`; collector subclass `Public/MassCrowdVisualizationLODProcessor.h:30` |
| `UMassVisualizationTrait` is **soft-deprecated** in 5.8: "Use MassStationaryVisualizationTrait or MassMovableVisualizationTrait" | `MG/MassRepresentation/Public/MassVisualizationTrait.h:18–19`; `UMassMovableVisualizationTrait` sets meshes `Movable` then calls the base (`Private/MassMovableVisualizationTrait.cpp:9–23`) |
| The trait sets the default actor-management class (`MassVisualizationTrait.cpp:30`), logs an error if none (:86–89) and registers both actor templates; the **same class for high and low res gives the same template index** (:94–95, `FindOrAddTemplateActor`) | `MG/MassRepresentation/Private/MassVisualizationTrait.cpp` |
| The LOD subsystem gathers viewers from player controllers | `MG/MassLOD/Private/MassLODSubsystem.cpp:270–283` |
| Entity configs can be built in code: `FMassEntityConfig::AddTrait` (:61), `GetOrCreateEntityTemplate` (:37; implementation `.cpp:66–93`); `UMassEntityConfigAsset::AddTrait(TSubclassOf)` (:159) | `MG/MassSpawner/Public/MassEntityConfigAsset.h`; `Private/MassEntityConfigAsset.cpp` |
| `UMassSpawnerSubsystem::SpawnEntities(Template, Count, OutEntities)` | `MG/MassSpawner/Public/MassSpawnerSubsystem.h:33` |
| Template build context: `AddFragment(FConstStructView)` (:94–99), `AddTag(UScriptStruct)` (:109–120), `AddConstSharedFragment` (:129), `HasFragment(UScriptStruct)` (:184) | `MG/MassSpawner/Public/MassEntityTemplateRegistry.h` |
| Mass determinism switch: `UE::Mass::Utils::IsDeterministic()` returns `FApp::bUseFixedSeed` | `MG/MassCommon/Private/MassCommonUtils.cpp:38–41` |

## C3. Fixed step and CSV capture (re-checked here for this project)

| Fact | Evidence |
|---|---|
| `-deterministic` = `-UseFixedTimeStep` + `-FixedSeed` (so Mass's `IsDeterministic()` is true too) | `Source/Runtime/Launch/Private/LaunchEngineLoop.cpp:2456–2461` |
| Fixed step: delta = fixed value, no waiting | `Source/Runtime/Engine/Private/UnrealEngine.cpp:3015–3023` |
| `-csvStartOnEvent=`, `-csvCaptureOnEventFrameCount=`, `-ExitAfterCsvProfiling` | `Source/Runtime/Core/Private/ProfilingDebugging/CsvProfiler.cpp:5081, 5078, 4309` |
| **Not deterministic under `-deterministic`:** Mass actor spawning, actor destruction and entity compaction are wall-clock time-sliced | [A](A_representation_internals.md) A4 |

## C4. Tracing, MassInsights, debug commands, memory

| Fact | Evidence |
|---|---|
| Trace channel `MassChannel`: phase begin/end, archetype and fragment registration, chunk execution, bulk add/destroy, **`MassEntityMoved` (entity, new archetype)**, query events | `ME/Private/MassEntityTrace.cpp:22–118` (`MassEntityMoved` :84–89) |
| Trace compiled in only if `UE_TRACE_ENABLED && WITH_MASSENTITY_DEBUG && !UE_BUILD_SHIPPING` | `ME/Public/MassEntityTrace.h:10–12` |
| `WITH_MASSENTITY_DEBUG` is off in **Test and Shipping**, on in Development | `Source/Runtime/Mass/MassCore/Public/Mass/EntityMacros.h:8` |
| So MassInsights shows **when processors run and entities move archetypes**. It has no notion of gameplay state or ownership | the event list above |
| Debug commands: `mass.LogArchetypes` (archetype dump), `mass.LogFragmentSizes`, `mass.LogMemoryUsage` | `ME/Private/MassDebugger.cpp:312, 366, 381` (inside `#if WITH_MASSENTITY_DEBUG`, :7) |
| The archetype dump prints **"Bytes / Entity"**, chunk count and size, entities per chunk, and each fragment's offset and size | `ME/Private/MassArchetypeData.cpp:1360–1424` (:1404) |
| `obj list class=<Class>` prints Count, NumKB, MaxKB, ResExcKB… per class: used for actor and component memory | `Source/Runtime/Engine/Private/UnrealEngine.cpp:9607, 10004` |
| Mass CSV categories: `MassEntities`, `MassEntitiesCounters` (`ME/Private/MassCommandBuffer.cpp:12–13`), `MassSignalsCounters` (`Source/Runtime/Mass/MassSignals/Private/MassSignalSubsystem.cpp:14`), `MassActors` (spawned/pooled counts, `MassActorSpawnerSubsystem.cpp:20, 581–582`), `MassStateTreeProcessor` (off by default, `MassStateTreeProcessors.cpp:26`) | as listed |

## C5. Replication and Iris (scope check: single-player only)

| Fact | Evidence |
|---|---|
| The `MassReplication` module contains **no Iris code** (case-insensitive search for "iris" in its folder is empty). It depends on `NetCore`, not on Iris | `MG/MassReplication/MassReplication.Build.cs:13–29` |
| Entity replication goes through client "bubble" actors (`AMassClientBubbleInfoBase : AInfo`) holding a `FFastArraySerializer` | `MG/MassReplication/Public/MassClientBubbleInfoBase.h:16`; `MassClientBubbleSerializerBase.h:23` |
| Iris appears only at the *actor* level: `AInstancedActorsManager::OnReplicationStartedForIris` (an ordinary replicated actor) and `SetupIrisSupport` in the InstancedActors and MassCrowd build files | `IA/Public/InstancedActorsManager.h:265`; `IA/InstancedActors.Build.cs:49`; `Plugins/AI/MassCrowd/Source/MassCrowd/MassCrowd.Build.cs:40` |
| Epic (forum, Jun 1 2026): Mass replication improvements "unlikely to land in time for 5.8" | [research/03](../Research/03_existing_solutions_gap_analysis.md) §3.4 |

**Verdict:** there is no Iris path for Mass *entity* replication in 5.8.4. The project stays single-player.
