# 05 · Line By Line Code Explanation: StateHandoff

**Rule:** every file that exists in `StateHandoff\` and `Tools\` is explained here, line by line, in the same step it is written. Sections follow the build order in [03_Implementation_Roadmap.md](03_Implementation_Roadmap.md). Engine references are relative to `C:\Program Files\Epic Games\UE_5.8\Engine` (5.8.4); `Templates\…` is relative to `C:\Program Files\Epic Games\UE_5.8`.

| Part | Step | Files |
|---|---|---|
| A | Phase 4, F1–F4 (foundation) | `.uproject`, two `Target.cs`, two `Build.cs`, two module `.cpp`, `.uplugin`, three config `.ini`, `Tools\*.bat`, `Tools\cpu_affinity.py` |
| B | S1 (workload skeleton: Mass only) | `SHDemoTypes.h`, `SHDemoStateTrait`, `SHDemoAgent`, `SHMassProcessors`, `SHBenchmarkSubsystem`; `Build.cs` and `DefaultGame.ini` changes; `Tools\run_uncooked.bat`, `run_packaged.bat` |
| C | S2a (the instrument, as plain C++) | Plugin `SHContractTypes.h`, `SHInvariantChecker`, tests; game `SHDemoState`, tests; both `Build.cs` changes; `Tools\run_tests.bat` |

---

## Part A · Foundation (Phase 4)

### A1. `StateHandoff\StateHandoff.uproject`: the project descriptor

```json
{
	"FileVersion": 3,
	"EngineAssociation": "5.8",
	"Category": "",
	"Description": "P09 StateHandoff: gameplay state that survives Mass LOD switches (UE 5.8.4 portfolio project).",
	"Modules": [ { "Name": "StateHandoff", "Type": "Runtime", "LoadingPhase": "Default" } ],
	"Plugins": [ { "Name": "StateHandoffCore", "Enabled": true }, { "Name": "MassGameplay", "Enabled": true },
	             { "Name": "AndroidFileServer", "Enabled": false } ]
}
```

| Line | What it does, and why |
|---|---|
| `"FileVersion": 3` | The descriptor schema version. Epic's 5.8 `TP_Blank` template uses 3 (`Templates/TP_Blank/TP_Blank.uproject:2`). A wrong value makes the editor refuse the file or offer to upgrade it |
| `"EngineAssociation": "5.8"` | Tells the launcher and the "Generate project files" shell action which installed engine opens this project. The raw template leaves it empty (`TP_Blank.uproject:3`), which makes double-clicking ask for an engine. Pinning 5.8 matters here more than usual: Mass was restructured in 5.7 and 5.8 (`01b` §4), so opening this project in another version would silently change the APIs it depends on |
| `"Category"`, `"Description"` | Shown in the project browser. Cosmetic |
| `"Modules"` | The C++ modules the **project** owns. UnrealBuildTool compiles exactly these, plus the modules of enabled plugins |
| `"Name": "StateHandoff"` | Must match the folder `Source\StateHandoff\` and the class name in `StateHandoff.Build.cs`. Module names are unique per project, which is why the plugin is called `StateHandoffCore` |
| `"Type": "Runtime"` | Built into game **and** editor targets. *Alternative:* `Editor`, which would leave the module out of packaged builds, where the benchmarks run |
| `"LoadingPhase": "Default"` | The normal loading point (`Source/Runtime/Projects/Public/ModuleDescriptor.h:47`), after engine start-up and before the first world exists. Early enough for this module's classes (world subsystems, Mass processors and traits in later steps) to be registered before the map loads. *Not* `PreDefault`/`PostEngineInit` (:44, :53): nothing here needs to load earlier or later |
| `"Plugins"` / `"StateHandoffCore"` | Enables the project's own plugin. Project plugins with `EnabledByDefault` load anyway (`Source/Runtime/Projects/Private/PluginManager.cpp:421–425`), but listing it keeps every dependency visible in one place |
| `"MassGameplay"` | Enables the **Experimental** MassGameplay plugin (`IsExperimentalVersion: true`, `Plugins/Runtime/MassGameplay/MassGameplay.uplugin:16`), which is off by default (`:13`). It brings the representation, actor, LOD and spawner modules, and through its own dependencies ZoneGraph, PoseSearch, ZoneGraphAnnotations, SmartObjects, StateTree and DataValidation (`:86–111`). Enabled in Phase 4, before any code uses it, so the **launcher-engine check** (does an installed engine build, cook and package these plugins?) happens now (`09` F-4). *Not listed:* `MassEntity`: since 5.8 it is an engine module, not a plugin (`01b` §4); `MassAI` and `InstancedActors`: not used by the design (`02` §1) |
| `"AndroidFileServer"`, `"Enabled": false` | **Turns off** an engine plugin that is on by default (`Plugins/Runtime/AndroidFileServer/AndroidFileServer.uplugin:15`). Its editor module's settings object generates a random `SecurityToken` the first time it is created and writes it into the project's `DefaultEngine.ini` (`Plugins/Runtime/AndroidFileServer/Source/AndroidFileServerEditor/Private/AndroidFileServerRuntimeSettings.cpp:31–36`; the class is `Config = Engine, DefaultConfig`, `Public/AndroidFileServerRuntimeSettings.h:23`). A token must never reach a public repository, and this project doesn't deploy to Android, so the plugin is disabled. Portfolio rule since 2026-11-21 |

### A2. `Source\StateHandoff.Target.cs` and `StateHandoffEditor.Target.cs`: build targets

```csharp
// P09 StateHandoff - game target (standalone/packaged builds).

using UnrealBuildTool;

public class StateHandoffTarget : TargetRules
{
	public StateHandoffTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("StateHandoff");
	}
}
```

| Line | What it does, and why |
|---|---|
| `// P09 StateHandoff - …` | A one-line comment saying which executable this file describes. Target files are easy to mix up because the two look almost identical |
| `using UnrealBuildTool;` | Target files are **C#**, compiled by UnrealBuildTool (UBT) itself, not C++. This brings `TargetRules`, `TargetType`, `BuildSettingsVersion` and `EngineIncludeOrderVersion` into scope |
| `public class StateHandoffTarget : TargetRules` | UBT finds targets by class name, `<Name>Target`. This class defines the target `StateHandoff`, which is why `build.bat StateHandoff …` works. The editor file defines `StateHandoffEditorTarget`, so the target `StateHandoffEditor` |
| `(TargetInfo Target) : base(Target)` | UBT passes the platform, configuration and architecture being built; the base constructor stores them |
| `Type = TargetType.Game` | A standalone game executable with no editor code: the target that gets packaged and benchmarked. The editor file uses `TargetType.Editor`, which builds DLLs loaded by `UnrealEditor.exe` |
| `DefaultBuildSettings = BuildSettingsVersion.V7` | Opts into this engine's newest default compiler and build settings. Epic's 5.8 template uses `V7` (`Templates/TP_Blank/Source/TP_Blank.Target.cs:11`). Older values keep older defaults for compatibility |
| `IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8` | Uses 5.8's header include order (Epic trims transitive includes each version). The template uses the same (`TP_Blank.Target.cs:12`). It matters for Mass in particular: 5.8 headers already guard old includes behind `UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6` (e.g. `MassRepresentationFragments.h:7–9`), so pinning the order means missing includes show up now, not after an upgrade |
| `ExtraModuleNames.Add("StateHandoff")` | Which of the project's own modules this target builds. Plugin modules come in through the plugin system |

**Why two target files?** The editor and the packaged game are different executables with different module sets, and UBT needs one target rule per executable type. There is no `Server` or `Client` target: the project is single-player (`01b #12`).

### A3. `Source\StateHandoff\StateHandoff.Build.cs`: the game module's build rules

```csharp
public class StateHandoff : ModuleRules
{
	public StateHandoff(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Phase 4: only the engine basics. Mass and StateHandoffCore are added in S1, when the first code uses them.
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
	}
}
```

| Line | What it does, and why |
|---|---|
| Header comments | Say what this module is for (workload, baselines, benchmark harness) and state the **dependency rule**: the game module depends on the plugin, never the reverse (`02` §3) |
| `class StateHandoff : ModuleRules` | The class name **must equal the module name**; UBT maps each `*.Build.cs` file to its module |
| `ReadOnlyTargetRules Target` | A module can read target settings (platform, configuration) but not change them, so module rules can't fight each other |
| `PCHUsage = UseExplicitOrSharedPCHs` | Use a **shared precompiled header** built by the engine for a common dependency set, unless the module names its own. Precompiled headers make each `.cpp` compile faster. The template uses the same (`TP_Blank.Build.cs:9`). *Rejected:* `NoPCHs` (slow); a private PCH (unneeded for a small module) |
| `PublicDependencyModuleNames … Core, CoreUObject, Engine` | `Core`: containers, strings, logging, the module macros. `CoreUObject`: UObject, reflection, GC. `Engine`: actors, components, worlds. **Public** because the later public headers (an actor, subsystems) expose engine types; the rule is "private unless a public header needs it" |
| The comment "Phase 4: only the engine basics" (**replaced in S1**: see B6) | The Mass modules and `StateHandoffCore` are **not** listed yet: no code includes their headers. Adding them with the first code keeps the list honest. Enabling the MassGameplay *plugin* in the `.uproject` is enough to build, link and package its modules now (UBT adds every module of an enabled plugin that compiles in the current configuration to the target, `Source/Programs/UnrealBuildTool/Configuration/UEBuildTarget.cs:5164–5193`), which is what the Phase 4 check needs |
| *Not included:* `InputCore`, `EnhancedInput` | The template adds them (`TP_Blank.Build.cs:11`), but this project reads no player input in code |

### A4. `Source\StateHandoff\Private\StateHandoffModule.cpp`

```cpp
// P09 StateHandoff - primary game module registration.

#include "Modules/ModuleManager.h"

// The game module needs no startup logic, so the engine's default implementation is enough.
IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, StateHandoff, "StateHandoff");
```

| Line | What it does, and why |
|---|---|
| `#include "Modules/ModuleManager.h"` | Declares `IMPLEMENT_PRIMARY_GAME_MODULE` and `FDefaultGameModuleImpl`. Include the specific header, not a large one ("include what you use") |
| `IMPLEMENT_PRIMARY_GAME_MODULE(…)` | Generates the module's entry point (`Source/Runtime/Core/Public/Modules/ModuleManager.h:1100–1136`). In monolithic (packaged) builds it also defines the project-name globals the game starts from; in modular (editor) builds the name comes from the command line. **Primary** = the project's main game module; a project needs at least one. *Without it* the module never registers, and a packaged game has no project-name global |
| `FDefaultGameModuleImpl` | An engine module class with empty `StartupModule`/`ShutdownModule`. This module has no start-up logic, so writing a class would add one with no reason |
| `StateHandoff` | The module name; must match the `.uproject` and `Build.cs` |
| `"StateHandoff"` | The old "game name" argument. In 5.8 it is ignored: the monolithic overloads name it `DEPRECATED_GameName`, and the header says specifying the name "is no longer necessary" (`ModuleManager.h:1096, 1100`). The macro still takes three arguments, so the project name is passed, as in the template (`Templates/TP_Blank/Source/TP_Blank/TP_Blank.cpp`) |

**Why there is no `StateHandoff.h`:** the template ships an empty module header that only its own `.cpp` includes. An empty header adds nothing, so it is left out. The empty `Public\` folder is left out too; S1 creates it with the first public header.

### A5. `Plugins\StateHandoffCore\StateHandoffCore.uplugin`: the plugin descriptor

| Field | What it does, and why |
|---|---|
| `"FileVersion": 3`, `"Version": 1`, `"VersionName": "1.0"` | Schema version (as for the `.uproject`); an integer version for upgrade checks; a display version |
| `"FriendlyName"`, `"Description"`, `"Category": "Gameplay"` | What the Plugins window shows. The description states the three parts of the system: contract, commit-time hand-off, checker |
| `"CreatedBy"`, `"…URL"` fields | Left empty on purpose: no personal contact details in a public repository |
| `"EnabledByDefault": true` | A project plugin with this flag loads even if a `.uproject` forgets it (`PluginManager.cpp:421–425`); the `.uproject` lists it anyway (A1) |
| `"CanContainContent": false` | The system is code only. Declaring no content keeps the plugin out of the cook's content scan |
| `"IsBetaVersion": false`, `"IsExperimentalVersion": false` | Our own maturity flags; they drive the editor badge. Note the plugin *depends* on an Experimental one, which the README states |
| `"Installed": false` | A project plugin, not an engine/marketplace install |
| `"Modules"` → `StateHandoffCore`, `Runtime`, `Default` | One runtime module, loaded at the normal point, so its classes (trait, subsystem, processor) are registered before any world. `Runtime` because the hand-off must run in packaged games |
| `"Plugins"` → `MassGameplay`, enabled | A **plugin dependency**: any project that enables StateHandoffCore also gets MassGameplay, and the plugin manager reads it from the `"Plugins"` array (`Source/Runtime/Projects/Private/PluginDescriptor.cpp:391`). It is declared now because the plugin has no meaning without Mass. The **module** dependencies (`MassEntity`, `MassRepresentation`, …) are added to `Build.cs` with the first code that includes their headers (A6) |

### A6. `Plugins\StateHandoffCore\Source\StateHandoffCore\StateHandoffCore.Build.cs`

```csharp
public class StateHandoffCore : ModuleRules
{
	public StateHandoffCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Phase 4: only the engine basics. The Mass modules (MassEntity, MassCore, MassActors, MassRepresentation,
		// MassSpawner) are added in S2/S5, together with the first code that includes their headers.
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
	}
}
```

| Line | What it does, and why |
|---|---|
| Header comment | "It must not depend on any game module": the plugin is the reusable half and must build in any project |
| `class StateHandoffCore : ModuleRules` | Class name = module name |
| `PCHUsage = UseExplicitOrSharedPCHs` | As in A3 |
| `Core, CoreUObject, Engine` (public) | The plugin's public headers will expose engine and Mass types (a trait, a subsystem, a processor), so its dependencies are public. The comment lists the Mass modules that arrive later and when (`02` §3). **S2a** added `MassCore` (see C7) |

### A7. `Plugins\…\Private\StateHandoffCoreModule.cpp`

```cpp
#include "Modules/ModuleManager.h"
IMPLEMENT_MODULE(FDefaultModuleImpl, StateHandoffCore);
```

| Line | What it does, and why |
|---|---|
| `IMPLEMENT_MODULE(FDefaultModuleImpl, StateHandoffCore)` | The entry point for a **non-primary** module (`ModuleManager.h:946, 955`): one per module, and a plugin module never uses the primary-game variant. `FDefaultModuleImpl` because there is no start-up or shutdown logic |

### A8. `Config\DefaultEngine.ini`

| Line(s) | What it does, and why |
|---|---|
| `[/Script/EngineSettings.GameMapsSettings]` `GameDefaultMap=`, `EditorStartupMap=` `/Engine/Maps/Templates/Template_Default` | An engine map with a floor and lights, so no map has to be authored. The workload is spawned by code (`02` §4.2), as in UE5-UpdateBudget |
| `[/Script/Engine.Engine]` `bSmoothFrameRate=False`, `bUseFixedFrameRate=False` | No frame-rate smoothing and no fixed frame-rate cap, so frame time reflects real work. These restate defaults on purpose, so a later change can't silently cap or smooth the benchmark |
| `[SystemSettings]` `r.VSync=0` | VSync off, so presenting never blocks the frame |
| `[/Script/Engine.RendererSettings]` `r.AllowStaticLighting=False`, `r.Shadow.Virtual.Enable=1` | Dynamic lighting only (nothing is baked; the map is the engine's) and virtual shadow maps, the 5.8 template defaults. They keep the render setup identical to the portfolio's other projects |
| `[/Script/WindowsTargetPlatform.WindowsTargetSettings]` `DefaultGraphicsRHI=DefaultGraphicsRHI_DX12`, `-…=PCD3D_SM5`, `+…=PCD3D_SM6` | DirectX 12 with Shader Model 6 only, so only one shader format is cooked: shorter cooks, and the same RHI as the benchmark runs |

### A9. `Config\DefaultGame.ini`

| Line | What it does, and why |
|---|---|
| `[/Script/EngineSettings.GeneralProjectSettings]` `ProjectName=StateHandoff`, `Description=…` | The project's display name and description in packaged builds |
| *Not yet:* `+DirectoriesToAlwaysCook=(Path="/Engine/BasicShapes")` | S1 adds it, when C++ first references an engine shape (a mesh referenced only from C++ is not cooked unless listed) |

### A10. `Config\DefaultInput.ini`

| Line | What it does, and why |
|---|---|
| `[/Script/Engine.InputSettings]` `DefaultPlayerInputClass=/Script/EnhancedInput.EnhancedPlayerInput`, `DefaultInputComponentClass=/Script/EnhancedInput.EnhancedInputComponent` | The two keys Epic's 5.8 template sets (`Templates/TP_Blank/Config/DefaultInput.ini:79–80`). `BaseInput.ini` doesn't set them, so without this file the engine creates the classic `UPlayerInput`, and the Enhanced Input plugin, which is enabled by default (`Plugins/EnhancedInput/EnhancedInput.uplugin:13`), logs a warning at start-up that its local-player subsystem "does not have a valid PlayerInput object" (`Plugins/EnhancedInput/Source/EnhancedInput/Private/EnhancedInputSubsystems.cpp:118`). The foundation must run with 0 warnings, so the file exists even though the project reads no input. Several sibling projects hit this warning in their first packaged run; it was checked here before the first run |

### A11. `Tools\*.bat`: build, package, smoke, and the chained Phase 4 check

All five scripts start with `@echo off` (don't echo each command), use `setlocal` (variables don't leak into the caller), quote every path (the folder name `UNREAL PORTFOLIO` contains a space), build paths from `%~dp0` (the script's own folder, so they work from any current directory), and end with `exit /b %ERRORLEVEL%` (pass the real exit code back to the queue). Each says in a comment that it must be run through the UE job queue.

**`build.bat [Target] [Configuration]`**

| Line | What it does, and why |
|---|---|
| `set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"` | The one installed engine this project is pinned to |
| `set "PROJECT=%~dp0..\StateHandoff\StateHandoff.uproject"` | The project file, relative to the script |
| `set "TARGET=%~1"` + default `StateHandoffEditor`; `set "CONFIG=%~2"` + default `Development` | Optional arguments. `%~1` strips quotes. The header lists the valid values; Test is marked unavailable on a launcher engine |
| `call "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" %TARGET% Win64 %CONFIG% -Project="%PROJECT%" -WaitMutex -NoHotReloadFromIDE` | Epic's UBT entry point. `-WaitMutex`: wait for another UBT instead of failing; `-NoHotReloadFromIDE`: a normal build, not a hot-reload patch. `call` returns to this script afterwards |

**`package.bat [Configuration]`**

| Line | What it does, and why |
|---|---|
| `set "ARCHIVE=%~dp0..\Build"` | The packaged build goes to `P09_StateHandoff\Build\` (git-ignored) |
| `RunUAT.bat BuildCookRun -project=… -noP4 -utf8output -unattended -platform=Win64 -clientconfig=%CONFIG% -build -cook -stage -pak -archive -archivedirectory=…` | One command that builds the game target, cooks content, stages, packs into `.pak` and archives. `-noP4`: no Perforce; `-unattended`: no dialogs; `-utf8output`: readable logs. `-clientconfig=Development`: the headline benchmark configuration (the launcher engine can't build Test) |
| Header comment, two output paths | The bootstrap `Build\Windows\StateHandoff.exe` and the **real** game `Build\Windows\StateHandoff\Binaries\Win64\StateHandoff.exe`. Timed runs start the real exe (A12) |

**`smoke_game.bat [extra args…]`**: the uncooked game in the editor binary

| Line | What it does, and why |
|---|---|
| `set "LOG=…\StateHandoff\Saved\Logs\SmokeGame.log"` | A fixed log path inside the git-ignored `Saved\` folder, so the check can grep it |
| `UnrealEditor-Cmd.exe "%PROJECT%" -game -nullrhi -nosound -unattended -nosplash -traceautostart=0 -ExecCmds="quit" -log -abslog="%LOG%" %*` | `-game`: run the game, not the editor UI; `-nullrhi`: no rendering (no GPU needed); `-nosound`; `-unattended`: no dialogs; `-traceautostart=0`: an open Unreal Insights must not trace this run; `-ExecCmds="quit"`: leave as soon as the map has loaded; `-log -abslog=`: write the log to the fixed path; `%*`: pass any extra arguments through. *Not passed:* `-ExitAfterSeconds`, which no engine source file in 5.8.4 parses (a search of every `.cpp` under `Source` and `Plugins` finds no match), so it would do nothing |

**`smoke_packaged.bat [extra args…]`**: the packaged game

| Line | What it does, and why |
|---|---|
| `set "EXE=…\Build\Windows\StateHandoff\Binaries\Win64\StateHandoff.exe"` | The real packaged executable, not the bootstrap, so the exit code is the game's own |
| `if not exist "%EXE%" ( … exit /b 2 )` | A clear message and a non-zero code if packaging hasn't run, instead of cmd's generic "path not found" |
| `"%EXE%" -nullrhi -nosound -unattended -nosplash -traceautostart=0 -ExecCmds="quit" -log -abslog="%LOG%" %*` | The same quick load-and-quit as the uncooked smoke, against cooked content |

**`phase4_checks.bat`**: all foundation checks as **one** queue job

| Line | What it does, and why |
|---|---|
| `echo === F-1 …` lines | Markers in the job log, so each step's output can be found and quoted in `09` |
| `call "%~dp0build.bat" StateHandoffEditor Development \|\| exit /b 1` and the four lines like it | Run the steps in order: editor build, game build, uncooked smoke, package, packaged smoke. `\|\| exit /b N` stops at the first failure and returns a code that names the failing step (1–5) |
| `exit /b 0` | All steps passed. One job instead of five means one wait in the machine-wide queue, which ten projects share |

### A12. `Tools\cpu_affinity.py`: pinning timed runs to the performance cores

| Part | What it does, and why |
|---|---|
| Module docstring | Why pinning exists (a hybrid CPU; UE5-UpdateBudget saw the game thread land on an E-core, about 1.6× slower), and the recipe for Phase 5: the batch script pins **itself** once, records the mask read back from Windows, then starts the real exe with `subprocess.Popen`, waits with a timeout and kills the tree in `finally`. It says explicitly why not `cmd /c start /wait`: that always returns 0 |
| `import ctypes`, `struct`, `sys` | `ctypes` calls Windows functions in `kernel32.dll`; `struct` unpacks the binary records Windows returns; `sys.platform` keeps the module importable off Windows |
| `KERNEL32 = ctypes.WinDLL("kernel32", use_last_error=True)` and the `restype`/`argtypes` lines | Load `kernel32` once and declare the argument and return types of the three functions used, so `ctypes` passes 64-bit handles and masks correctly instead of truncating them to 32-bit `int`s. `None` off Windows, and every function then returns a neutral value |
| `performance_core_mask()` | Calls `GetLogicalProcessorInformationEx(RelationProcessorCore)` twice: once to learn the buffer size, once to fill it. Walks the variable-length records; for each physical core reads its `EfficiencyClass` (byte 9) and its group-0 affinity mask (offset 32), and ORs the masks per class. On a hybrid CPU the P-cores have the higher class, so the mask of the highest class is the P-core mask. Returns `None` on a non-hybrid CPU (one class) or any failure, so callers simply don't pin. Copied from UE5-UpdateBudget so this repository stands alone |
| `pin_self(mask)` | `SetProcessAffinityMask(GetCurrentProcess(), mask)`: restricts the **batch script itself**. Windows gives a new process its parent's affinity, so every game the script starts afterwards is pinned from its first instruction, engine start-up included. One call instead of pinning each child after `Popen` (which left a start-up window) |
| `applied_mask()` | `GetProcessAffinityMask` reads the mask back from Windows, so the environment record shows what was **applied**, not what was requested; it says whether that is all logical processors or the P-cores only |
| `if __name__ == "__main__":` | `python -I cpu_affinity.py` prints the mask: on this machine `0xFFFF` (logical CPUs 0–15), checked on 2026-11-21 (`09` T-1, T-2) |

---

## Part B · S1: workload skeleton (Mass only)

S1 builds a workload that the engine's own representation LOD acts on: N Mass entities that switch between actor, instanced cube and nothing as a camera moves and jumps. Nothing about state yet. Engine paths below use `MG/` = `Plugins/Runtime/MassGameplay/Source/`, as in `01b`.

### B1. `Public\SHDemoTypes.h`: the agent index and the demo tag

| Line | What it does, and why |
|---|---|
| `#include "Mass/EntityElementTypes.h"` | Brings in `FMassFragment` and `FMassTag`, which since 5.8 live in the MassCore module (`Source/Runtime/Mass/MassCore/Public/Mass/EntityElementTypes.h:15, 40`). **Found in S1:** the first draft included the old MassEntity header `MassEntityElementTypes.h`, which 5.8 keeps only as a deprecated forwarder (`UE_DEPRECATED_HEADER`, `Source/Runtime/MassEntity/Public/MassEntityElementTypes.h:5–6`); the build reported warning C4996 in four files, so the include was changed. MassCore is already a public dependency (B6) |
| `#include "SHDemoTypes.generated.h"` | The file the Unreal Header Tool (UHT) writes for this header's `USTRUCT`s; it must be the last include |
| `USTRUCT() struct FSHAgentIdFragment : public FMassFragment` | A **fragment**: one plain struct per entity, stored by Mass in chunks of struct arrays. `USTRUCT()` registers it with reflection, which Mass needs to know its size and type at run time (`UScriptStruct`). Deriving from `FMassFragment` is what makes Mass treat it as a fragment; the engine's static check says a fragment must be trivially copyable (`EntityElementTypes.h:21`), so no constructors or heap members |
| `GENERATED_BODY()` | Pastes the reflection boilerplate UHT generated (e.g. `StaticStruct()`) into the struct. Required in every `USTRUCT` |
| `UPROPERTY() int32 AgentIndex = INDEX_NONE;` | The agent's position in spawn order. `int32`, the engine's index type; `INDEX_NONE` (−1) means "not set", so a forgotten initialisation is visible. `UPROPERTY()` without specifiers: visible to reflection (debuggers such as `mass.PrintEntityFragments` can print it, `01b` M-2) but not editable or exposed to Blueprint. **Why an index and not a map keyed by entity handle:** the oracle and the checker (S2) keep one array slot per agent, which is cheaper and simpler than hashing handles every frame |
| `USTRUCT() struct FSHDemoAgentTag : public FMassTag` | A **tag**: a type with no data that an entity either has or doesn't. Mass stores tags as bits in the archetype, so a tag costs no memory per entity. Every processor of this project requires it, as MassCrowd does with `FMassCrowdTag` (`Plugins/AI/MassCrowd/Source/MassCrowd/Private/MassCrowdVisualizationProcessor.cpp:31–35`), so the project never touches entities other systems spawn |

### B2. `SHDemoStateTrait.h` / `.cpp`: what every demo entity is made of

| Line | What it does, and why |
|---|---|
| `#include "MassEntityTraitBase.h"` | The trait base class, from the MassSpawner module (`MG/MassSpawner/Public/MassEntityTraitBase.h:18`) |
| `UCLASS(meta = (DisplayName = "SH Demo State"))` | A reflected class, so Mass can create and list it (traits are `UObject`s that live in entity configs). The display name is what the editor would show in a config asset |
| `class USHDemoStateTrait : public UMassEntityTraitBase` | A **trait** is a reusable recipe of fragments and tags; entity configs are lists of traits (`MassEntityTraitBase.h:11–15`) |
| `virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;` | The one function a trait must implement (pure virtual, `MassEntityTraitBase.h:51`). `const`: building a template must not change the trait. `override`: the compiler checks the signature matches the base. Protected, as the engine's visualization trait declares it (`MG/MassRepresentation/Public/MassVisualizationTrait.h:84`) |
| `BuildContext.AddFragment<FTransformFragment>();` | Every demo entity has a transform. The visualization trait only **requires** it (`MG/MassRepresentation/Private/MassVisualizationTrait.cpp:68`), it doesn't add it; a missing required fragment fails template validation. `FTransformFragment` moved to MassCore in 5.8 (`Source/Runtime/Mass/MassCore/Public/Mass/EntityFragments.h:10`) |
| `BuildContext.AddFragment<FMassActorFragment>();` | Where Mass stores the entity's actor, when it has one. Also only *required* by the visualization trait (`MassVisualizationTrait.cpp:69`) |
| `BuildContext.AddFragment<FSHAgentIdFragment>();` / `AddTag<FSHDemoAgentTag>();` | The demo's own index and tag (B1) |
| The header comment "S2 adds the four state channels" | This trait is used in **every** configuration, so the four state fragments added in S2 exist identically in B0–B3 and SH (memory fairness, SC7) |

### B3. `SHDemoAgent.h` / `.cpp`: the actor representation

| Line | What it does, and why |
|---|---|
| `UCLASS() class ASHDemoAgent : public AActor` | The class Mass spawns for both high and low resolution (B5). One class for both gives one template index, which makes the engine keep the actor, disabled, while the entity is shown as an instanced mesh: the kept-disabled path (`01b #5`, `MassVisualizationTrait.cpp:94–95`) |
| `UPROPERTY(VisibleAnywhere, Category = "StateHandoff") TObjectPtr<UStaticMeshComponent> Mesh;` | The only component in S1. `UPROPERTY` makes the garbage collector see the reference; `TObjectPtr` is the 5.x member-pointer type for UObjects; `VisibleAnywhere` shows it in the details panel but doesn't allow replacing it |
| `PrimaryActorTick.bCanEverTick = false;` | The actor never needs `Tick`; Mass drives everything. It also means "disabling" the actor (the engine turns the actor tick off, `MassRepresentationActorManagement.cpp:30–45`) changes nothing for it |
| `CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh")); RootComponent = Mesh;` | Creates the component as part of the class's default object, so every spawned or pooled instance has it. The root component is what Mass teleports |
| `SetCollisionEnabled(NoCollision)`, `SetGenerateOverlapEvents(false)`, `SetCanEverAffectNavigation(false)` | Nothing collides or navigates in this workload. Keeping physics and navigation out removes costs that aren't the subject and can't differ between configurations |
| `static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));` | Finds the engine cylinder once per class (`static`: the constructor runs again for every object, the lookup doesn't; `Source/Runtime/CoreUObject/Public/UObject/ConstructorHelpers.h:77`). A cylinder, so actors are easy to tell from the instanced cubes. Cooked through `DirectoriesToAlwaysCook` (B8) |

### B4. `SHMassProcessors.h` / `.cpp`: making Mass representation run for the demo

The engine ships the LOD collector, visualization-LOD and visualization processors **unregistered** (`MG/MassLOD/Private/MassLODCollectorProcessor.cpp:14`, `MG/MassRepresentation/Private/MassVisualizationLODProcessor.cpp:13`, `MassRepresentationProcessor.cpp:54`). A game subclasses each one, filters it to its own tag and turns registration on. The three classes copy MassCrowd's pattern exactly and add **no logic**: the switching under test is the engine's.

| Line | What it does, and why |
|---|---|
| `class USHLODCollectorProcessor : public UMassLODCollectorProcessor` | Computes each entity's distance and visibility to the viewers (the camera). Constructor: `bAutoRegisterWithProcessingPhases = true`, as MassCrowd's collector (`Plugins/AI/MassCrowd/Source/MassCrowd/Private/MassCrowdVisualizationLODProcessor.cpp:112–115`). The LOD collector trait checks that some collector subclass is registered and logs an error otherwise (`MG/MassLOD/Private/MassLODTrait.cpp:16–72, 86–95`) |
| Its `ConfigureQueries`: `Super::ConfigureQueries(EntityManager);` then four `AddTagRequirement<FSHDemoAgentTag>(EMassFragmentPresence::All)` | First let the engine build its four queries, then narrow each to entities that have the demo tag. `EMassFragmentPresence::All` = "must have it". Same four queries as MassCrowd (`MassCrowdVisualizationLODProcessor.cpp:117–125`) |
| `class USHVisualizationLODProcessor : public UMassVisualizationLODProcessor` | Turns distance and visibility into LOD High/Medium/Low/Off, and applies the per-LOD entity caps. Registration on; `ExecutionFlags = Client \| Standalone` (single-player, `01b #12`); the group (`LOD`, after `LODCollector`) comes from the engine constructor (`MassVisualizationLODProcessor.cpp:14–15`) |
| Its `ConfigureQueries`: the tag on `CloseEntityQuery`, `CloseEntityAdjustDistanceQuery`, `FarEntityQuery`, `DebugEntityQuery`; `FilterTag = FSHDemoAgentTag::StaticStruct();` | The engine processor only prepares and count-adjusts LOD settings whose `FilterTag` equals its own (`MassVisualizationLODProcessor.cpp:72, 98`); the director sets the same tag in the trait's LOD parameters (B5). Without matching tags the LOD would never be computed |
| `class USHVisualizationProcessor : public UMassVisualizationProcessor` | Switches the representation from the LOD: spawns, enables, disables and releases actors, and adds instanced meshes. `bRequiresGameThreadExecution = true` because it touches actors; group and order come from the engine (`MassRepresentationProcessor.cpp:56–57, 523`) |
| Its `ConfigureQueries`: `EntityQuery.AddTagRequirement<FSHDemoAgentTag>(…)` | The engine's base query, narrowed to the demo (`MassCrowdVisualizationProcessor.cpp:31–35`) |
| `namespace SHMassProcessors { constexpr int32 ExecutionFlags = … Client \| Standalone; }` | One constant shared by the two processors that set flags, so they can't drift apart. `constexpr`: a compile-time constant, no storage, no initialisation order |

*Not subclassed:* `UMassUpdateISMProcessor`, which writes the instanced-mesh transforms. It is auto-registered and has no tag filter (`MG/MassRepresentation/Private/MassUpdateISMProcessor.cpp:15–36`).

### B5. `SHBenchmarkSubsystem.h` / `.cpp`: the director

**Header.**

| Line | What it does, and why |
|---|---|
| `#include "Mass/EntityHandle.h"` | `FMassEntityHandle`, the entity identifier (index + serial number) stored in `Agents`. MassCore since 5.8 |
| `class USHBenchmarkSubsystem : public UTickableWorldSubsystem` | One director per game world, with the world's lifetime, ticking every frame. Its tick runs as a tickable object after the world's tick groups (`Source/Runtime/Engine/Private/LevelTick.cpp:1821`), so it sees the frame's Mass results. Same design and reasons as UE5-UpdateBudget's director |
| `virtual void Tick(float DeltaTime) override;` / `GetStatId()` | The two functions a tickable must provide. `GetStatId` names the tick in `stat` profiling |
| `const TArray<FMassEntityHandle>& GetAgents() const` | Read-only access for the S2 workload subsystem; `Agents[i]` is agent `i` |
| `DoesSupportWorldType(…)`: Game and PIE | The engine default also includes Editor worlds (`Source/Runtime/Engine/Private/Subsystems/WorldSubsystem.cpp:30–32`); a director spawning thousands of entities in the editor would be a bug |
| `struct FRunSettings { … }` | The command-line values, read once. Defaults: no agents (an empty map behaves normally), seed 1, warm-up 10 s, capture 60 s, a flip every 4 s, tag "run", don't quit, don't log counts at flips |
| `enum class ERunPhase : uint8 { Unread, Idle, WarmUp, Capture, Done }` | The run's timeline. `enum class`: scoped names that don't convert to `int` by accident. `uint8`: one byte. `Unread` exists because the settings are read on the first tick (below) |
| `StartRun`, `BuildEntityConfig`, `SpawnAgents`, `UpdateCamera`, `Teleport`, `LogRepresentationCounts` | One function per job (B5 .cpp). `StartRun` was split out of `Tick` in the S1 simplify pass, so `Tick` reads as the timeline only |
| `ScheduledFlips`, `Teleports`, `BounceFramesLeft`, `HotSpot` | Flip bookkeeping: scheduled flips done, all camera jumps (bounce returns included), frames until a bounce returns, current hot spot (0 west, 1 east) |
| `UPROPERTY(Transient) TObjectPtr<UMassEntityConfigAsset> EntityConfig;` | The code-built config. `UPROPERTY` keeps it, and the traits created inside it, alive for the garbage collector: the entity template refers to them. `Transient`: never saved |
| `UPROPERTY(Transient) TObjectPtr<ACameraActor> Camera;` | The LOD viewer (see `UpdateCamera`) |
| `TArray<FMassEntityHandle> Agents;` | Plain handles: they stay valid when entities change archetype, unlike fragment pointers (`07`) |

**Source.**

| Code | What it does, and why |
|---|---|
| `CSV_DEFINE_CATEGORY(SHBench, true);` | A CSV-profiler category for the run's events, on by default (`Source/Runtime/Core/Public/ProfilingDebugging/CsvProfiler.h:50`) |
| `namespace SHBench { … }` | Every tuning constant in one place, each with its reason. **Grid:** 4 m spacing, ±1 m jitter, agents at 0.5 m height (4,000 agents fit in a 250 m square). **Camera:** a 15 m orbit, 4 m up, one lap per 20 s, around a hot spot at ±50% of the arena half-width. **Bounces:** every 3rd flip returns after 10 frames, the quick-return case of H1 |
| `BaseLODDistance`, `VisibleLODDistance`, `LODMaxCount…` | The visualization-LOD settings: distances in cm where each LOD starts, for entities out of and in view, and how many entities each LOD may hold. **S1 starting values**; S3 calibrates them (≥ 20,000 transitions per correctness batch, SC1). The engine defaults allow about 150 actors (`MassRepresentationFragments.h:313`; `MassVisualizationTrait.cpp:46–49`) |
| `ReadRunSettings()` | `FParse::Value(CommandLine, TEXT("SHAgents="), …)` reads `-SHAgents=N` and leaves the default if absent; `FParse::Param(…, TEXT("SHExit"))` checks a flag. Command-line values, not cvars, so the run script sets everything in one place (the approach UE5-UpdateBudget chose after finding that cheat cvars can't be set from config) |
| `ValidateRunSettings()` | Returns `false`, with an **error** per problem, for settings that would make a run misleading: a warm-up under 1 s (frame 1 spawns every entity, and that hitch must stay out of the capture), no capture, or a flip period that isn't 0 or between 1 s and the capture length (a bounce must return well before the next flip). It also notes, at Display level, when there is no fixed time step (`FApp::UseFixedTimeStep()`, `Source/Runtime/Core/Public/Misc/App.h:557`), because only then do flips fall on the same frames in every run. **Changed after the S1 code review**, which found that the first version logged the errors and ran anyway |
| `BuildEntityConfig()`: `NewObject<UMassEntityConfigAsset>(this, TEXT("SHDemoEntityConfig"), RF_Transient)` | A config asset created in code, owned by the subsystem. Its constructor gives it a GUID derived from its path, so the template ID is stable (`MG/MassSpawner/Private/MassEntityConfigAsset.cpp:28–31`) |
| `Config.AddTrait(*NewObject<USHDemoStateTrait>(EntityConfig));` and the LOD collector trait | Traits are created as subobjects of the config, then added with the **runtime** `FMassEntityConfig::AddTrait` (`MassEntityConfigAsset.cpp:203–206`). The convenient `UMassEntityConfigAsset::AddTrait(TSubclassOf)` is editor-only (inside `#if WITH_EDITOR`, `:241–284`) and would not compile into the game |
| `UMassMovableVisualizationTrait* Visualization = …` | The 5.8 replacement for the soft-deprecated visualization trait (`MassVisualizationTrait.h:18–19`). `HighResTemplateActor` and `LowResTemplateActor` = `ASHDemoAgent` (B3). The default per-LOD representation is kept: High → high-res actor, Medium → low-res actor, Low → instanced mesh, Off → none (`MassVisualizationTrait.cpp:30–34`) |
| `LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"))` → `StaticMeshInstanceDesc.Meshes.AddDefaulted_GetRef()` | The instanced-mesh representation: the engine cube at 60% scale. A failed load is logged as an error, because the run would silently lack its ISM representation |
| The loops over `LODParams` and `LODParams.FilterTag = FSHDemoAgentTag::StaticStruct();` | Apply the `SHBench` distances and caps, and the tag that `USHVisualizationLODProcessor` filters on (B4) |
| `SpawnAgents()`: `EntityConfig->GetOrCreateEntityTemplate(World)` | Builds the archetype from the traits once (`MassEntityConfigAsset.cpp:66–93`), cached in the spawner's template registry |
| `TSharedPtr<FMassEntityManager::FEntityCreationContext> Creation = Spawner->SpawnEntities(…)` | Creates all N entities in one batch (`MG/MassSpawner/Public/MassSpawnerSubsystem.h:33`). The returned creation context is **kept** until the fragments are filled, then `Creation.Reset()`: the entities' creation notifications run when the context is released, so they see real transforms instead of zeros. `static_cast<uint32>`: the count parameter is unsigned, and `NumAgents > 0` is already known |
| The loop: grid column `Index % Side`, row `Index / Side`, jitter from `FRandomStream Random(Settings.Seed)` | Same seed → same positions. `FRandomStream` is a seeded generator independent of the global one, so nothing else in the engine can change the sequence |
| `GetFragmentDataChecked<FTransformFragment>(Agent).SetTransform(…)`, `…<FSHAgentIdFragment>(…).AgentIndex = Index` | Writes the fragments directly through the entity manager: allowed here because no Mass processing is running in the director's tick. "Checked" = asserts if the fragment is missing, which would be a template bug |
| `PositionHash = FCrc::TypeCrc32(FVector3f(Position), PositionHash);` | A running CRC of the start positions: the S1 determinism check compares it across runs. `FVector3f` hashes the float values, independent of how doubles print |
| `UpdateCamera()` | Orbits the current hot spot and looks at its centre. `SetViewTarget(Camera)` every frame: Mass's LOD subsystem takes the viewer from the player controller's view point (`MG/MassLOD/Private/MassLODSubsystem.cpp:376`), so this camera **is** the LOD viewer |
| `Teleport()` | Jumps to the other hot spot and fires `CSV_EVENT(SHBench, TEXT("BenchFlip"))` (`CsvProfiler.h:56`), so flip windows can be cut from the capture |
| `LogRepresentationCounts()` | Counts entities per `CurrentRepresentation` and entities holding an actor, by reading each agent's fragments (`GetFragmentDataPtr` returns null if missing). Logged on the last warm-up frame and the frame after `BenchEnd`, both outside the capture, and at each scheduled flip only with `-SHLogCounts` (the frame of the flip: the counts are still the previous frame's). A sanity check, not a metric |
| `Tick()`, first frame (`Phase == Unread`) → `StartRun()` | Settings are read and the entities spawned on the **first tick**, not in `OnWorldBeginPlay`: by then every subsystem, including Mass's, has had its `OnWorldBeginPlay`, so the order between subsystems can't matter. Invalid settings start no workload, and with `-SHExit` the game exits with code 3 (`FPlatformMisc::RequestExitWithStatus`, `Source/Runtime/Core/Public/GenericPlatform/GenericPlatformMisc.h:1096`), so a run script can never mistake a misconfigured run for a good one. `CSV_METADATA` records the run description, warm-up and capture lengths included, inside the CSV (`CsvProfiler.h:85`) |
| `Tick()`, `WarmUp` → `Capture` | `BenchStart` fires when the planned time is reached. All times are measured from that **planned** start, so the captured path doesn't depend on the warm-up length. The representation counts are logged on the **last warm-up frame** (`FromBenchStart + DeltaTime >= 0`), which is never captured |
| `Tick()`, capture | The end check comes **first**: a flip due on the `BenchEnd` frame would have no measured window, so it doesn't happen (found in the S1 code review: with a 60 s capture and 4 s flips, flip 15 used to fire on the `BenchEnd` frame). Then `FlipIndex = Floor(FromBenchStart / FlipEvery)`; when it passes `ScheduledFlips`, one scheduled flip happens; every third one sets `BounceFramesLeft = 10`, and ten frames later the camera jumps back. With a fixed time step the flips fall on the same frames in every run. Counts at flips only with `-SHLogCounts`: walking every agent on a captured frame would inflate exactly the flip windows being measured (also from the code review) |
| `Tick()`, `Done` | The frame after `BenchEnd` logs the representation counts: outside the capture, because timed runs pass `-csvStopOnEvent=BenchEnd` (`Source/Runtime/Core/Private/ProfilingDebugging/CsvProfiler.cpp:5090`). Three frames after `BenchEnd` (so the CSV file is written), `-SHExit` requests a normal exit once |

### B6. `StateHandoff.Build.cs` (changed in S1)

| Line | What it does, and why |
|---|---|
| Public: `MassEntity`, `MassCore`, `MassSpawner`, `MassLOD`, `MassRepresentation` | The rule from A3, applied header by header: `SHDemoTypes.h` includes a MassEntity header; `SHBenchmarkSubsystem.h` a MassCore one; `SHDemoStateTrait.h` the MassSpawner trait base; `SHMassProcessors.h` derives from MassLOD and MassRepresentation processors |
| Private: `MassActors` | Only `.cpp` files use `FMassActorFragment` |
| *Not yet:* `StateHandoffCore` | Nothing uses the plugin until the checker (S2) |

### B7. `Config\DefaultGame.ini` (changed in S1)

| Line | What it does, and why |
|---|---|
| `[/Script/UnrealEd.ProjectPackagingSettings]` `+DirectoriesToAlwaysCook=(Path="/Engine/BasicShapes")` | The cylinder (B3) and the cube (B5) are referenced only from C++, and no map references them, so the cook would leave them out of the packaged build. The `+` appends to the engine's list instead of replacing it |

### B8. `Tools\run_uncooked.bat` and `Tools\run_packaged.bat`

| Line | What it does, and why |
|---|---|
| `if "%SH_LOG%"=="" set "SH_LOG=Run"`; `set "LOG=…\Saved\Logs\%SH_LOG%.log"` | The log name comes from an environment variable. cmd splits `=` inside positional arguments (`%1`), so `-SHAgents=4000` can't follow a name argument; `%*` passes the whole argument string through unchanged. **Caveat (code review):** the variable stays set in the calling shell, so a caller sets it before **every** run, as the queue commands in `09` do |
| `UnrealEditor-Cmd.exe "%PROJECT%" -game -nullrhi -nosound -unattended -nosplash -traceautostart=0 -log -abslog="%LOG%" %*` | The uncooked game, without rendering, until the game quits itself (`-SHExit`). For functional checks only: the editor binary and `-nullrhi` are never benchmark conditions |

**`Tools\run_packaged.bat`** is the same for the packaged build: it starts the real executable `Build\Windows\StateHandoff\Binaries\Win64\StateHandoff.exe` (so the exit code is the game's), refuses with exit code 2 if packaging hasn't run, and passes every argument through (`-nullrhi` is given by the caller when wanted). It is for functional checks; timed runs go through `run_bench.py`, pinned to the performance cores (S2).

---

## Part C · S2a: the instrument, as plain C++

S2a builds the measuring instrument before anything it measures: the contract's base type, the checker that classifies a difference, and the demo's state, events and rules, all testable without a world. Nothing runs in the game yet (S2b wires it in).

### C1. Plugin `Public\SHContractTypes.h`: the contract's vocabulary

| Line | What it does, and why |
|---|---|
| `#include "Mass/EntityElementTypes.h"` | `FMassFragment`, from MassCore (`05` B1) |
| `USTRUCT() struct FSHChannelFragment : public FMassFragment` | The base of every **state channel**. Deriving from `FMassFragment` lets the same struct type be stored as an entity fragment; S4 also stores it as a component property. One type, two homes (`02` §4.1) |
| `UPROPERTY() uint32 Version = 0;` | Bumped by every write (`SHDemo::ApplyWrite`, C5). It turns "are these copies the same?" into three precise answers: version behind = **lost** writes, ahead = **duplicated**, equal but different = **corrupted**. `uint32`: 4 billion writes per copy is more than any run; it can't go negative. `UPROPERTY` so `CompareScriptStruct` and `CopyScriptStruct` (C3) include it |
| `UENUM() enum class ESHDivergence : uint8 { None, Lost, Duplicated, Corrupted, Count UMETA(Hidden) }` | The checker's classes. `UENUM` makes them reflected (printable by name in debuggers and Blueprint if ever needed); `Count` is the number of values, used to size arrays, and `UMETA(Hidden)` keeps it out of editor lists |

### C2. Plugin `Public\SHInvariantChecker.h`

| Line | What it does, and why |
|---|---|
| `#define UE_API STATEHANDOFFCORE_API` … `#undef UE_API` | 5.8's export pattern, as the Mass headers use (`MG/MassRepresentation/Public/MassRepresentationActorManagement.h:7, 140`): `STATEHANDOFFCORE_API` makes a function visible from other modules (the game module calls `Classify`). Defining a short local name and undefining it at the end keeps the macro from leaking into other headers |
| `class FSHInvariantChecker` | Plain C++, no `UObject`: no reflection or garbage collection needed, and the tests can create one on the stack |
| `static UE_API ESHDivergence Classify(const UScriptStruct& Type, const FSHChannelFragment& Expected, const FSHChannelFragment& Observed);` | `static`: classification needs no state. The caller passes the channel's reflected type, because the two references only show the base; the type lets the comparison see every property of the real struct. References, not pointers: both copies must exist |
| `void Record(const ESHDivergence Result) { check(Result < ESHDivergence::Count); ++Counts[…]; }` | Counts one comparison, `None` included, so the number of comparisons is known too. Inline in the header: called for every agent and channel every frame (about 16,000 times a frame at N = 4,000), so it must be inlinable. The `check` stops a `Count` value from writing one past the end of the array (code review) |
| `int64 GetCount(…) const` (same `check`), `UE_API int64 GetIncidents() const`, `UE_API void Reset()` | Read the counts; incidents = every class except `None`. `int64`: a long run makes millions of comparisons |
| `int64 Counts[static_cast<int32>(ESHDivergence::Count)] = {};` | One counter per class, zero-initialised (`= {}`) |

### C3. Plugin `Private\SHInvariantChecker.cpp`

| Code | What it does, and why |
|---|---|
| `checkf(Type.IsChildOf(FSHChannelFragment::StaticStruct()), …)` | A wrong type would make the property comparison read past the end of the structs. `checkf` stops at once with a message in development builds (it is compiled out of Shipping) |
| `if (Observed.Version < Expected.Version) return Lost;` / `> … return Duplicated;` | The version decides first: it is the history of writes, so a copy that missed a write is "lost" even if later writes happen to make its values match again |
| `return Type.CompareScriptStruct(&Expected, &Observed, PPF_None) ? None : Corrupted;` | Same number of writes: every property must be equal (`Source/Runtime/CoreUObject/Public/UObject/Class.h:2481`). The comparison is exact: each numeric property compares with `==` (`Source/Runtime/CoreUObject/Public/UObject/UnrealType.h:1760–1769`), so +0 equals −0 and a NaN never equals itself. That is right here: both copies ran the same writes in the same order, so a correct copy has the same values, and the demo's arithmetic (clamped adds and subtracts) never makes a NaN. (The first version of this text said "bit for bit"; corrected after the code review.) Taking the address of the base reference is the struct's address, because `FSHChannelFragment` is the first and only base of every channel (its base `FMassFragment` is empty) |
| `GetIncidents()` loops from `None + 1` to `Count`; `Reset()` zeroes every counter | Written out, so a new class added to the enum is included automatically |

### C4. Plugin `Private\Tests\SHTestTypes.h` and `SHInvariantCheckerTests.cpp`

| Code | What it does, and why |
|---|---|
| `USTRUCT() struct FSHTestChannel : public FSHChannelFragment { float Value = 100.f; int32 Slot = INDEX_NONE; }` | A channel shaped like the demo's (a float and an integer), so both property comparisons are exercised. In `Private\Tests`, so it is not part of the plugin's public API. The tests are compiled only with `WITH_DEV_AUTOMATION_TESTS`, but the struct itself is reflected in every build of the plugin (a small type, never instantiated outside the tests): accepted, and stated here after the code review |
| `#if WITH_DEV_AUTOMATION_TESTS` … `#endif` | Tests compile only in builds that support automation tests (Development editor and game, not Shipping) |
| `constexpr EAutomationTestFlags Flags = EditorContext \| ClientContext \| ProductFilter;` | Where the tests may run (editor and game) and exactly one filter, which the macro requires at compile time (`Source/Runtime/Core/Public/Misc/AutomationTest.h:4126–4133`) |
| `Write(Channel, Delta)`, `Classify(…)` returning `int32`, `AsInt(…)` | Helpers: one write the way the demo writes (value, then version); the classification as an integer, because `TestEqual` has no overload for an enum class (`AutomationTest.h:1985–2003`) |
| `IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHCheckerIdenticalTest, "StateHandoff.Checker.Identical", Flags)` and the four others | Each macro declares a test class and registers it under that name; `RunTest` returns `true` and reports failures through `TestEqual`/`TestTrue`. The cases: **Identical** (two defaults; the same ten writes give bit-identical floats); **Lost** (one write missing; a reset to defaults); **Duplicated** (one write too many); **Corrupted** (a different float, a different integer, same version); **Counts** (per class, incidents exclude `None`, `Reset`) |
| `const FSHTestChannel Defaults{};` | `{}` value-initialises the const object: a `const` object needs an initialiser |

### C5. Game `Public\SHDemoState.h` / `Private\SHDemoState.cpp`: the demo's state and rules

| Code | What it does, and why |
|---|---|
| `FSHHealthState { float Current = 100.f; float Max = 100.f; }` | Health, as an attribute set would hold it |
| `FSHCooldownState { double ReadyAtA, ReadyAtB; int32 Uses; }` | Two cooldowns as **absolute world times**, not "seconds left": an absolute time needs no simulation while the agent is dormant, so it is right in every representation. `double` because world time is `double` in UE 5. `Uses` records the decision outcome (each successful use) |
| `FSHMemoryState { FVector LastSeenPosition; double LastSeenTime = -1.0; uint8 Alert = 0; }` | What the AI remembers about the player. `-1` = never seen. `uint8` because the alert level is 0–3 |
| `FSHReservationState { int32 Slot = INDEX_NONE; }` | The interaction slot the agent believes it holds. The registry (below) holds the other side, so their agreement can be checked |
| `enum class ESHChannel : uint8 { Health, Cooldown, Memory, Reservation, Count }` | The channels as an index (arrays of checkers, names, CSV columns). Plain C++ enum: never needs reflection |
| `struct FSHAgentState { …four channels…; Get(ESHChannel) }` | One agent's expected state for the oracle. The non-const `Get` calls the const one and removes `const` with `const_cast`, the standard way to write the logic once for both |
| `ESHEventType { Damage, Heal, UseAbility, Perceive, Claim, Release }`, `ESHEventPoint { Simulation, Actor }`, `FSHEvent` | What happens to an agent, and where in the frame (`01` §6.1). An event carries every parameter any type needs; each type reads only its own |
| `struct FSHWrite` | The **decided** effect of an event on one channel: what both copies must apply. `Channel == Count` means "no change" (an ability not ready, nothing to release, no free slot), so no version is bumped |
| `class FSHSlotRegistry` | The shared slots, one holder per slot. `TakeFree(Start, Agent)` scans from `Start`, wrapping, for the first free slot; `Free(Slot, Agent)` frees only if `Agent` is the holder, so a stale release can't steal someone else's slot. World-level state: it never changes representation |
| `ESHChannel::Count` in `Get`, `GetChannelStruct` | "No channel" is never a valid argument: every caller filters it first (a write with `Channel == Count` is never applied, and loops stop before `Count`). The `check` in the `default:` branch catches a caller that forgets, in development builds; it is compiled out of Shipping, which this project never builds |
| `SHDemo::CooldownA = 2.0`, `CooldownB = 5.0`, `MaxAlert = 3` | The rules' constants, in one place |
| `Mix(uint32)` in an anonymous namespace | MurmurHash3's 32-bit finaliser (shifts and two multiplications), written out so the event sequence can't change with an engine version. The anonymous namespace makes it private to this file |
| `MakeEvent(Seed, Frame, Point, Agent, Rate, OutEvent)` | **Deterministic**: the event is a function of its arguments only, so every configuration sees exactly the same events. One chained hash decides "event or not" (its low 16 bits against `Rate × 65,536`) and the type; a second hash of the first gives the parameters, so type and amount aren't correlated |
| `Decide(Expected, Event, Agent, Now, Registry)` | Turns an event into a write, **using the oracle's state**: damage and heal always write; an ability writes only if `Now >= ReadyAt`; a claim only if the agent holds nothing and a slot is free (the registry is updated here, once); a release only if the agent holds a slot. Deciding once and applying the same write to both copies means a correct hand-off gives bit-identical copies, and a wrong one shows as a version or value difference |
| `ApplyWrite(Copy, Write)` | Applies one write to one copy and bumps its version: damage subtracts and clamps at 0, heal adds and clamps at `Max`, an ability sets `ReadyAt = Time + cooldown` and counts a use, perceive sets position, time and alert (capped), a reservation sets the slot. `static_cast<FSHHealthState&>(Copy)` turns the base reference back into the real type; safe because the write's channel says which type it is. `default: return;` = no version bump for "no change" |

### C6. Game `Private\Tests\SHDemoStateTests.cpp`

| Test | What it proves |
|---|---|
| `StateHandoff.Demo.EventsDeterministic` | 40,000 draws twice give identical events; the rate lands near 5% (bounds 4–6%, a wiring check, not a statistics test); rate 0 never fires, rate 1 always does |
| `…Health` | Damage, clamp at 0, heal, clamp at max; one version per write |
| `…Cooldown` | First use succeeds and sets `ReadyAt`; a second use too early writes nothing; the two abilities are independent; uses and versions count only successful writes |
| `…Reservation` | Claim takes the slot at the salt; a second claim while holding does nothing; release clears both sides; release with nothing held does nothing; a full registry refuses; only the holder can free a slot |
| `…CopiesAgree` | 2,000 frames of events decided once and applied to two copies: the checker finds them identical; a third copy that stops receiving writes half-way is `Lost` on every channel. This is the instrument's end-to-end property, without a world |

### C7. Build rules (changed in S2a)

| File | Change, and why |
|---|---|
| `StateHandoffCore.Build.cs` | Public `MassCore`: `SHContractTypes.h` (public) derives from MassCore's `FMassFragment`. The other Mass modules come with the first code that uses them (S4–S6) |
| `StateHandoff.Build.cs` | Public `StateHandoffCore`: `SHDemoState.h` (public) derives from the plugin's `FSHChannelFragment` |

### C8. `Tools\run_tests.bat`

| Line | What it does, and why |
|---|---|
| `UnrealEditor-Cmd.exe "%PROJECT%" -ExecCmds="Automation RunTests %FILTER%;Quit" -unattended -nullrhi -nosound -nosplash -nopause -log -abslog="%LOG%" -ReportExportPath="%REPORT%"` | Runs every test whose full name **contains** the filter (default `StateHandoff`; a plain argument is a substring match, and only a leading `^` or trailing `$` anchors it: `Source/Developer/AutomationController/Private/AutomationCommandline.cpp:211–227`, `Source/Developer/AutomationController/Public/AutomationControllerSettings.h:52–74`), headless, then quits. The default finds exactly this project's tests (`09`, S2a). A failing test sets `GIsCriticalError`, and the editor exits with −1 (`Source/Developer/AutomationController/Private/AutomationCommandline.cpp:493–504`), so the queue job fails visibly. The report goes to `Saved\Automation\Reports` (git-ignored) |
