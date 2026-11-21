# 09 · Testing And Benchmarks: StateHandoff

**Rule:** every check is recorded with its date, its exact command and its result, **including failures**. The functional and benchmark sections are filled in during Phases 5–8; results from earlier phases are recorded as they happen. Every Unreal job runs through the machine-wide queue (`tools\ue_queue.py` at the portfolio root), one at a time.

---

## Phase 4 · Foundation checks (2026-11-21)

One queue job ran all five checks in order (`Tools\phase4_checks.bat`), so the project waits in the shared queue once:

```text
python -I -u ue_queue.py run --project P09 --label "P09 Phase 4: …" --timeout 5400 --idle 1500 -- "…\P09_StateHandoff\Tools\phase4_checks.bat"
```

| # | Attempt | Result |
|---|---|---|
| Q-1 | First submission, queued 01:28:59 | ❌ **Never ran.** After about 25 minutes in line, the shared queue tool raised `PermissionError: [WinError 32] … tickets\…_22900.json` inside `acquire()` (`os.remove` of the ticket while another process had it open) and exited with code 1. No Unreal process was started. The tool lives outside this repository, so the fix is reported to the portfolio owner; the job was **re-submitted once**, as the queue rules allow |
| Q-2 | Re-submission, queued 01:51:07, ran 02:47:02–02:50:03 (job `20261121_015107_038d98`). Same command; meanwhile `AndroidFileServer` was disabled and `-ExitAfterSeconds` dropped (`05` A1, A11) | ✅ `ok (exit 0) in 181.0 s`; the script's last line: "Phase 4 checks: all steps exited with code 0" |

| # | Check | Command (from `P09_StateHandoff\`) | Result |
|---|---|---|---|
| F-1 | Editor target builds | `Tools\build.bat StateHandoffEditor Development` | ✅ `Result: Succeeded` in 61.7 s; 12 actions (both modules compiled and linked into `UnrealEditor-StateHandoff.dll` and `UnrealEditor-StateHandoffCore.dll`); **0 errors, 0 warnings**. Toolchain: MSVC 14.44.35229, Windows SDK 10.0.22621.0 |
| F-2 | Game target builds | `Tools\build.bat StateHandoff Development` | ✅ `Result: Succeeded` in 35.1 s; 24 actions, ending in `Link [x64] StateHandoff.exe` (the monolithic game, with the enabled Mass plugins' precompiled modules); **0 errors, 0 warnings** |
| F-3 | Uncooked game loads the map and quits | `Tools\smoke_game.bat` (`UnrealEditor-Cmd -game -nullrhi … -ExecCmds="quit"`) | ✅ Exit 0. The plugins mount: `StateHandoffCore`, `MassGameplay`, and its dependencies `PoseSearch`, `SmartObjects`, `StateTree`, `ZoneGraph`, `ZoneGraphAnnotations` (plus `MassInsights`, which is enabled by default). `Template_Default` loads in 0.047 s, then `RequestExit(0)`. **0 errors.** 21 warnings, **all** `LogEditorDataStorageUI` ("Unable to register widget factory …"): engine-side, from the editor binary running with `-game`; none from this project's code or from Mass. `AndroidFileServer` is not mounted |
| F-4 | **Launcher-engine check:** package Development with MassGameplay enabled | `Tools\package.bat Development` | ✅ `BUILD SUCCESSFUL`, `BuildCookRun time: 63.68 s`; cook commandlet "Success - 0 error(s), 0 warning(s)"; 516 packages cooked (7 skipped by platform). Output: `Build\Windows\StateHandoff\Binaries\Win64\StateHandoff.exe` (342 MB exe, about 1 GB staged in total, git-ignored). **Conclusion: the launcher engine can build, cook and package the Experimental MassGameplay plugin and its dependencies; no source-built engine is needed** |
| F-5 | Packaged game (real exe, not the bootstrap) loads the map and quits | `Tools\smoke_packaged.bat` (`-nullrhi`) | ✅ Exit 0. Pak and IoStore containers mount; the same plugins mount as in F-3 (MassInsights is not packaged: its modules are for the Insights program only, `01b` M-1); `Template_Default` loads in 0.018 s; `LogExit: Exiting.` **0 errors, 0 warnings** |

**Audit after the job:** `git status` shows no change to `Config\DefaultEngine.ini` and no `AndroidFileServerRuntimeSettings`/`SecurityToken` block (the editor binary ran in F-3 and F-4's cook).

**What Phase 4 established for later steps:**
1. The Mass plugins are usable from the launcher engine for both the editor and packaged Development builds, so the architecture needs no engine change (`02` §3).
2. The uncooked `-game` run always carries the engine's 21 `LogEditorDataStorageUI` warnings. Later smoke checks therefore count warnings **per category**, and only the project's own categories (and Mass's) must stay at zero. The packaged build has none.
3. Packaging the whole project takes about 1 minute once the game target is built; the full Phase 4 job took 3 minutes of machine time, plus about 80 minutes of waiting in the shared queue.

### Checks outside the queue (no Unreal process)

| # | Check | Command | Result |
|---|---|---|---|
| T-1 | Performance-core mask on this machine | `python -I Tools\cpu_affinity.py` | ✅ `0xFFFF` (logical CPUs 0–15), 2026-11-21 |
| T-2 | `pin_self` pins the script, and a child inherits the mask | `pin_self(performance_core_mask())`, then `applied_mask()`, then a child PowerShell printing its own `ProcessorAffinity` | ✅ before `0xFFFFFF (all logical processors)`; after `0xFFFF (performance cores only)`; child `65535` (= `0xFFFF`), 2026-11-21 |
| T-3 | Links stay inside the project folder | `python -I tools\check_links.py P09_StateHandoff` (portfolio root) | ✅ 0 problems before each commit (the count is in the commit's audit) |

## Step S1 · Workload skeleton checks (2026-11-21)

**Job 1** `20261121_030815_421625` (43.5 s): editor build, then three uncooked runs (`Tools\run_uncooked.bat`, editor binary, `-game -nullrhi`) with `-SHAgents=4000 -SHWarmup=2 -SHCapture=16 -SHFlipEvery=4 -SHExit -deterministic -FPS=60`, seeds 7, 7 and 8. These are **functional checks, not measurements**.

| # | Check | Result |
|---|---|---|
| S1-a | **Build** | ⚠ `Result: Succeeded`, but **4 × warning C4996**: `MassEntityElementTypes.h` "has moved to the MassCore module and been renamed to Mass/EntityElementTypes.h" (`Source/Runtime/MassEntity/Public/MassEntityElementTypes.h:5–6`). Fixed (`05` B1); the warning-free build is S1-f |
| S1-b | **Determinism of placement** | ✅ Seed 7 gave start-position hash `0x0B07F62C` in both runs; seed 8 gave `0x776DED0C` |
| S1-c | **Representation switching happens** | ✅ At every flip, about 100 high-res actors, about 290 low-res actors, 900–1,080 instanced cubes, about 2,550–2,700 entities with no representation (N = 4,000). The counts change at every flip |
| S1-d | **Kept-disabled actors exist (the H2 premise, `01b #5`)** | ✅ More entities **hold an actor** (451–565) than are **in an actor representation** (about 390): for example 565 vs 394 at flip 3. The difference is entities on an instanced cube or none whose actor the engine keeps, disabled |
| S1-e | **Repeatability of the representation sequence** | ✅ The two seed-7 runs logged identical counts at every flip. (On this machine, in these short uncooked runs; the wall-clock spawn and destroy budgets, `01b #8`, can still make runs differ, so the checker never relies on it) |
| S1-g | **Timeline** | ✅ `BenchStart` at 2.017 s, `BenchEnd` after 16.000 s of capture; 4 scheduled flips and 5 camera jumps (flip 3 is a bounce); exit code 0 |
| S1-h | **Errors** | ✅ 0 errors. 21 warnings per run, **all** `LogEditorDataStorageUI` (the editor binary in `-game`, as in Phase 4 F-3); none from Mass or this project |

**Audit trail for S1:**
- `code-review` (medium, Skill tool, on the staged diff): **10 findings, 3 important and 7 nits, no Mass API misuse** (the review checked processor subclassing, `FilterTag`, which trait adds which fragment, `FMassEntityConfig::AddTrait`, `SpawnEntities` and the CSV start/stop events against the engine source). Fixed:
  1. *important:* representation counts walked every agent on captured flip frames, inflating the flip windows → logged only outside the capture, or with `-SHLogCounts`;
  2. *important:* with capture a multiple of the flip period, a flip fired on the `BenchEnd` frame → the end check comes first;
  3. *important:* the docs claimed repeatable flip frames without saying a fixed step is required → comments and `05` corrected; a Display note when there is no fixed step;
  4. invalid settings were logged and the run continued → no workload, exit code 3 with `-SHExit`;
  5. warm-up 0 put the spawn frame inside the capture → warm-up ≥ 1 s;
  6. a bounce could cancel the next flip at very short periods → flip period 0 or ≥ 1 s;
  7. the header said the camera flips during warm-up → corrected;
  8. warm-up and capture lengths were missing from the CSV metadata → added;
  9. the exit-delay comment didn't say `-csvStopOnEvent=BenchEnd` is what ends the capture → corrected;
  10. `SH_LOG` stays set in the caller → documented in the scripts and `05` B8 (callers set it before every run).
- `simplify`: the skill launches four parallel sub-agents, which this project's brief doesn't allow, so its four angles (reuse, simplification, efficiency, altitude) were applied by hand to the same diff. One change: the first-frame block of `Tick` became `StartRun()`. Reviewed and not changed: the grid and CRC code that mirrors UE5-UpdateBudget (copied, not shared, because each repository stands alone); entity-manager lookups at spawn (once, outside the capture).

**Job 2** (verification after the fixes): editor and game builds, Development package, one packaged run. See the table below.

| # | Check | Result |
|---|---|---|
| S1-f | Editor and game builds after the fixes: 0 errors, 0 warnings | ✅ Job `20261121_032455_ee5079` (95.3 s): editor `Result: Succeeded` (11.9 s, 7 actions), game `Result: Succeeded` (19.2 s, 9 actions, monolithic `StateHandoff.exe`); **0 errors, 0 warnings**: the C4996 warning of S1-a is gone |
| S1-i | Package Development: the cook includes `/Engine/BasicShapes` | ✅ `BUILD SUCCESSFUL` (60.1 s); 519 packages cooked (Phase 4: 516; the extra three are the basic shapes), cook "Success - 0 error(s), 0 warning(s)" |
| S1-j | Packaged run (`-nullrhi`, N = 1,000, seed 7, 6 s capture, flips every 2 s): spawns, switches representations, no missing-asset warnings, exit 0 | ✅ Exit 0. Hash `0x1D1A1588`. End of warm-up: 104 high-res + 252 low-res actors, 562 cubes, 82 none, 387 entities holding an actor; at `BenchEnd`: 363 actors, 495 cubes, 142 none, 591 holding an actor. **2 scheduled flips** in the 6 s capture: the flip due at 6 s, on the `BenchEnd` frame, is skipped (the code-review fix). **0 `Warning:` and 0 `Error:` lines**; the only "Failed to load" lines are the engine probing for optional profiler DLLs at Display level |

## Step S2a · The instrument as plain C++ (2026-11-21)

**Job 1** `20261121_035727_39b841` (50.5 s): editor build, game build, then `Tools\run_tests.bat StateHandoff` (headless editor, `Automation RunTests StateHandoff`).

| # | Check | Result |
|---|---|---|
| S2a-a | **Editor build** | ✅ `Result: Succeeded` (16.5 s, 17 actions: the plugin's first code, the game module); **0 errors, 0 warnings** |
| S2a-b | **Game build** | ✅ `Result: Succeeded` (17.6 s, 13 actions, monolithic `StateHandoff.exe`); **0 errors, 0 warnings** |
| S2a-c | **Automation tests** | ✅ "Found 10 automation tests based on 'StateHandoff'": `StateHandoff.Checker.{Corrupted, Counts, Duplicated, Identical, Lost}` and `StateHandoff.Demo.{Cooldown, CopiesAgree, EventsDeterministic, Health, Reservation}`, every one `Result={Success}`; "TEST COMPLETE. EXIT CODE: 0". `Tests.log`: 0 `Warning:` and 0 `Error:` lines |

**Audit trail for S2a:**
- `code-review` (medium, Skill tool, on the staged diff): **7 findings, all nits**, none blocking; the reviewer also checked the MassCore include, the test runner's exit code on failure, the build rules and the event-rate edge cases against the engine source. All fixed:
  1. `Record` and `GetCount` would index one past the end with `ESHDivergence::Count` → a `check` in both;
  2. the checker's comment and `05` C3 said floats compare "bit for bit"; numeric properties compare with `==` (`UnrealType.h:1760–1769`: +0 equals −0, NaN never equals itself) → comment and docs corrected;
  3. `ESHChannel::Count` is mapped to Reservation in Shipping, where the `check` is compiled out → documented in `05` C5 (every caller filters it; this project never builds Shipping);
  4. the test struct `FSHTestChannel` is reflected in every build, not only with tests → accepted and stated in `05` C4;
  5. `04` §14 named "transition helpers" that S2a doesn't have → removed (S2b adds them);
  6. `05` C8 and `06` §17 said `Automation RunTests` matches a prefix; a plain filter is a **substring** match (`AutomationCommandline.cpp:211–227`, `AutomationControllerSettings.h:52–74`) → docs corrected; the default filter still finds exactly this project's 10 tests (S2a-c);
  7. two different line ranges cited for the automation-flags rule → `AutomationTest.h:4126–4133` everywhere (application flag at 4126, one filter at 4127–4133).
- `simplify`: applied by hand (the skill starts sub-agents, which this project's brief doesn't allow), on the same diff, for reuse, simplification, efficiency and altitude. **No change.** Reviewed: the four `switch`es over `ESHChannel` (kept: each maps the enum to a different kind of thing, and a table would hide the `check`); the per-property comparison (about 16,000 reflection compares per frame at N = 4,000 when the checker is on: acceptable, because correctness runs and timing runs are separate batches, `Benchmarks/README.md` rule 6); the checker in the plugin and the demo rules in the game (the right altitude: the checker knows nothing about the demo).
- `02` updated in the same step: the demo-state row and §14 record that an event is decided once (on the oracle) and its write applied to each copy.

**Job 2** (verification after the review fixes): editor and game builds, the automation tests.

| # | Check | Result |
|---|---|---|
| S2a-d | Both builds after the fixes: 0 errors, 0 warnings; the 10 tests pass | ✅ Job `20261121_042452_b8c1e9` (47.9 s): editor `Result: Succeeded` (11.3 s, 8 actions), game `Result: Succeeded` (17.1 s, 5 actions); **0 errors, 0 warnings**. "Found 10 automation tests based on 'StateHandoff'", 10 × `Result={Success}`, "TEST COMPLETE. EXIT CODE: 0"; `Tests.log`: 0 `Warning:`, 0 `Error:` |

## Step S2b · The instrument in the world (2026-11-21)

**Job 1** `20261121_052114_dd421a`: `Tools\s2b_checks.bat`. ❌ **Failed at S2b-1**, exit code 6: `SHMassProcessors.cpp(12,18): error C2374` / `C2086`, `SHMassProcessors::ExecutionFlags` redefined. The review fix that moved the constant into the header (fix 5 below) left the old definition in the `.cpp`: the edit script treated an empty replacement as "already applied". Every other file compiled without a diagnostic. The failure also showed the new step check working: the build's exit code 6 came through as the job's exit code. Fixed by removing the duplicate.

**Job 2** `20261121_061648_b401eb` (211.5 s): the same script, which stops at the first failure. S2b-7 to S2b-9 are small `run_bench.py` batches (one B0 run each, N = 4,000, seed 1, 10 s warm-up, 30 s capture, packaged Development, pinned to `0xFFFF`, read back by `applied_mask()`). They are **functional checks of the instrument, not measurements**.

| # | Check | Result |
|---|---|---|
| S2b-1 | Editor build: 0 errors, 0 warnings | ✅ `Result: Succeeded` (13.3 s) |
| S2b-2 | Game build: 0 errors, 0 warnings | ✅ `Result: Succeeded` (23.6 s, monolithic `StateHandoff.exe`) |
| S2b-3 | Automation tests `StateHandoff.*` | ✅ "Found 11 automation tests based on 'StateHandoff'" (S2a's 10 plus `StateHandoff.Demo.SwitchBack`), 11 × `Result={Success}`, "TEST COMPLETE. EXIT CODE: 0"; `Tests.log` 0 `Warning:`, 0 `Error:` |
| S2b-4 | `csv_stats.py --selftest` | ✅ "selftest: OK (percentiles, summary and event parsing match hand-worked values)", flip-window cases included |
| S2b-5 | Package Development: cook without errors or warnings | ✅ cook "Success - 0 error(s), 0 warning(s)", `BUILD SUCCESSFUL` |
| S2b-6 | `-SHConfig=SH` (not implemented until S5) is refused: exit code 3, an error, nothing spawned | ✅ `S2b_RefuseSH.log`: `LogSHBench: Error: -SHConfig=SH is not implemented yet.` on frame 0, then `RequestExitWithStatus(1, 3, …)` (forced); the step's exit-code test passed. No entity was spawned (no "Spawned" line) |
| S2b-7 | **B0 must show incidents** (orbit + flips, checker on). Batch `2026-11-21_070851_s2b_orbit` | ✅ **27,464 incidents in 30,107 transitions (912.2 per 1,000)**, `--expect-incidents some` met. Exit 0, 0 `Error:`, 0 `Warning:` in the game log. Details below |
| S2b-8 | **No false positives**: camera parked, no flips, checker on: 0 incidents. Batch `2026-11-21_070949_s2b_park` | ✅ **0 transitions, 0 incidents** over 1,801 frames with **196,419 writes applied** and 7,204,000 comparisons per channel (every agent, every frame). Exit 0, 0 `Error:`, 0 `Warning:`. 408 agents held an actor on every captured frame (`SHWorkload/AgentsWithActor` = `MassActors/NumSpawned` = 408 throughout) |
| S2b-9 | Checker off (timing mode): the run completes with its capture and report. Batch `2026-11-21_071008_s2b_nocheck` | ✅ Exit 0; capture finalised; report written: 30,105 transitions, 0 incidents (none counted: the sweep is off), 196,976 events |

**What the B0 run shows (S2b-7, `…_r1_incidents.csv`).** The checker, not a measurement batch, so one run and no statistics:
- **Transitions by kind** (capture only): NoneToMesh 12,149 · MeshToNone 9,381 · MeshToActor 3,870 · ActorToNone 2,765 · ActorToMesh 1,110 · ActorSwap 832 · NoneToActor 0.
- **Incidents by channel and class:** Health lost 6,637 / duplicated 436 / corrupted 47; Cooldown 6,020 / 937 / 18; Memory 6,046 / 897 / 33; Reservation 3,650 / 1,692 / 0; slots leaked 1,047; slots double-held 4.
- **When:** 27,090 of the 27,464 incidents (98.6%) are on the frame of the agent's latest transition (`frames_since_transition = 0`). By that transition: MeshToActor 13,123 (a new actor starts with default state), ActorToNone 10,691 (the actor is released and the game falls back to the stale entity copy), MeshToNone 3,276 (a kept, disabled actor released), NoneToMesh 374. This is H1's mechanism, and it shows the instrument attributes incidents to transitions; the numbers themselves are for S3's calibrated baseline capture.
- **Consistency with the CSV:** the report and the CSV's per-frame sums agree on incidents (27,464 both). The CSV has 1,800 rows to the report's 1,801 frames, and 7 fewer transitions and 103 fewer events: the CSV capture starts on the frame **after** `BenchStart` (no `BenchStart` event in the file; `BenchEnd` is in its last row), while the checker counts from the `BenchStart` frame (`05` D7). Recorded, and the checker's start moves one frame later in S3. Three code comments that claimed identical windows were corrected after job 2, and the `ESHDivergence` value comments were moved into the enum's doc comment, because the generated code showed that the header tool had attached the second line of a two-line trailing comment to the next value's metadata (`SHContractTypes.gen.cpp`: `Count.ToolTip`). These are comments only; the code is unchanged.
- **Observations, not results:** the priming run's FrameTime median was 5.03 ms against 3.76 ms in the measured run (the priming rule at work); the run took 13 s of wall time for 40 s of game time (`-deterministic -FPS=60` advances a fixed 1/60 s per frame, without a frame-rate cap). Events applied differ between the three batches (190,103 · 196,419 · 196,976): every run generates the same events, but the decided writes depend on the oracle, which the correctness run re-syncs, and claims depend on which slots are free when an agent is reached, so possibly on chunk order **[inferred]**. To check in S3 before the baselines: whether claim outcomes depend on processing order, and if so, make them independent of it.

**Audit trail for S2b:**
- `code-review` (medium, Skill tool, on the staged diff, **before** the build, so one queue job verifies the fixed code): **7 findings, 1 important and 6 nits**; the reviewer also checked the tick order, FrameEnd in `TG_LastDemotable`, the `Representation` group name, the iterator-to-index conversion and the default execution flags against the engine source. All fixed:
  1. *important:* versions were per-copy counters, and the re-sync copied one copy's counter into the oracle; after a switch back to the other copy, a stale entity copy was classified *duplicated* → `Version` is now the write's sequence number, one numbering per agent and channel (`FSHAgentState::Issued`, outside the channel structs), stamped by `ApplyWrite`; new test `StateHandoff.Demo.SwitchBack`; `02` §14 records the change (this touches S2a's code, with its docs: `05` C1, C5, C6);
  2. an unimplemented configuration was refused only after N entities were spawned → refused in `ValidateRunSettings`, before spawning;
  3. reservation incidents had no transition → `LastObserved` per agent; one `AddIncident` for every incident;
  4. the `ESHEventPoint` comment still said actor events need an actor → corrected;
  5. the workload processors kept the default `Server | Standalone` flags while the representation processors use `Client | Standalone` → one shared constant, moved to `SHMassProcessors.h`;
  6. the report's frames and events counted the whole run, and counting stopped one frame before the CSV capture ends → capture-only counts; counting stops on the frame after `BenchEnd`;
  7. an agent that never changed representation showed the whole run as "frames since transition" → empty field.
- `simplify`: applied by hand (the skill starts sub-agents, which this project's brief doesn't allow). Changes: the reservation incidents now share `AddIncident` (fix 3). Reviewed and not changed: the three four-way `switch`es from `ESHChannel` to a member (`FSHAgentState`, `USHAgentStateComponent`, `FSHAgentView`: different owner types, and a template would hide the `check`); `FindComponentByClass` per agent per pass (about two lookups per agent with an actor per frame, on a two-component actor; S4's hand-off caches the component per binding); `IsImplemented` checked by both the director and `BeginRun` (the second is a cheap guard for any other caller).
- **Batch exit codes (coordinator, AGENT_BRIEF rule 12, found by UE5-NavReadyPCG).** `phase4_checks.bat` and the new `s2b_checks.bat` used `call step.bat || exit /b N`, which can exit with 0 after a failure under `cmd /c`. Both now test `if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%` after every step, and the director's failing exits are forced (`RequestExitWithStatus(true, 3, …)`), because a normal exit loses the code on Windows (`WindowsPlatformMisc.cpp:1493–1521`). **Every earlier P09 result was re-checked from each step's own log line, not from a job's exit code: no false pass.**

  | Job | Step evidence in its own log |
  |---|---|
  | Phase 4 `20261121_015107_038d98` | F-1 and F-2 `Result: Succeeded`; F-3 `SmokeGame.log` ends with `RequestExit(0)`, `LogExit: Exiting.`, 0 `Error:`; F-4 cook "Success - 0 error(s), 0 warning(s)", `BUILD SUCCESSFUL`, `AutomationTool exiting with ExitCode=0`; F-5 `SmokePackaged.log` ends the same way as F-3 |
  | S1 `20261121_030815_421625` | `Result: Succeeded` (the build with the four C4996 warnings, as recorded in S1-a); `S1_seed7_a/b.log`, `S1_seed8.log`: `RequestExit(0, USHBenchmarkSubsystem)`, `LogExit: Exiting.`, 0 `Error:` |
  | S1 `20261121_032455_ee5079` | Editor, game and the package's own build: 3 × `Result: Succeeded`; `BUILD SUCCESSFUL`; `S1_packaged.log` ends with `RequestExit(0, …)`, 0 `Error:` |
  | S2a `20261121_035727_39b841`, `20261121_042452_b8c1e9` | 2 × `Result: Succeeded` each; `Tests.log` "TEST COMPLETE. EXIT CODE: 0", 10 × `Result={Success}` |

  (The "fatal" and "crash" words in those logs are the crash reporter's start-up lines, `Session CrashGUID` and "Custom abort handler registered", not failures.)

## Step S3a · The baselines B1, B2, B3 and the workload switches (2026-11-21)

**Job** `20261121_072916_4695ff` (357.0 s): `Tools\s3a_checks.bat`, which stops at the first failure. S3a-6 to S3a-10 are small `run_bench.py` batches (one run per configuration, N = 4,000, seed 1, 10 s warm-up, 30 s capture, flips every 4 s, checker on, packaged Development, pinned to `0xFFFF`). They are **functional checks of the baselines, not the baseline capture** (that is S3b, with full runs, rotation and statistics).

| # | Check | Result |
|---|---|---|
| S3a-1 | Editor build: 0 errors, 0 warnings | ✅ `Result: Succeeded` (20.2 s) |
| S3a-2 | Game build: 0 errors, 0 warnings | ✅ `Result: Succeeded` (33.9 s) |
| S3a-3 | Automation tests `StateHandoff.*` | ✅ "Found 12 automation tests based on 'StateHandoff'" (S2b's 11 plus `StateHandoff.Router.Rules`), 12 × `Result={Success}`, "TEST COMPLETE. EXIT CODE: 0"; `Tests.log` 0 `Warning:`, 0 `Error:` |
| S3a-4 | `csv_stats.py --selftest` | ✅ OK |
| S3a-5 | Package Development | ✅ cook "Success - 0 error(s), 0 warning(s)", `BUILD SUCCESSFUL` |
| S3a-6 | **B3 (entity-owned) shows no incidents**, pooling off. Batch `2026-11-21_084528_s3a_b3p0` | ✅ **0 incidents in 30,096 transitions** (priming run: 0 in 30,094) |
| S3a-7 | **B3 shows no incidents**, pooling on. Batch `2026-11-21_084606_s3a_b3p1` | ✅ **0 incidents in 30,117 transitions** |
| S3a-8 | B0, B1, B2, pooling off. Batch `2026-11-21_084626_s3a_p0` | ✅ all runs exit 0, 0 `Error:`, B1/B2 baselines made copies (table below) |
| S3a-9 | B0, B1, B2, pooling on. Batch `2026-11-21_084721_s3a_p1` | ✅ as S3a-8 |
| S3a-10 | B0, B1, B2, pooling on, `bKeepLowResActors` off (H2 variant). Batch `2026-11-21_084819_s3a_k0` | ✅ as S3a-8 |

**What the functional runs show** (one 30 s run each, so no statistics; the reports are `Benchmarks/Raw/<batch>_<Config>_…_incidents.csv`):

| Run | Transitions | Incidents (per 1,000) | Lost / Duplicated / Corrupted | Slots leaked / double-held | Baseline copies |
|---|---|---|---|---|---|
| B0, pooling off | 30,095 | 27,458 (912.4) | 22,340 / 3,967 / 106 | 1,042 / 3 | 0 |
| B1, pooling off | 30,092 | 15,769 (524.0) | 7,601 / 7,210 / 39 | 492 / 427 | 7,144 |
| B2, pooling off | 30,092 | 57 (1.89) | 47 / 0 / 1 | 4 / 5 | 3,719 |
| B3, pooling off | 30,096 | 0 | 0 / 0 / 0 | 0 / 0 | 0 |
| B0, pooling on | 30,117 | 29,422 (976.9) | 17,840 / 8,416 / 1,526 | 1,023 / 617 | 0 |
| B1, pooling on | 30,117 | **0** | 0 / 0 / 0 | 0 / 0 | 7,144 |
| B2, pooling on | 30,117 | 58 (1.93) | 48 / 0 / 1 | 4 / 5 | 3,951 |
| B3, pooling on | 30,117 | 0 | 0 / 0 / 0 | 0 / 0 | 0 |
| B0, pooling on, K0 | 30,116 | 31,975 (1,061.7) | 18,291 / 10,028 / 1,935 | 1,033 / 688 | 0 |
| B1, pooling on, K0 | 30,116 | **0** | 0 / 0 / 0 | 0 / 0 | 7,755 |
| B2, pooling on, K0 | 30,116 | 51 (1.69) | 44 / 0 / 0 | 3 / 4 | 4,400 |

Reading them (mechanisms, checked in the engine source; the numbers wait for S3b's full capture):
- **B3 = 0 everywhere**, as its rule demands, over 60,213 transitions with 196,873 writes per run: the checker raises nothing where nothing can go wrong (with S2b-8's parked run, two independent no-false-positive checks).
- **B1 without pooling fails at the release** (incidents on ActorToNone and MeshToNone, Lost and Duplicated in equal measure, 90% within 4 frames of the transition). Mass releases actors at the end of the PrePhysics phase within a time slice (`ProcessPendingDestruction`, `MG/MassActors/Private/MassActorSpawnerSubsystem.cpp:500–544`, called from `OnPrePhysicsPhaseFinished`, `:585–595`); actors past the slice that **can't be pooled** are only deactivated and destroyed on a later frame (`:527–542`). Until then the game reads the stale entity copy (*lost*); when `EndPlay` finally dehydrates, the entity jumps to the actor's older history (*duplicated*). This is H1's mechanism.
- **B1 with pooling showed 0 incidents**, the opposite of what `02` §7 expected ("the path lifecycle code misses"). Mass offers **every** queued actor to the pool in the release frame, inside or past the time slice (`:514–524`, `:527–542`), so a pool-aware `PrepareForPooling` runs before the frame's FrameEnd sweep; and until a reused actor has hydrated, B1's rule reads the entity. A lifecycle hand-off that is *not* pool-aware would still fail here (`01b #4`). To confirm in S3b with full runs, and to state in the write-up as found, not as predicted.
- **B0 with pooling** gains *corrupted* incidents (106 → 1,526) and double-held slots (3 → 617): pooled actors carry the previous agent's state into the next one (H4).
- **B2 is low but not zero** (≈ 1.9 per 1,000): its incidents are *lost* writes at ActorToNone and MeshToNone, the writes made after the frame-start mirror in a release frame (H3).
- **K0** (no kept actors) adds ActorToMesh releases: B0 rises to 1,061.7 per 1,000; B1 and B2 barely change.

**The two items carried from S2b:**
- *The checker's window now equals the CSV's.* Every S3a report says `frames=1800`, and its totals equal the CSV's per-frame sums exactly; for B1 with pooling off: 1,800 rows, 30,092 transitions, 15,769 incidents, 196,792 events, 7,144 baseline copies in both. ✅ Closed.
- *Do decided writes depend on processing order?* The two B3 runs (pooling off and on, same camera path, no re-sync because there are no incidents) applied exactly the same 196,873 writes. So configurations that share a camera path see the same decisions; differences between runs come from the checker's re-syncs (correctness runs) and from different camera paths (S2b's parked run). The claim logic stays as it is. ✅ Closed, with this evidence.

**Audit trail for S3a:**
- `code-review` (medium, Skill tool, on the staged diff, before the build): **6 findings, 2 important and 4 nits.**
  1. *important, kept by design:* B1's dehydrate runs in `EndPlay`, which Mass's queued, budgeted destruction can delay by frames, so it can overwrite newer entity writes. This is hypothesis H1 (`01` §4), the weakness B1 exists to measure, so it was **not** changed; it is now stated in the code, `05` E1 and `02` §14;
  2. *important:* the capture switch took effect mid-frame (in the director's tick, after the simulation point), so events and baseline copies were counted over a window shifted by one frame → `RequestCapturing`, applied at the start of the next frame (`BeginFrame`);
  3. the docs for the step were not written yet at review time (as the review request said) → written in the same commit;
  4. the router test couldn't check B1's hydrated case → a `friend` test accessor and both B1 cases;
  5. nothing proved B1 and B2 actually ran → `run_bench.py` fails a B1/B2 run whose report says `baselinecopies=0`; the column is in the summaries;
  6. `keeplowres` was missing from the report header → added.
- `simplify`: applied by hand (the skill starts sub-agents, which this project's brief doesn't allow). No change. Reviewed: the per-agent `FindComponentByClass` lookups (now two per agent with an actor per processor pass, in every configuration, so the cost is the same everywhere; S4's hand-off caches components per binding); the three copies by reflection in B1 and B2 (one loop over the channel types, shared shape; a shared helper would hide the direction of each copy); B2's mirror as plain struct assignments (typed, no reflection, in the per-frame path).
- **Tooling, recorded because it touched the tree:** an edit script for the review fixes was run twice; its guard ("skip if the old text is gone") re-applied insertions whose new text contains the old, which duplicated six lines (in the director, the workload header, `csv_stats.py`, `run_bench.py` and the router test). All were found before the build by a repeated-line scan of the staged files and removed. The opposite guard ("skip if the new text is present") silently skips an edit whose new text is contained in its old text: it skipped one edit of S3a's first script (the director's old capture switch, found and removed before the review) and, in S2b, the removal of one line of `02`'s diagram (an orphan "OR" node, removed now). Every edit script is now checked for both cases before it runs.

## Final job · StateHandoff v1 (S4–S5 minimal), 2026-11-21

**Job** `20261121_103528_3815df` (174.5 s): `Tools\s5_checks.bat`, the one queue job the owner's direction allowed.

| # | Check | Result |
|---|---|---|
| S5-1 | Editor build | ✅ `Result: Succeeded`, 0 errors, 0 warnings |
| S5-2 | Game build | ✅ `Result: Succeeded`, 0 errors, 0 warnings |
| S5-3 | Automation tests | ✅ "Found 12 automation tests based on 'StateHandoff'", 12 × `Result={Success}` (the router test now covers SH), "TEST COMPLETE. EXIT CODE: 0"; 0 `Warning:` in `Tests.log` |
| S5-4 | Package | ✅ cook "Success - 0 error(s), 0 warning(s)", `BUILD SUCCESSFUL` |
| S5-5 | One short batch, `2026-11-21_103723_s5_sh`: B0, B2, SH; N = 4,000, pooling on, checker on, one 30 s run each | ✅ exit 0, 0 `Error:` each. **SH: 0 incidents in 29,871 transitions**; B0: 29,454 in 30,117 (978.0 per 1,000); B2: 59 in 30,115 (1.96 per 1,000) |

SH's hand-off in that run (CSV `StateHandoff/*`, 1,800 frames; checker on, so not a timing result): processor time per frame median 0.036 ms, p95 0.079, p99 0.243, max 0.549 ms; about 3,880 binds and 3,880 unbinds in total (mean 2.15 per frame each), up to 249 binds and 393 unbinds in one frame (flips).

## Success criteria: what the existing data says

Every number below comes from the batches named, all **single 30 s functional runs** (one run per configuration, checker on). None is the planned measurement batch (S3b was cancelled), so none carries a [min–max] across runs. "Not measured" means no data exists.

| # | Target | Result | Status |
|---|---|---|---|
| SC1 | SH 0 incidents, ≥ 20,000 transitions per config per batch, pooling off and on | **0 in 29,871 transitions** (pooling on, `s5_sh`); 95% upper bound 3 / 29,871 ≈ **0.10 per 1,000**. Pooling off: not measured for SH | ✅ pooling on (one run); pooling off not measured |
| SC2 | 100% of injected faults detected; B0 shows incidents | Fault injection not built: **not measured**. B0 shows incidents in every run (912–978 per 1,000) | ❌ not measured (sanity part ✅) |
| SC3 | B1 or B2 > 0 incidents under default settings | **B1, pooling off: 524.0 per 1,000** (`s3a_p0`); **B2: 1.9–2.0 per 1,000** in every run (`s3a_p0`, `s3a_p1`, `s5_sh`). B1, pooling on: 0 (`s3a_p1`) | ✅ |
| SC4 | Hand-off per transition: median ≤ 2 µs, p99 ≤ 10 µs | Per-transition timing not built: **not measured**. Only the processor's time per frame exists (median 0.036 ms over 4,000 entities, p99 0.243 ms; checker on) | not measured |
| SC5 | Flip windows: SH p99 ≤ B0 + 0.5 ms; no extra frames > 33.3 ms | p99 in the 60 frames after each flip: B0 6.02 ms, B2 6.02 ms, SH 6.29 ms (+0.27 ms); max frame 8.8 / 9.1 / 8.3 ms, so no frame > 33.3 ms (`s5_sh`) | ✅ indicative only (one run, checker on) |
| SC6 | Parked, N = 4,000: SH game-thread median ≤ B0 + 0.10 ms | No parked SH run: **not measured** | not measured |
| SC7 | SH bookkeeping ≤ 32 bytes per entity over B3 | `FSHHandoffFragment` holds one `TWeakObjectPtr` (8 bytes) plus a tag (no memory) **[inferred from the types; the archetype dump was not run]**: **not measured** | not measured |
| SC8 | 0 crashes, ensures or errors; the edge-case tests | 0 `Error:` and 0 `Warning:` lines in every packaged run's game log (S2b, S3a, S5); the four edge-case tests were not built | partial (runs ✅, edge cases not measured) |

---

## Benchmark batches

No measurement batch was run: the S3b capture was cancelled when the project was closed (owner direction, 2026-11-21). Every batch recorded above is a functional check; [Benchmarks/README.md](../Benchmarks/README.md) lists them.
