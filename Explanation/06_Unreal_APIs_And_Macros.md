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
| 15 | `UENUM`, `UMETA` | S2a |
| 16 | `UScriptStruct::CompareScriptStruct`, `CopyScriptStruct` | S2a |
| 17 | Automation tests | S2a |
| 18 | `checkf` | S2a |
| 19 | `UWorldSubsystem`; Mass subsystem requirements | S2b |
| 20 | Processing phases and groups | S2b |
| 21 | Chunk views and the entity iterator | S2b |
| 22 | Components as data holders, `FindComponentByClass` | S2b |
| 23 | `CSV_CUSTOM_STAT` | S2b |
| 24 | `FFileHelper::SaveStringToFile`, `FPaths::ProjectSavedDir` | S2b |
| 25 | `IMassActorPoolableInterface` (BlueprintNativeEvents) | S3a |
| 26 | `UMassRepresentationActorManagement`, `RepresentationActorManagementClass` | S3a |
| 27 | `GetEntityHandleFromActor`, `GetFragmentDataStruct` | S3a |
| 28 | Component ticking on demand, `EndPlay` reasons | S3a |

Mass APIs are added here as they enter the code (S1: fragments, traits, spawning, processors, representation LOD settings; S2a: reflection-based comparison, automation tests; S2b: phases, chunk views, subsystem requirements; S3a: pooling, actor management, actor → entity lookups). Their verified behaviour is already in [01b_Research_Findings.md](01b_Research_Findings.md).

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

## 15. `UENUM` and `UMETA`

1. **What it is.** A reflected enum. `UMETA(Hidden)` hides one value from editor lists.
2. **Why it exists.** Reflected enums print by name (logs, debuggers, `UEnum::GetValueAsString`) and can be used in `UPROPERTY`s and Blueprint.
3. **How it works.** UHT generates a `UEnum` object describing the values. `enum class … : uint8` is required for Blueprint-exposed enums.
4. **When to use it.** An enum that appears in reflected data or should print by name.
5. **When not to.** Internal indices that never leave C++ (the demo's `ESHChannel`, `ESHEventType` are plain enums).
6. **Common mistakes.** Using a `Count` value as a real choice; forgetting `UMETA(Hidden)` on it.
7. **Example.** `UENUM() enum class EMood : uint8 { Calm, Angry, Count UMETA(Hidden) };`
8. **In this project.** `ESHDivergence` (`05` C1).

## 16. `UScriptStruct::CompareScriptStruct` and `CopyScriptStruct`

1. **What it is.** Reflection-based comparison and copy of a struct whose type is known only at run time (`Source/Runtime/CoreUObject/Public/UObject/Class.h:2481, 2491`).
2. **Why it exists.** Generic code (serialisation, undo, `FInstancedStruct`) must compare or copy structs it doesn't know at compile time.
3. **How it works.** It walks the struct's reflected properties (`UPROPERTY` members) and compares or copies each one.
4. **When to use it.** When the type comes from data: the checker compares any channel; S4's hand-off copies any channel.
5. **When not to.** When the type is known at compile time: plain `==` or assignment is faster.
6. **Common mistakes.** Members without `UPROPERTY` are ignored; passing memory of a different type (it reads past the end; the checker asserts the type first); expecting a tolerance for floats, or bit identity: numeric properties compare with `==` (`UnrealType.h:1760–1769`), so +0 equals −0 and NaN never equals itself.
7. **Example.** `if (!Type.CompareScriptStruct(&A, &B, PPF_None)) { /* they differ */ }`
8. **In this project.** `FSHInvariantChecker::Classify` (`05` C3); S2b's oracle re-sync uses `CopyScriptStruct`.

## 17. Automation tests (`IMPLEMENT_SIMPLE_AUTOMATION_TEST`, `EAutomationTestFlags`)

1. **What it is.** The engine's unit-test framework (`Source/Runtime/Core/Public/Misc/AutomationTest.h`).
2. **Why it exists.** Tests that run in the editor or game, from the command line, with reports.
3. **How it works.** The macro declares a test class with a name like `StateHandoff.Checker.Lost` and registers it; `RunTest` calls `TestEqual`/`TestTrue`, which record failures. Flags say where it may run and must include exactly one filter (`AutomationTest.h:4126–4133`). `Automation RunTests <filter>` runs every test whose full name contains the filter (`^` or `$` anchors it: `Source/Developer/AutomationController/Private/AutomationCommandline.cpp:211–227`); failures make the editor exit with −1 (`Source/Developer/AutomationController/Private/AutomationCommandline.cpp:493–504`).
4. **When to use it.** Pure logic (no world): fast and deterministic.
5. **When not to.** Behaviour that needs a running world and many frames (use the run checks in `09`).
6. **Common mistakes.** No filter flag (compile error); `TestEqual` on an enum class (no overload: compare as integers); tests that depend on the order they run in.
7. **Example.** See `SHInvariantCheckerTests.cpp`.
8. **In this project.** `StateHandoff.Checker.*` (plugin) and `StateHandoff.Demo.*` (game), run by `Tools\run_tests.bat`.

## 18. `checkf`

1. **What it is.** An assertion with a formatted message.
2. **Why it exists.** To stop at the first sign of a programming error, close to its cause.
3. **How it works.** Evaluated in development builds; compiled out of Shipping.
4. **When to use it.** Conditions that can only be false through a bug (a wrong struct type).
5. **When not to.** Data that can legitimately be bad (use an error log and a safe fallback).
6. **Common mistakes.** Side effects inside the condition (they vanish in Shipping).
7. **Example.** `checkf(Type.IsChildOf(FBase::StaticStruct()), TEXT("%s is not a channel"), *Type.GetName());`
8. **In this project.** `FSHInvariantChecker::Classify` (`05` C3).

## 19. `UWorldSubsystem` without a tick, and Mass subsystem requirements (`AddSubsystemRequirement`, `TMassExternalSubsystemTraits`)

1. **What it is.** A world subsystem is one object per world, created and destroyed with it (`Source/Runtime/Engine/Public/Subsystems/WorldSubsystem.h`). A Mass processor declares which subsystems it uses with `ProcessorRequirements.AddSubsystemRequirement<T>(Access)` (`Source/Runtime/MassEntity/Public/MassRequirements.h:100`) and gets them with `Context.GetMutableSubsystem<T>()` (`MassExecutionContext.h:732`).
2. **Why it exists.** World-scoped state that several systems share, without placing an actor; for processors, declared access lets Mass order processors that touch the same subsystem.
3. **How it works.** `TMassExternalSubsystemTraits<T>` says whether `T` may be used off the game thread; without a specialisation, a subsystem is treated as game-thread-only (`Source/Runtime/Mass/MassCore/Public/Mass/ExternalSubsystemTraits.h:26–35`), which forces the processor onto the game thread.
4. **When to use it.** State that both processors and ordinary game code need, for the world's lifetime.
5. **When not to.** Per-entity data (a fragment); work that needs its own tick at a specific point (a tickable subsystem, or a processor in a phase).
6. **Common mistakes.** Accessing a subsystem from a processor without declaring it (Mass can't order the access); declaring a subsystem thread-safe that isn't; caching a subsystem pointer beyond its world.
7. **Example.** `ProcessorRequirements.AddSubsystemRequirement<USHWorkloadSubsystem>(EMassFragmentAccess::ReadWrite);`
8. **In this project.** `USHWorkloadSubsystem` (`05` D4), used by both workload processors and by the director.

## 20. Processing phases and groups (`ProcessingPhase`, `ExecuteInGroup`, `ExecuteBefore`, `UE::Mass::ProcessorGroupNames`)

1. **What it is.** Where a processor runs in the frame: a **phase** (PrePhysics … FrameEnd), and inside a phase a named **group** with order constraints (`Source/Runtime/MassEntity/Public/MassProcessingTypes.h`, `MassProcessor.h:244`).
2. **Why it exists.** Processors are ordered by declared dependencies, not by registration order.
3. **How it works.** Each phase runs in one world tick group: PrePhysics in `TG_PrePhysics` … FrameEnd in `TG_LastDemotable` (`Source/Runtime/MassEntity/Private/MassProcessingPhaseManager.cpp:46–51`). The world runs `TG_LastDemotable` after tickable objects and `TG_PostUpdateWork` (`Source/Runtime/Engine/Private/LevelTick.cpp:1821, 1877, 1883`). Group names are plain `FName`s: `Behavior` from MassCommon (`MG/MassCommon/Public/MassCommonTypes.h:22`), `Representation` and its sub-group `Representation.VisualizationProcessing` from MassRepresentation (`MG/MassRepresentation/Public/MassRepresentationTypes.h:27–30`).
4. **When to use it.** Always state the phase and order a processor depends on.
5. **When not to.** Don't invent a group name to force an order that a dependency would state better.
6. **Common mistakes.** Assuming FrameEnd means "after rendering" (it is a tick group inside the world tick); a typo in a group name (an `FName` is just a name, so a misspelt group is a different group, not an error) **[inferred]**; ordering against a group that isn't in the same phase.
7. **Example.** `ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior; ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::Representation);`
8. **In this project.** `USHSimEventProcessor` (PrePhysics, Behavior, before Representation) and `USHFrameEndProcessor` (FrameEnd) (`05` D5).

## 21. Chunk views and the entity iterator (`GetFragmentView`, `GetMutableFragmentView`, `FEntityIterator`, `FMassActorFragment::GetMutable`)

1. **What it is.** Inside `ForEachEntityChunk`, a processor reads one chunk's fragments as arrays (`TConstArrayView` / `TArrayView`) and walks its entities with `CreateEntityIterator()` (`Source/Runtime/MassEntity/Public/MassExecutionContext.h:256, 268, 349`).
2. **Why it exists.** Fragments of one type are stored contiguously per chunk, so a loop over an array view is cache-friendly.
3. **How it works.** The iterator converts to the entity's index in the chunk (`operator int32`, `:268`), so `View[It]` is that entity's fragment. A mutable view needs the requirement declared `ReadWrite`. `FMassActorFragment::GetMutable()` is non-`const` (`MG/MassActors/Public/MassActorSubsystem.h:117`), so a mutable actor pointer needs the actor fragment's view to be mutable too.
4. **When to use it.** Every per-entity loop in a processor.
5. **When not to.** Outside the chunk callback: a view's memory is only valid during that chunk's iteration.
6. **Common mistakes.** Storing a pointer into a view (the entity may move archetype and the memory with it); requesting `ReadOnly` and then needing to write; fetching views per entity instead of once per chunk.
7. **Example.** `const TConstArrayView<FSHAgentIdFragment> Ids = Context.GetFragmentView<FSHAgentIdFragment>(); for (auto It = Context.CreateEntityIterator(); It; ++It) { Use(Ids[It]); }`
8. **In this project.** `SHWorkloadProcessors::FChunkViews` (`05` D5).

## 22. Components as data holders (`UActorComponent`, `PrimaryComponentTick.bCanEverTick`, `FindComponentByClass`)

1. **What it is.** A component is a `UObject` owned by an actor; `PrimaryComponentTick.bCanEverTick` says whether it ever ticks; `AActor::FindComponentByClass<T>()` returns the actor's first component of a class, or null (`Source/Runtime/Engine/Classes/GameFramework/Actor.h:3788`).
2. **Why it exists.** Actors are built from reusable parts; gameplay systems usually keep their state in a component.
3. **How it works.** A component created with `CreateDefaultSubobject` in the actor's constructor exists on every instance. `FindComponentByClass` walks the actor's component list.
4. **When to use it.** State that belongs to the actor and should live and die with it.
5. **When not to.** State that must outlive the actor, such as an agent's state across a representation change: that is exactly the problem this project studies.
6. **Common mistakes.** Leaving ticking on for a data holder (per-frame cost for nothing); calling `FindComponentByClass` in a hot loop over many actors when a cached pointer would do.
7. **Example.** `State = CreateDefaultSubobject<USHAgentStateComponent>(TEXT("State"));`
8. **In this project.** `USHAgentStateComponent` (`05` D2), found once per agent per processor pass (`05` D5). S4's hand-off will cache it per binding.

## 23. `CSV_CUSTOM_STAT` and `ECsvCustomStatOp`

1. **What it is.** A per-frame custom value in the CSV profiler (`Source/Runtime/Core/Public/ProfilingDebugging/CsvProfiler.h:155`).
2. **Why it exists.** Game counters next to the engine's timings, frame by frame.
3. **How it works.** `CSV_CUSTOM_STAT(Category, Name, Value, Op)` adds a column `Category/Name`; `ECsvCustomStatOp` is `Set`, `Min`, `Max` or `Accumulate` (`:219–225`): `Set` replaces the frame's value, `Accumulate` adds to it. Compiled out without the profiler (`:185`).
4. **When to use it.** One aggregated value per frame.
5. **When not to.** Per entity (about 50 ns a call, measured in UE5-UpdateBudget): count in a member, publish once.
6. **Common mistakes.** Publishing per object instead of once per frame; a category defined in one file and used in another without `CSV_DECLARE_CATEGORY_EXTERN` (`:52`).
7. **Example.** `CSV_CUSTOM_STAT(SHWorkload, Transitions, FrameCounts.Transitions, ECsvCustomStatOp::Set);`
8. **In this project.** `USHWorkloadSubsystem::EndFrame` publishes five counts every frame (`05` D4).

## 24. Writing a file: `FFileHelper::SaveStringToFile`, `FPaths::ProjectSavedDir`

1. **What it is.** Write a string to a file in one call (`Source/Runtime/Core/Public/Misc/FileHelper.h:196`); the project's `Saved` directory (`Source/Runtime/Core/Public/Misc/Paths.h:290`).
2. **Why it exists.** Simple reports without managing file handles.
3. **How it works.** If the file can't be opened, the file manager creates its directory and tries again (`Source/Runtime/Core/Private/HAL/FileManagerGeneric.cpp:110–116`); the call returns `false` on failure. In a packaged build, `ProjectSavedDir()` is the build's own `Saved` folder (`Build\Windows\StateHandoff\Saved` here).
4. **When to use it.** One report at the end of a run.
5. **When not to.** Writing every frame (build the text in memory and write once).
6. **Common mistakes.** Ignoring the return value; assuming the packaged game writes to the project's `Saved` folder.
7. **Example.** `FFileHelper::SaveStringToFile(Report, *FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SHRuns"), Name));`
8. **In this project.** `USHWorkloadSubsystem::WriteReport` (`05` D4); `run_bench.py` collects the file (`05` D9).

## 25. Mass actor pooling: `IMassActorPoolableInterface` (BlueprintNativeEvents)

1. **What it is.** The interface an actor class implements to be pooled by Mass: `CanBePooled`, `PrepareForPooling`, `PrepareForGame` (`MG/MassActors/Public/MassActorPoolableInterface.h:22–29`).
2. **Why it exists.** Spawning actors is expensive; Mass can hide a released actor and give it to another entity later.
3. **How it works.** On release, Mass pools an actor only if pooling is on (`mass.spawning.actorpooling`, default on, `MG/MassActors/Private/MassActorSpawnerSubsystem.cpp:24, 31`), the actor implements the interface and `CanBePooled` says yes (`:121–127`); it calls `PrepareForPooling` and hides the actor. On reuse it shows and moves the actor and calls `PrepareForGame` (`:235–262`). A pooled actor gets **no** `EndPlay` or `BeginPlay` (`01b #4`). The functions are BlueprintNativeEvents: callers use `IMassActorPoolableInterface::Execute_CanBePooled(Actor)`, and a C++ class overrides `CanBePooled_Implementation()`, whose generated default returns `false`.
4. **When to use it.** Actor representations that are spawned and released often.
5. **When not to.** Actors whose state can't be reset reliably: a pooled actor keeps everything its previous entity left in it.
6. **Common mistakes.** Overriding `CanBePooled()` instead of `CanBePooled_Implementation()`; calling the function directly instead of through `Execute_…`; putting hand-off code only in `BeginPlay`/`EndPlay` (it never runs for pooled actors).
7. **Example.** `virtual bool CanBePooled_Implementation() override { return bAllowPooling; }`
8. **In this project.** `ASHDemoAgent` (`05` E4); B1's pool callbacks (`05` E1).

## 26. Replacing Mass's actor management: `UMassRepresentationActorManagement`, `RepresentationActorManagementClass`

1. **What it is.** The class whose functions Mass calls when it spawns, enables, disables, teleports or releases an entity's actor (`MG/MassRepresentation/Public/MassRepresentationActorManagement.h`), chosen per visualization trait with `Params.RepresentationActorManagementClass` (`MassRepresentationFragments.h:169`).
2. **Why it exists.** Games customise how actors are spawned and set up without rewriting the representation processor.
3. **How it works.** Mass uses the class default object (`MassRepresentationFragments.h:290`), so overrides are `const` and keep no state. `OnPostActorSpawn` runs after an actor was spawned or taken from the pool; the base version records the actor in the entity's `FMassActorFragment` and the actor → entity map (`MassRepresentationActorManagement.cpp:92–117`). `OnPreActorSpawn` runs before.
4. **When to use it.** Per-actor setup that needs the entity's data at spawn time.
5. **When not to.** Anything that must run when the actor is re-enabled without a spawn (a kept, disabled actor): no spawn callback runs then.
6. **Common mistakes.** Forgetting `Super::OnPostActorSpawn` (the actor fragment is never set); storing state in the class; assuming the callback runs for a kept actor that is re-enabled.
7. **Example.** `Visualization->Params.RepresentationActorManagementClass = UMyActorManagement::StaticClass();`
8. **In this project.** B2's push on spawn, `USHB2ActorManagement` (`05` E2). S6's pre-spawn capture uses `OnPreActorSpawn`.

## 27. Finding an actor's entity and its fragments: `UMassActorSubsystem::GetEntityHandleFromActor`, `FMassEntityManager::GetFragmentDataStruct`

1. **What it is.** The actor → entity map (`MG/MassActors/Public/MassActorSubsystem.h:148`), and access to one fragment of one entity by its reflected type (`Source/Runtime/MassEntity/Public/MassEntityManager.h:788`), with `IsEntityValid` (`:738`).
2. **Why it exists.** Actor-side code (components) has an actor, not an entity handle.
3. **How it works.** The map entry is written when the actor is recorded in the entity's actor fragment (`SetAndUpdateHandleMap`, in `OnPostActorSpawn`) and removed when the actor is released (`ResetAndUpdateHandleMap`, `MassRepresentationActorManagement.cpp:135–141`). `GetFragmentDataStruct` returns an `FStructView` (type + memory) of the fragment.
4. **When to use it.** Occasional lookups from outside a processor.
5. **When not to.** Per-entity work every frame: a processor's chunk views are far cheaper (`06` §21).
6. **Common mistakes.** Looking up the entity in `BeginPlay` (the map entry doesn't exist yet, `01b #7`); looking it up again at `EndPlay` (the entry is already gone); keeping the `FStructView` across frames.
7. **Example.** `const FMassEntityHandle E = ActorSubsystem->GetEntityHandleFromActor(GetOwner()); if (EntityManager.IsEntityValid(E)) { … }`
8. **In this project.** B1's hydrate and dehydrate (`05` E1); B2's push uses the spawn request's entity handle instead.

## 28. Component ticking on demand, and `EndPlay` reasons

1. **What it is.** `PrimaryComponentTick.bCanEverTick` / `bStartWithTickEnabled` decide whether a component can tick and whether it starts ticking; `SetComponentTickEnabled` turns it on and off at run time. `EndPlay(EEndPlayReason::Type)` says why play ends (`Destroyed`, `LevelTransition`, `EndPlayInEditor`, `RemovedFromWorld`, `Quit`).
2. **Why it exists.** Ticking costs time even when the tick does nothing; and cleanup depends on why the object goes away.
3. **How it works.** A component with `bCanEverTick = true` and `bStartWithTickEnabled = false` registers a tick function that stays disabled until enabled.
4. **When to use it.** Retry-until-ready work, like waiting for a mapping to appear.
5. **When not to.** Work that has a natural event to hook into.
6. **Common mistakes.** Leaving the tick on after the work is done; doing world work in `EndPlay` during teardown (`Quit`, `EndPlayInEditor`), when other systems are shutting down.
7. **Example.** `if (!TryHydrate()) { SetComponentTickEnabled(true); }`
8. **In this project.** `USHLifecycleComponent` (`05` E1).
