# 03 · Implementation Roadmap: StateHandoff

**Written:** end of Phase 3, 2026-11-21 · **Engine:** UE 5.8.4

**Rules for every step:**
- Each step is small enough to read in one sitting: a few hundred lines at most.
- A step is finished only when its code **compiles with 0 warnings through the UE job queue**, its check **passes**, and its `Explanation/` entries (`05`, `06`, and `07` where relevant) are **written in the same step**.
- After each step that adds C++: run `simplify`, then `code-review`.
- **Instrument first, baselines second, system third.** The checker is the measuring instrument, so it is built and proven on the baselines before any hand-off code exists. Steps S1–S3 build the workload, the checker and the baselines B0–B3, and **capture the baselines**. The hand-off itself starts at S4.

---

**Progress (2026-11-21):** Phases 1–3 ✅. **Phase 4 ✅** (F1–F4; one queue job, all five checks passed, `09`). **S1 ✅** (code review and simplify done; `09`). **S2a ✅** (10 automation tests pass; code review and simplify done; `09`). **S2b ✅** (B0 shows incidents, a parked camera shows none; `09`). Next: **S3**. Carried into S3 from S2b (`09`): start the checker's count one frame after `BenchStart`, as the CSV capture does; check whether claim outcomes depend on processing order and, if so, make them independent of it.

**Change to the plan (S2 split, 2026-11-21):** S2 as first written was about 1,500 lines (plugin checker, demo state, Mass integration and the Python tools). It is split into **S2a**, the instrument as plain C++ that tests without a world, and **S2b**, the instrument wired into Mass with the run tools, so each step stays readable in one sitting.

## Phase 4: Foundation (no gameplay code yet) ✅

| Step | What gets built | How it is checked |
|---|---|---|
| **F1** | `StateHandoff\StateHandoff.uproject` (EngineAssociation 5.8; plugins `StateHandoffCore`, `MassGameplay`), `Source\StateHandoff.Target.cs`, `StateHandoffEditor.Target.cs`, the game module (`StateHandoff.Build.cs`, module `.cpp`) | `Tools\build.bat StateHandoffEditor Development` and `Tools\build.bat StateHandoff Development` succeed through the queue with 0 errors and 0 warnings |
| **F2** | The plugin skeleton: `Plugins\StateHandoffCore\StateHandoffCore.uplugin` (depends on the `MassGameplay` plugin), `Build.cs`, module `.cpp` | Builds; the module loads in the smoke runs |
| **F3** | `Config\DefaultEngine.ini` (default map, VSync off, no smoothing), `DefaultGame.ini`, `DefaultInput.ini` (the two Enhanced Input keys of Epic's template) | Uncooked `-game -nullrhi` smoke run loads the map and quits with 0 errors |
| **F4** | `Tools\package.bat`, `smoke_packaged.bat`, `phase4_checks.bat`; `Tools\cpu_affinity.py` | A packaged **Development** build with MassGameplay enabled cooks, stages and runs: this is the **launcher-engine check** (are the Experimental Mass plugins buildable and packageable from a launcher install?) |

## Phase 5: Implementation

| Step | What gets built | How it is checked | Explanation files touched |
|---|---|---|---|
| **S1. Workload skeleton (Mass only)** | `FSHAgentIdFragment`, `FSHDemoAgentTag`, `USHDemoStateTrait` (no channels yet); `ASHDemoAgent` (mesh only); `USHVisualizationProcessor`, `USHVisualizationLODProcessor`, `USHLODCollectorProcessor`; `USHBenchmarkSubsystem`: command line, code-built entity config with `UMassMovableVisualizationTrait`, seeded spawn of N entities, camera path, flips, `CSV_EVENT`s. Build.cs gains the Mass modules; `DefaultGame.ini` cooks `/Engine/BasicShapes` | Entities spawn; representation switches are visible (`MassActors/NumSpawned` > 0, archetype dump shows the three representations); same seed → same position hash in two runs | 04, 05, 06, 07 |
| **S2a. The instrument, as plain C++** | Plugin: `SHContractTypes.h` (`FSHChannelFragment`, `ESHDivergence`), **`FSHInvariantChecker`** + automation tests (classification with hand-made structs, no world), `Tools\run_tests.bat`. Game: the four channel structs, events (`SHDemo::MakeEvent`, deterministic), the oracle's rules (`Decide`, `ApplyWrite`), the slot registry, with automation tests | Checker and demo-state tests pass (`StateHandoff.Checker.*`, `StateHandoff.Demo.*`); editor and game targets build with 0 warnings | 04, 05, 06, 07, 09 |
| **S2b. The instrument in the world** | `USHAgentStateComponent` on the demo actor; the channel fragments and `FSHObservedFragment` in the demo trait; `FSHStateRouter` (B0 rule); `USHWorkloadSubsystem` (oracle, registry, sweep, re-sync, reservation invariants, CSV, run report); `USHSimEventProcessor`, `USHFrameEndProcessor`; the director passes the configuration. Tools: `csv_stats.py`, `run_bench.py` (adapted from UE5-UpdateBudget; pins itself with `pin_self` and records `applied_mask()`, then launches the real exe with `subprocess.Popen`) | **B0 must show incidents** (sanity, part of SC2); a run with no transitions (camera parked, no flips) shows 0 incidents (no false positives); `csv_stats.py --selftest` | 04, 05, 06, 07, 09 |
| **S3. Baselines B1, B2, B3 + calibration + baseline capture** | `USHLifecycleComponent` (B1, pool-aware), `USHB2MirrorProcessor` + `USHB2ActorManagement` (B2), the B3 router rule; pooling and `bKeepLowResActors` switches. **Calibration** of LOD counts and distances, event rate and flip timing so a correctness batch reaches ≥ 20,000 transitions per config | **The baseline capture**, before any hand-off code: correctness batch (B0–B3, N = 4,000, pooling off/on, checker on), timing batch (checker off; N = 1,000/4,000; flips, and camera parked), memory table, H2 variant. H1–H5 answered for the baselines **with data** | 05, 06, 07, 09 |
| **S4. Contract + trait + subsystem (no hand-off yet)** | `FSHChannelSpec`, `FSHHandoffFragment`, `FSHChannelSetFragment`, `FSHHandoffTag`; `USHStateHandoffTrait` (resolve, validate, add fragments); `USHStateHandoffSubsystem` (channel sets, authoritative-copy query, telemetry skeleton); the SH router rule | Validation rejects bad specs (wrong type, missing property, duplicate) in tests; SH archetype = baseline archetype + bookkeeping only (archetype dump, SC7) | 04, 05, 06, 07 |
| **S5. Commit-time hand-off** | `USHHandoffProcessor` (bind/unbind detection, queued hand-offs after the chunk loop), `Bind`/`Unbind` with reflection copies | First SH correctness run; processor order checked with Mass's dependency output (the "to verify" item in `02` §8) | 05, 06, 07 |
| **S6. The other release paths** | `USHRepresentationActorManagement` (pre-spawn capture), `OnEndPlay` safety net, `USHHandoffFragmentDestructor` | Edge-case tests (SC8): entity destroyed while bound, actor destroyed by gameplay while bound, world teardown mid-flip, pooled actor reused for another entity | 05, 06, 07, 09 |
| **S7. Telemetry, timings, fault injection** | `StateHandoff` CSV stats, per-transition timing array and end-of-run summary, `sh.FaultInject` / `-SHFault=` | **SC2 batch:** each injected fault detected (≥ 1 incident per 1,000 transitions on a run that exercises its path) | 05, 06, 08, 09 |
| **S8. Debug view** | `sh.Debug`: on-screen counters, per-agent health tint and text | Visual check (screenshots for `Media/`) | 05, 08 |

## Phases 6–10 (after the code exists)

| Phase | Work |
|---|---|
| 6 Explanation | Consolidate 04–07 into one consistent line-by-line pass; re-check every Mass API against 5.8.4 |
| 7 Testing | Edge cases (N = 0 and 1; all agents bound; nothing bound; pooling toggled; every LOD flip pattern), a 30-minute soak with flips, scalability (N sweep) |
| 8 Profiling | The full matrices below, fixed step, P-core pinned; Insights drill-down of a flip frame with the Mass trace; optimisation only with data (`11_Optimization_Decisions.md`) |
| 9 Code review | `code-review` and `simplify`, plus a written self-review: weaknesses, not praise |
| 10 Portfolio | Charts (`dataviz`), an architecture page, the demo video, README results, interview answers |

## Planned measurement matrices (runs per combination = number of configs in the batch; one priming run per batch)

| Batch | Configs | Parameters | Checker | Criteria | Runs |
|---|---|---|---|---|---|
| Correctness | B0, B1, B2, B3, SH | N = 4,000; pooling 0 and 1; flips on | on | SC1, SC3, SC8 | 5 × 2 × 5 = 50 |
| H2 variant | B0, B1, B2, SH | N = 4,000; pooling 1; `bKeepLowResActors = false` | on | H2 | 4 × 4 = 16 |
| Fault injection | SH with each of 4 faults | N = 4,000; pooling 1 | on | SC2 | 4 × 1 (detection, not timing) |
| Timing, flips | B0, B1, B2, B3, SH | N = 1,000 and 4,000; pooling 1 | off | SC4, SC5 | 5 × 2 × 5 = 50 |
| Timing, parked | B0, B2, SH | N = 4,000; camera parked, no flips | off | SC6 | 3 × 3 = 9 (B1, B3 skipped: no per-frame work when nothing changes) |
| Memory | B0, B3, SH | N = 4,000; one fixed moment | off | SC7 | 3 × 1 |

**Rough cost:** about 130 runs × (about 20 s start-up + 10 s warm-up + 60 s capture) ≈ 3.5 hours of queue time, scripted and unattended, split across batches so other projects can use the machine in between.
