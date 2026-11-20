# 06 · Unreal APIs And Macros: StateHandoff

**Rule:**
- Only APIs this project **actually uses** appear here, each in the same step it enters the code.
- Each entry follows the 8-point format: what it is · why it exists · how it works · when to use it · when not to · common mistakes · a small example · how we use it.
- Verified against UE **5.8.4** source; file references are relative to `Engine/`.

| # | API | First used in |
|---|---|---|
| 1 | `IMPLEMENT_PRIMARY_GAME_MODULE` / `IMPLEMENT_MODULE`, `FDefaultGameModuleImpl` / `FDefaultModuleImpl` | Phase 4 |
| 2 | `ModuleRules`: dependency lists and `PCHUsage` | Phase 4 |
| 3 | `TargetRules`: `Type`, `DefaultBuildSettings`, `IncludeOrderVersion` | Phase 4 |
| 4 | Descriptors: `LoadingPhase`, and plugin dependencies (`"Plugins"` in `.uproject` / `.uplugin`) | Phase 4 |
| 5 | Reflection basics: `UCLASS`, `USTRUCT`, `UPROPERTY`, `GENERATED_BODY`, `TObjectPtr` | S1 |
| 6 | Mass fragments and tags | S1 |
| 7 | Traits and template building | S1 |
| 8 | Building and spawning entities in code | S1 |
| 9 | Mass processors and queries | S1 |
| 10 | Representation LOD settings | S1 |
| 11 | `UTickableWorldSubsystem`, `DoesSupportWorldType` | S1 |
| 12 | CSV profiler macros | S1 |
| 13 | `CreateDefaultSubobject`, `ConstructorHelpers::FObjectFinder`, `LoadObject`, `NewObject` | S1 |
| 14 | `FParse`, `FRandomStream`, `FCrc` | S1 |

Mass APIs are added here as they enter the code (S1: fragments, traits, spawning, processors, representation LOD settings). Their verified behaviour is already in [01b_Research_Findings.md](01b_Research_Findings.md).

---

## 1. `IMPLEMENT_PRIMARY_GAME_MODULE` / `IMPLEMENT_MODULE`

1. **What it is.** Macros that generate a module's entry point (`Source/Runtime/Core/Public/Modules/ModuleManager.h:946–955` for `IMPLEMENT_MODULE`; `:1100–1136` for the primary game variant).
2. **Why it exists.** The engine loads code as **modules**: DLLs in editor builds, static code in packaged builds. Each module needs a standard way to be found and started, so the module manager can call its `StartupModule()`/`ShutdownModule()`.
3. **How it works.**
   - In a modular build it exports an `InitializeModule` function, which the module manager calls after loading the DLL; it returns an instance of the class you name.
   - In a monolithic build it registers a static initializer instead.
   - The *primary* variant also sets up project-wide globals for packaged builds and key and target-name registration.
4. **When to use it.** Exactly one `IMPLEMENT_MODULE` (or the game variant) per module; at least one `IMPLEMENT_PRIMARY_GAME_MODULE` per project.
5. **When not to.** Not twice in one module (duplicate-symbol link errors). A plugin module uses `IMPLEMENT_MODULE`, never the primary game variant.
6. **Common mistakes.**
   - The module name argument doesn't match `Build.cs` and the `.uproject` ("module not found" at start-up).
   - Writing an empty custom `IModuleInterface` class when `FDefaultModuleImpl` would do.
   - Thinking the third argument still sets the game name. In 5.8 it is ignored (`DEPRECATED_GameName`, `ModuleManager.h:1096, 1100`).
7. **Example.**
   ```cpp
   #include "Modules/ModuleManager.h"
   IMPLEMENT_MODULE(FDefaultModuleImpl, MyPluginModule);
   ```
8. **In this project.** `StateHandoffModule.cpp` uses the primary variant with `FDefaultGameModuleImpl`; `StateHandoffCoreModule.cpp` uses `IMPLEMENT_MODULE(FDefaultModuleImpl, StateHandoffCore)`. Neither module needs start-up logic: Mass discovers processors and traits through reflection, not through module start-up.

## 2. `ModuleRules`: dependencies and `PCHUsage`

1. **What it is.** The C# class in `<Module>.Build.cs` that tells UnrealBuildTool how to compile one module.
2. **Why it exists.** Unreal is hundreds of modules. Each declares what it depends on, so UBT can work out include paths, link order and what to rebuild.
3. **How it works.**
   - `PublicDependencyModuleNames`: their include paths are given to us *and* to anyone who depends on us.
   - `PrivateDependencyModuleNames`: only to us.
   - Both link the dependency.
   - `PCHUsage = UseExplicitOrSharedPCHs` uses a shared precompiled header built for a common dependency set, unless the module names its own.
4. **When to use which.** Public **only** if one of *your public headers* includes that module's headers. Everything else is private.
5. **When not to.** Don't add modules "just in case": each costs build time and couples modules.
6. **Common mistakes.**
   - Making everything public, which spreads include paths and widens rebuilds.
   - A missing dependency: `cannot open include file` or `unresolved external symbol`.
   - Confusing a **plugin** dependency (in a descriptor, §4) with a **module** dependency (here). Enabling MassGameplay doesn't give a module the Mass include paths; listing `MassRepresentation` in `Build.cs` does.
7. **Example.**
   ```csharp
   PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
   PrivateDependencyModuleNames.Add("MassRepresentation");
   ```
8. **In this project.** Both modules depend only on `Core`, `CoreUObject` and `Engine` in Phase 4. The Mass modules (`MassEntity`, `MassCore`, `MassSpawner`, `MassActors`, `MassRepresentation`; the game also `MassLOD`, `MassCommon`) are added with the first code that includes their headers (`02` §3).

## 3. `TargetRules`: `Type`, `DefaultBuildSettings`, `IncludeOrderVersion`

1. **What it is.** The C# class in `<Name>.Target.cs` that describes one executable: game, editor, client, server or program.
2. **Why it exists.** The same modules build into different executables, each with different settings and module sets.
3. **How it works.** UBT finds the class `<Name>Target` and builds its `ExtraModuleNames` plus the engine and the enabled plugins. `DefaultBuildSettings` selects a versioned set of default build options; `IncludeOrderVersion` selects which engine version's header include order to use.
4. **When to use it.** One file per executable type. Here: `StateHandoff` (game) and `StateHandoffEditor` (editor).
5. **When not to.** No `Client`/`Server` targets: the project is single-player (`01b #12`).
6. **Common mistakes.**
   - The class name doesn't follow `<Name>Target`.
   - Leaving an old `IncludeOrderVersion` after an engine upgrade, which hides missing-include bugs until later.
   - Expecting `Test` to build on a launcher engine. It doesn't (established in UE5-UpdateBudget's Phase 4), so headline builds are packaged Development.
7. **Example.** See `Source\StateHandoff.Target.cs` (`05` A2).
8. **In this project.** `V7` and `Unreal5_8`, the values in Epic's 5.8 template (`Templates/TP_Blank/Source/TP_Blank.Target.cs:11–12`, relative to the engine install root).

## 4. Descriptors: `LoadingPhase` and plugin dependencies

1. **What it is.** Fields of the JSON descriptors: each module's `LoadingPhase` in `.uproject`/`.uplugin`, and the `"Plugins"` array, which enables other plugins.
2. **Why it exists.** Some modules must load before others (rendering, config), and a plugin can be useless without another one.
3. **How it works.**
   - `LoadingPhase` is one of `ELoadingPhase` (`Source/Runtime/Projects/Public/ModuleDescriptor.h:24–60`); `Default` (:47) loads after engine start-up and before the first world.
   - The `"Plugins"` array is read into plugin references (`Source/Runtime/Projects/Private/PluginDescriptor.cpp:391`); the plugin manager enables each referenced plugin along with the one that names it.
   - A project plugin with `EnabledByDefault: true` loads even if the `.uproject` doesn't list it (`PluginManager.cpp:421–425`).
4. **When to use it.** `Default` for gameplay modules. A plugin dependency when your plugin can't work without the other one.
5. **When not to.** `PreDefault`/`PostConfigInit` only for modules other modules need while loading; `PostEngineInit` only for things that need the whole engine first (MassGameplay uses it for its movement editor module, `MassGameplay.uplugin:74`).
6. **Common mistakes.**
   - Forgetting to enable an **Experimental, off-by-default** plugin: MassGameplay is `EnabledByDefault: false` (`MassGameplay.uplugin:13`), so a module that uses Mass types builds only if the plugin is enabled.
   - Assuming a plugin dependency also gives include paths (it doesn't; see §2).
   - Looking for `MassEntity.uplugin`: in 5.8 MassEntity is an engine module, not a plugin (`01b` §4).
7. **Example.**
   ```json
   "Plugins": [ { "Name": "MassGameplay", "Enabled": true } ]
   ```
8. **In this project.** Both modules use `Default`. The `.uproject` enables `StateHandoffCore` and `MassGameplay`; `StateHandoffCore.uplugin` itself depends on `MassGameplay`, so any project using the plugin gets Mass.

## 5. Reflection basics: `UCLASS`, `USTRUCT`, `UPROPERTY`, `GENERATED_BODY`, `TObjectPtr`

1. **What it is.** Macros read by the Unreal Header Tool (UHT), which generates the `*.generated.h` code that registers types and members with the reflection system.
2. **Why it exists.** C++ has no run-time type information rich enough for serialisation, the editor, Blueprint or garbage collection. Reflection gives every class a `UClass` and every struct a `UScriptStruct`.
3. **How it works.** UHT parses the header; `GENERATED_BODY()` pastes the generated members in; `UPROPERTY` members become `FProperty` objects the garbage collector and serialiser walk. `TObjectPtr<T>` is the 5.x member-pointer type for UObjects (a raw pointer in packaged builds, with access tracking in the editor).
4. **When to use it.** Any `UObject` class (`UCLASS`), any struct the engine must know about (`USTRUCT`: Mass needs it to know a fragment's size and type), and every `UObject` pointer member (`UPROPERTY`), or the garbage collector can't see the reference.
5. **When not to.** Plain data used only by your own C++ (the director's `FRunSettings`, its `Agents` array of handles) needs no reflection.
6. **Common mistakes.** A `UObject*` member without `UPROPERTY` (it can be collected while still in use); forgetting that the `.generated.h` include must come last; adding specifiers "just in case" (`EditAnywhere`, `BlueprintReadWrite`) that expose internals.
7. **Example.**
   ```cpp
   UPROPERTY(Transient) TObjectPtr<UMassEntityConfigAsset> EntityConfig;   // kept alive, never saved
   ```
8. **In this project.** `FSHAgentIdFragment` and `FSHDemoAgentTag` are `USTRUCT`s (Mass requires it). The director's `EntityConfig` and `Camera` are `UPROPERTY(Transient)`: kept alive, never saved. The agent's mesh is `UPROPERTY(VisibleAnywhere)`: visible in the details panel, not replaceable.

## 6. Mass fragments and tags (`FMassFragment`, `FMassTag`)

1. **What it is.** A fragment is a plain struct of data per entity; a tag is a struct with no data whose presence marks an entity. Base types in MassCore since 5.8 (`Source/Runtime/Mass/MassCore/Public/Mass/EntityElementTypes.h:15, 40`); include `Mass/EntityElementTypes.h`, not the deprecated `MassEntityElementTypes.h` (`Source/Runtime/MassEntity/Public/MassEntityElementTypes.h:5–6`).
2. **Why it exists.** Mass stores entities by **archetype** (the exact set of fragment and tag types) in chunks of struct arrays, so a processor walks contiguous memory of exactly the data it needs.
3. **How it works.** Each archetype has fixed-size chunks; each fragment type is an array inside a chunk. Tags are bits in the archetype's composition, so they cost no memory per entity. Adding or removing a fragment or tag **moves the entity** to another archetype.
4. **When to use it.** Data a processor reads or writes per entity (fragment); a filter for queries (tag).
5. **When not to.** Data shared by many entities (use a shared or const shared fragment); anything with a constructor, destructor or heap memory: fragments must be trivially copyable unless you opt out (`EntityElementTypes.h:21`).
6. **Common mistakes.** Keeping a pointer or reference to a fragment across something that can move the entity (it dangles; engine warning `MassRepresentationActorManagement.h:115–116`); putting a `TArray` in a hot fragment.
7. **Example.**
   ```cpp
   USTRUCT() struct FHealth : public FMassFragment { GENERATED_BODY() UPROPERTY() float Value = 100.f; };
   USTRUCT() struct FEnemyTag : public FMassTag { GENERATED_BODY() };
   ```
8. **In this project.** `FSHAgentIdFragment` (the agent's index) and `FSHDemoAgentTag` (the demo filter). S2 adds the four state channels as fragments.

## 7. Traits and template building (`UMassEntityTraitBase`, `FMassEntityTemplateBuildContext`)

1. **What it is.** A trait is a `UObject` recipe that adds fragments, tags and shared fragments to an entity template (`MG/MassSpawner/Public/MassEntityTraitBase.h:18, 51`).
2. **Why it exists.** Entity types are composed from reusable pieces (visualization, LOD, movement) instead of written by hand.
3. **How it works.** An entity config lists traits; building the template calls each trait's `BuildTemplate(BuildContext, World)`, which calls `AddFragment<T>()`, `AddTag<T>()`, `RequireFragment<T>()` (a dependency another trait must satisfy, checked in validation) or adds shared fragments. Also available by `UScriptStruct`, so a trait can add types chosen by data (`MG/MassSpawner/Public/MassEntityTemplateRegistry.h:94–120`).
4. **When to use it.** Whenever a group of fragments belongs together for a feature.
5. **When not to.** For per-entity values (traits build templates once per entity type; set per-entity values after spawning).
6. **Common mistakes.** Requiring a fragment that no trait adds (validation fails: the visualization trait only *requires* the transform and actor fragments, `MassVisualizationTrait.cpp:67–69`); side effects in `BuildTemplate` when `BuildContext.IsInspectingData()` is true (editor inspection).
7. **Example.** `BuildContext.AddFragment<FTransformFragment>(); BuildContext.AddTag<FSHDemoAgentTag>();`
8. **In this project.** `USHDemoStateTrait` adds the transform, actor fragment, agent index and tag; the director also uses the engine's `UMassLODCollectorTrait` and `UMassMovableVisualizationTrait`.

## 8. Building and spawning entities in code (`UMassEntityConfigAsset`, `FMassEntityConfig`, `UMassSpawnerSubsystem`, `FMassEntityManager`)

1. **What it is.** A config asset holds a `FMassEntityConfig` (a list of traits); the spawner subsystem creates entities from the template the config builds; the entity manager owns all entity data.
2. **Why it exists.** Usually configs are data assets made in the editor. This project has no content, so the config is built in code.
3. **How it works.** `FMassEntityConfig::AddTrait(UMassEntityTraitBase&)` (`MG/MassSpawner/Private/MassEntityConfigAsset.cpp:203–206`) appends a trait. `GetOrCreateEntityTemplate(World)` builds and caches the template (`:66–93`). `SpawnEntities(Template, Count, OutHandles)` creates the entities in one batch and returns a creation context (`MassSpawnerSubsystem.h:33`). `FMassEntityManager::GetFragmentDataChecked<T>(Handle)` / `GetFragmentDataPtr<T>(Handle)` read or write one entity's fragment (`Source/Runtime/MassEntity/Public/MassEntityManager.h:768–778`).
4. **When to use it.** Procedural or code-driven workloads; tests.
5. **When not to.** Writing fragments through the entity manager inside a processor's parallel work (use the processor's query views instead).
6. **Common mistakes.** Calling the editor-only `UMassEntityConfigAsset::AddTrait(TSubclassOf)` from game code (it is inside `#if WITH_EDITOR`, `MassEntityConfigAsset.cpp:241–284`); releasing the creation context before the fragments are filled; letting the config be garbage-collected while its template is in use.
7. **Example.**
   ```cpp
   auto Context = Spawner->SpawnEntities(Template, 100, Handles);
   for (FMassEntityHandle H : Handles) { EntityManager.GetFragmentDataChecked<FTransformFragment>(H).SetTransform(T); }
   Context.Reset();   // creation notifications run now, with the real transforms
   ```
8. **In this project.** `USHBenchmarkSubsystem::BuildEntityConfig` and `SpawnAgents`.

## 9. Mass processors and queries (`UMassProcessor`, `FMassEntityQuery`)

1. **What it is.** A processor is a `UObject` whose `Execute` runs once per frame in a processing phase and walks the chunks matching its queries (`Source/Runtime/MassEntity/Public/MassProcessor.h`).
2. **Why it exists.** It is Mass's unit of work: declared data access, explicit ordering, optional parallelism.
3. **How it works.** Defaults: phase PrePhysics (`MassProcessor.h:244`), auto-registered (`:252`), not game-thread (`:258`), `Server | Standalone` flags (`MassProcessor.cpp:138`). `ExecutionOrder.ExecuteInGroup` / `ExecuteAfter` / `ExecuteBefore` place it in the frame. A query lists requirements (`AddRequirement`, `AddTagRequirement(…, EMassFragmentPresence::All/None)`) and is iterated with `ForEachEntityChunk`.
4. **When to use it.** Per-entity work over many entities.
5. **When not to.** One-off work for a few entities (do it in a subsystem's tick, as the director does).
6. **Common mistakes.** Forgetting `bAutoRegisterWithProcessingPhases` (the processor never runs); touching actors without `bRequiresGameThreadExecution = true`; not calling `Super::ConfigureQueries` when subclassing an engine processor.
7. **Example.**
   ```cpp
   void UMyProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EM)
   { Super::ConfigureQueries(EM); EntityQuery.AddTagRequirement<FMyTag>(EMassFragmentPresence::All); }
   ```
8. **In this project.** The three S1 subclasses only switch registration on, set flags and add the demo tag to the engine's queries (`05` B4).

## 10. Representation LOD settings (`UMassMovableVisualizationTrait`, `FMassRepresentationParameters`, `FMassVisualizationLODParameters`)

1. **What it is.** The trait that gives an entity actor/instanced-mesh/none representations, with its two parameter structs (`MG/MassRepresentation/Public/MassVisualizationTrait.h`, `MassRepresentationFragments.h:160–330`).
2. **Why it exists.** It is how Mass draws thousands of agents cheaply: actors only where they matter.
3. **How it works.** `LODRepresentation[LOD]` maps each LOD to a representation; `BaseLODDistance` / `VisibleLODDistance` set where each LOD starts (out of view / in view); `LODMaxCount` caps entities per LOD; `BufferHysteresisOnDistancePercentage` stops flicker at the boundaries; `FilterTag` ties the settings to one LOD processor. `bKeepLowResActors` keeps a disabled actor during ISM if the high- and low-res classes match (`01b #5`).
4. **When to use it.** Any Mass agent that is sometimes an actor.
5. **When not to.** Stationary props (`UMassStationaryVisualizationTrait`, or Instanced Actors).
6. **Common mistakes.** Using the soft-deprecated `UMassVisualizationTrait` (`MassVisualizationTrait.h:18–19`); a `FilterTag` that doesn't match the LOD processor's (the LOD is never computed); the default `LODMaxCount` silently capping actors at about 150.
7. **Example.** `Visualization->HighResTemplateActor = AMyAgent::StaticClass(); Visualization->LODParams.LODMaxCount[EMassLOD::High] = 200;`
8. **In this project.** Configured in `USHBenchmarkSubsystem::BuildEntityConfig`, with one actor class for both resolutions.

## 11. `UTickableWorldSubsystem` and `DoesSupportWorldType`

1. **What it is.** A world subsystem (one instance per world, created and destroyed with it) that also ticks as a tickable object.
2. **Why it exists.** World-scoped managers without placing an actor.
3. **How it works.** It ticks after the world's tick groups (`Source/Runtime/Engine/Private/LevelTick.cpp:1821`). `DoesSupportWorldType` decides which worlds create it; the default includes Editor worlds (`Source/Runtime/Engine/Private/Subsystems/WorldSubsystem.cpp:30–32`).
4. **When to use it.** Per-world logic that runs every frame after gameplay.
5. **When not to.** Logic that must run at a specific tick group (use a tick function, or a Mass processor in a phase).
6. **Common mistakes.** Running in editor worlds by accident; doing work in `Initialize` that needs other subsystems (use the first tick or `OnWorldBeginPlay`).
7. **Example.** `virtual bool DoesSupportWorldType(const EWorldType::Type T) const override { return T == EWorldType::Game || T == EWorldType::PIE; }`
8. **In this project.** `USHBenchmarkSubsystem`; it reads settings and spawns on its first tick, after every subsystem's `OnWorldBeginPlay`.

## 12. CSV profiler macros (`CSV_DEFINE_CATEGORY`, `CSV_EVENT`, `CSV_METADATA`)

1. **What it is.** Macros of the engine's CSV profiler, which writes one row per frame (`Source/Runtime/Core/Public/ProfilingDebugging/CsvProfiler.h:50, 56, 85`).
2. **Why it exists.** Per-frame numbers for offline statistics.
3. **How it works.** A category groups stats and events; `CSV_EVENT` marks a frame with a named event (used by `-csvStartOnEvent=`); `CSV_METADATA` adds key/value pairs to the file. In builds without the profiler (Shipping) they compile to nothing (`:96–108`).
4. **When to use it.** Benchmark markers and per-frame counters.
5. **When not to.** Per-entity calls in hot loops (measured at about 50 ns per custom stat in UE5-UpdateBudget): aggregate first, publish once per frame.
6. **Common mistakes.** Expecting events in Shipping builds; publishing per object.
7. **Example.** `CSV_EVENT(SHBench, TEXT("BenchFlip"));`
8. **In this project.** Category `SHBench` with `BenchStart`, `BenchFlip`, `BenchEnd`, and the run's settings as metadata.

## 13. Object helpers: `CreateDefaultSubobject`, `ConstructorHelpers::FObjectFinder`, `LoadObject`, `NewObject`

1. **What it is.** Ways to create or find UObjects: a component of the class default (`CreateDefaultSubobject`), an asset found once in a constructor (`FObjectFinder`, `Source/Runtime/CoreUObject/Public/UObject/ConstructorHelpers.h:77`), an asset loaded at run time (`LoadObject`), a new object at run time (`NewObject`).
2. **Why it exists.** UObjects need registration, an outer and GC tracking, so they are never created with plain `new`.
3. **How it works.** Subobjects created in a constructor become part of every instance; `FObjectFinder` must be used only in constructors; `LoadObject` loads synchronously by path; `NewObject<T>(Outer, …)` creates an object owned by `Outer`.
4. **When to use it.** Components (subobject), fixed assets of a class (`FObjectFinder`), assets chosen at run time (`LoadObject`), traits and configs made in code (`NewObject`).
5. **When not to.** `LoadObject` on a hot path (it blocks); `FObjectFinder` outside constructors (it asserts).
6. **Common mistakes.** An asset referenced only from C++ is not cooked unless a directory is listed in `DirectoriesToAlwaysCook` (`05` B7).
7. **Example.** `static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));`
8. **In this project.** The agent's mesh component and cylinder; the director's cube (`LoadObject`), config and traits (`NewObject`).

## 14. Small utilities: `FParse::Value` / `FParse::Param`, `FRandomStream`, `FCrc::TypeCrc32`

1. **What it is.** Command-line parsing, a seeded random generator, and a CRC over a value's bytes.
2. **Why it exists.** Reproducible runs need fixed inputs, a fixed random sequence and a cheap way to compare results.
3. **How it works.** `FParse::Value(CommandLine, TEXT("SHAgents="), Out)` leaves `Out` unchanged when the key is missing; `FParse::Param` tests a flag. `FRandomStream(Seed)` has its own state, so nothing else in the engine can change its sequence. `FCrc::TypeCrc32(Value, Crc)` chains a running CRC.
4. **When to use it.** Benchmark parameters, seeded placement, determinism checks.
5. **When not to.** `FRandomStream` for security; a CRC as an identity (collisions are possible, fine for a check).
6. **Common mistakes.** Using `FMath::Rand` (global, shared state) for a reproducible layout; hashing `double`s through text.
7. **Example.** `FRandomStream R(7); const float X = R.FRandRange(-1.f, 1.f);`
8. **In this project.** The director's settings, the jittered grid and the start-position hash compared across runs (`09` S1).
