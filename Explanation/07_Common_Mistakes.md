# 07 · Common Mistakes: StateHandoff

**Rule:** for every module, the mistakes that are easy to make here, **how to spot them, how to prevent them and how to debug them**. Mistakes the code reviews **actually catch in this project** are marked ⚠ caught. The module sections are filled in during Phase 5, step by step; the Mass-specific hazards found in Phase 2 are listed now, because the design is built around them.

---

## Foundation (Phase 4)

| Mistake | How to spot it | How to prevent it | How to debug it |
|---|---|---|---|
| Using a Mass type without enabling the **Experimental, off-by-default** MassGameplay plugin | `cannot open include file 'MassRepresentationFragments.h'`, or the module doesn't load | Enable it in the `.uproject` (and as a plugin dependency of `StateHandoffCore`) | Check `MassGameplay.uplugin:13` (`EnabledByDefault: false`) |
| Confusing a plugin dependency with a module dependency | The plugin is enabled but the include still fails | Plugin in the descriptor *and* module in `Build.cs` (`06` §2, §4) | UBT's error names the missing include path |
| Looking for `MassEntity.uplugin` | It doesn't exist in 5.8 | Depend on the engine modules `MassEntity`/`MassCore` instead | `01b` §4 |
| Leaving the default-enabled Android File Server plugin on | A `[/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings]` block with a random `SecurityToken` appears in `DefaultEngine.ini` after the editor runs | `"AndroidFileServer", "Enabled": false` in the `.uproject` (`05` A1) | `git diff Config/DefaultEngine.ini` before every commit; grep for `SecurityToken` |
| Passing command-line switches the engine doesn't parse (e.g. `-ExitAfterSeconds` in 5.8.4) | Nothing happens, silently | Grep the engine source for the switch before relying on it | Search `Source` and `Plugins` `.cpp` files for the switch name |
| No `DefaultInput.ini` | A start-up warning "does not have a valid PlayerInput object" from Enhanced Input | The two keys from Epic's template (`05` A10) | Grep the smoke log for `LogEnhancedInput` |
| A batch step written as `call step.bat \|\| exit /b` | The script can exit with 0 after a failed step under `cmd /c` (found by UE5-NavReadyPCG) | After each step: `if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%` (`05` A11, D9) | Read each step's own success line in the job log, never only the job's exit code (`09`) |
| A failing game check that exits normally (`RequestExitWithStatus(false, code)`) | Exit code 0 on Windows despite the failure | `RequestExitWithStatus(true, code)`: forced, keeps the code (`WindowsPlatformMisc.cpp:1493–1521`) | Compare the exit code with the log's error line |
| Starting the packaged **bootstrap** exe (or `cmd /c start /wait`) in a timed run | Exit code 0 even when the game failed | Start `Build\Windows\StateHandoff\Binaries\Win64\StateHandoff.exe` directly (`05` A11–A12) | Compare the exit code with the log's last lines |

## Game module: workload (S1)

| Mistake | How to spot it | How to prevent it | How to debug it |
|---|---|---|---|
| ⚠ caught: including a Mass header that 5.8 moved (`MassEntityElementTypes.h` → MassCore's `Mass/EntityElementTypes.h`) | Build warning C4996 "has moved to the MassCore module" (`MassEntityElementTypes.h:5–6`) | Include the MassCore header and depend on MassCore; read every warning, don't silence it | The warning names the new header |
| Not registering the visualization, visualization-LOD or LOD-collector processor | Entities exist but never get actors or instanced meshes; or the LOD collector trait logs "Using … trait while the required processor … is not enabled by default" (`MassLODTrait.cpp:64`) | Subclass all three with `bAutoRegisterWithProcessingPhases = true` (`05` B4) | The director's representation counts stay at `none` for every entity |
| A `FilterTag` on the LOD processor that differs from the trait's `LODParams.FilterTag` | The LOD is never computed: every entity stays at its initial LOD | Set both to the demo tag, together (`05` B4, B5) | Compare the two tags; `MassVisualizationLODProcessor.cpp:72, 98` is the comparison |
| A required fragment no trait adds (transform, actor fragment) | Template validation errors at spawn | The demo trait adds both (`05` B2) | The error names the missing type |
| Calling the editor-only `UMassEntityConfigAsset::AddTrait(TSubclassOf)` | Compiles in the editor target, fails in the game target | Use `FMassEntityConfig::AddTrait(Trait&)` on `NewObject`-made traits | Build the game target, not only the editor |
| Letting the code-built config be garbage-collected | Crash or invalid template after the next GC | Keep it in a `UPROPERTY` (`05` B5) | `obj refs` on the config; force a GC with `obj gc` |
| Releasing the spawn's creation context before filling the fragments | Creation notifications see default transforms | Hold the returned context until the fragments are written | Log the first frame's representation and positions |
| Spawning in `OnWorldBeginPlay` and depending on the Mass subsystems' own `OnWorldBeginPlay` having run | Works or fails depending on subsystem order | Spawn on the director's first tick | — |
| Assuming the default `LODMaxCount` | Only about 150 actors ever exist | Set the counts explicitly; calibrate (S3) | The representation counts in the log |
| A mesh referenced only from C++ | Invisible actors or cubes in the packaged build, a load warning in its log | `DirectoriesToAlwaysCook` (`05` B7) | Grep the packaged log for the asset path |

## StateHandoffCore and demo state: the instrument (S2a)

| Mistake | How to spot it | How to prevent it | How to debug it |
|---|---|---|---|
| Comparing two copies by value only, or with a tolerance | A lost write followed by a write that restores the value goes unseen | Compare the **version** first; values only when versions are equal (`05` C3) | The `Lost` test: a copy that missed one write |
| A channel member without `UPROPERTY` | `CompareScriptStruct` and `CopyScriptStruct` ignore it: corruption there is invisible, and a hand-off doesn't copy it | Every channel member is a `UPROPERTY` | Review each channel struct; a test that changes only that member must say `Corrupted` |
| Bumping the version outside the one write function, or forgetting it | False `Lost` or `Duplicated` everywhere | All writes go through `SHDemo::ApplyWrite`; "no change" writes nothing | The tests check versions after every write |
| ⚠ caught (S2b code review): a version counter **per copy** | After the checker re-syncs the oracle to one copy, a switch to the other copy compares unrelated counters: a stale copy shows as `Duplicated` | One sequence per agent and channel, stamped by each write and kept outside the channel structs (`05` C1, C5) | Test `StateHandoff.Demo.SwitchBack` |
| Deciding an event separately for each copy (oracle and live) | The copies diverge by design, because each decides from its own state | Decide **once**, on the oracle; apply the same write to both copies | The `CopiesAgree` test |
| Using `FMath::Rand` or an engine hash for events | The event sequence changes with other code's random calls or an engine version, so configurations see different events | The project's own hash of (seed, frame, point, agent) | The `EventsDeterministic` test |
| Storing "seconds remaining" for a cooldown | A dormant agent's cooldown stops counting, so it differs by representation | Absolute "ready at" world times (`05` C5) | Compare a cooldown before and after a long dormant period |
| `TestEqual` on an enum class | Compile error (no overload) | Compare as integers | — |

## The instrument in the world (S2b)

| Mistake | How to spot it | How to prevent it | How to debug it |
|---|---|---|---|
| Letting the checker read a different copy than the configuration's gameplay writes | Zero incidents for a configuration that loses state, or incidents for one that doesn't | One router function (`SHRouter::Authoritative`) used by the event points **and** the sweep (`05` D3) | Log the router's choice for one agent across a transition |
| Counting transitions from the system under test (for example from the hand-off's own bind/unbind events) | Incidents per 1,000 transitions differ between configurations for the same workload | The checker's own `FSHObservedFragment`, updated identically in every configuration (`05` D1) | Compare the transition counts of two configurations on the same seed: they must be close |
| Counting one root cause on every frame it lasts | Incident counts that grow with run length instead of with transitions | Re-sync the oracle after each divergence; count reservation invariants when they start (`05` D4) | The report's `frames_since_transition` column: one line per cause, not a run of lines |
| Storing a fragment pointer or a view across frames | Garbage values or a crash after an entity changes archetype | Build `FSHAgentView` per agent inside the chunk loop; never keep it (`04` §20) | Run with `-SHCheck=1` and many transitions; break where the stale pointer is read |
| Requesting a fragment `ReadOnly` and then needing a mutable actor | Compile error on `GetMutable()` through a const view, or a const_cast that hides a real write | Request `FMassActorFragment` `ReadWrite` and say why in a comment (`05` D5) | — |
| A processor that writes actor components off the game thread | Rare crashes or torn writes | `bRequiresGameThreadExecution = true` on both workload processors | — |
| Running a configuration whose rule doesn't exist yet | Numbers reported for a rule that silently fell back to B0 | `SHRouter::IsImplemented`; the workload refuses and the game exits with code 3 (`05` D4, D7) | `s2b_checks.bat` step S2b-6 |
| Counting the warm-up or the frames after `BenchEnd`, or missing the `BenchEnd` frame | Transition or incident totals that don't match the capture | `SetCapturing` on at `BenchStart`, off on the frame after `BenchEnd`: the director ticks before the FrameEnd sweep (`05` D7) | The report's totals vs the sum of the CSV's `SHWorkload/Transitions` column. ⚠ found in S2b: the CSV starts one frame **after** `BenchStart`, so the report had one frame more (`09`, S2b-7) |
| Refusing a run only after spawning its entities | A refused run leaves thousands of entities switching representation with no workload | Validate every setting, the configuration included, before spawning (`05` D7) | — |
| A trailing comment on a `UENUM` value that continues on the next line | The next value's tooltip shows the wrong text (the header tool reads the second line as that value's leading comment; seen in `SHContractTypes.gen.cpp`, `Count.ToolTip`) | One-line trailing comments; longer explanations in the enum's doc comment | Read the generated `.gen.cpp` metadata |
| Running the checker in a timing run | Timing results include the instrument's cost | `-SHCheck=0` for timing batches (`run_bench.py --check 0`) | The CSV metadata `SHCheck` of each capture |

## Mass representation hazards (from Phase 2, to be turned into tests)

| Mistake | How to spot it | How to prevent it | How to debug it |
|---|---|---|---|
| Handing state off in `BeginPlay` | The actor→entity lookup fails on the spawn frame | Hand off at the commit (`02` §2); never in `BeginPlay` | The mapping is set after spawn (`MassActorSpawnerSubsystem.cpp:334` vs `MassRepresentationActorManagement.cpp:108`) |
| Saving state only in `EndPlay` | Lost or stale state with pooling, or after a quick LOD return | Capture at the commit; keep `OnEndPlay` only as a safety net | Toggle `mass.spawning.actorpooling`; watch the checker's *lost* count |
| Treating a "disabled" Mass actor as gone | Its components keep ticking and change their copy | The entity owns state while the actor is disabled; restore on re-enable | `Actor.cpp:1754–1760`; break in a component tick while the entity is on ISM |
| Trusting `OnPostActorSpawn` as "the actor is back" | A kept actor re-enabled without a spawn keeps stale state | Detect binding from `CurrentRepresentation` + the actor fragment | `MassRepresentationProcessor.cpp:256–264` |
| Holding a fragment reference across a release | Crash or garbage after the entity moves archetype | Store handles; re-read fragments | Engine warning `MassRepresentationActorManagement.h:115–116` |
| Expecting identical frames under `-deterministic` | Transition frames differ run to run | Judge per transition; normalise per 1,000 transitions | Spawn/destroy budgets are wall-clock (`01b #8`) |
| Using the soft-deprecated `UMassVisualizationTrait`, or forgetting to register the visualization processors | No representation switching at all | `UMassMovableVisualizationTrait` + MassCrowd-style processor subclasses | `01b #13` |
