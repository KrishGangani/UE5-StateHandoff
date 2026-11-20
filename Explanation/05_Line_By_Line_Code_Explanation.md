# 05 · Line By Line Code Explanation: StateHandoff

**Rule:** every file that exists in `StateHandoff\` and `Tools\` is explained here, line by line, in the same step it is written. Sections follow the build order in [03_Implementation_Roadmap.md](03_Implementation_Roadmap.md). Engine references are relative to `C:\Program Files\Epic Games\UE_5.8\Engine` (5.8.4); `Templates\…` is relative to `C:\Program Files\Epic Games\UE_5.8`.

| Part | Step | Files |
|---|---|---|
| A | Phase 4, F1–F4 (foundation) | `.uproject`, two `Target.cs`, two `Build.cs`, two module `.cpp`, `.uplugin`, three config `.ini`, `Tools\*.bat`, `Tools\cpu_affinity.py` |
| B | S1 onwards | written with the code (Phase 5) |

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
| The comment "Phase 4: only the engine basics" | The Mass modules and `StateHandoffCore` are **not** listed yet: no code includes their headers. Adding them with the first code keeps the list honest. Enabling the MassGameplay *plugin* in the `.uproject` is enough to build, link and package its modules now (UBT adds every module of an enabled plugin that compiles in the current configuration to the target, `Source/Programs/UnrealBuildTool/Configuration/UEBuildTarget.cs:5164–5193`), which is what the Phase 4 check needs |
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
| `Core, CoreUObject, Engine` (public) | The plugin's public headers will expose engine and Mass types (a trait, a subsystem, a processor), so its dependencies are public. The comment lists the Mass modules that arrive later and when (`02` §3) |

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
