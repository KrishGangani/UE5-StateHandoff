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

---

## Benchmark batches

*(pending)*. The first batch is the S3 baseline capture, recorded here with its batch name, as in [Benchmarks/README.md](../Benchmarks/README.md).
