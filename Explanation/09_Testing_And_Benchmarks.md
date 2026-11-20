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

---

## Phase 5–8

*(pending)*. The first batch is the S3 baseline capture, recorded here with its batch name, as in [Benchmarks/README.md](../Benchmarks/README.md).
