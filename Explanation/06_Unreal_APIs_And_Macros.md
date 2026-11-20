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

The Mass APIs (fragments, traits, processors, queries, actor management, the actor spawner, pooling) are added here from S1, as they enter the code. Their verified behaviour is already in [01b_Research_Findings.md](01b_Research_Findings.md).

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
