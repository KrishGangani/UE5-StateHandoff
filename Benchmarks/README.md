# P09 · Benchmarks

Every number in this project's write-up must trace back to a file in this folder. The method is in [../Explanation/01_Problem_Analysis.md §7](../Explanation/01_Problem_Analysis.md); the planned matrices are in [../Explanation/03_Implementation_Roadmap.md](../Explanation/03_Implementation_Roadmap.md). The results, batch by batch, go into [../Explanation/09_Testing_And_Benchmarks.md](../Explanation/09_Testing_And_Benchmarks.md).

**Status (2026-11-21): only the S2b functional-check batches have run** (`s2b_orbit`, `s2b_park`, `s2b_nocheck`: one B0 run each, to prove the instrument; not measurements). The first measurement batch is the baseline capture in step S3, before any hand-off code exists.

## Measurement rules

1. **Instrument first, baselines second.** The invariant checker is built and proven on B0 before anything else (S2). B0–B3 are captured before the hand-off exists (S3), on the same workload, build and machine as every later run.
2. **Packaged Development build only** (never PIE, never the editor). The launcher engine can't build Test.
3. **Fixed step** (`-deterministic -FPS=60`), real GPU, windowed 1280×720, `-novsync`, `-nosound`. The capture window is opened and closed by the game itself (`BenchStart` / `BenchEnd` CSV events; `BenchFlip` marks each camera flip), never by a timer.
4. **Pinned to the performance cores.** `Tools/run_bench.py` (S2) pins itself once with `pin_self(performance_core_mask())` from `Tools/cpu_affinity.py` (mask `0xFFFF` = logical CPUs 0–15 on this i7-14650HX) before recording the environment or starting anything, so every game it starts inherits the mask. It records `applied_mask()` (read back from Windows), starts the real executable `Build\Windows\StateHandoff\Binaries\Win64\StateHandoff.exe` with `subprocess.Popen`, waits with a timeout and kills the process tree in a `finally` block. Never `cmd /c start /wait` (it always returns 0).
5. **One discarded priming run per batch**, then **interleaved runs with the config order rotated every round**; **runs per combination = the number of configs in the batch**, so each config sits in each order position once. Combinations no success criterion needs are skipped.
6. **Correctness runs** (checker on) and **timing runs** (checker off) are separate batches, so the instrument never adds cost to a timing result.
7. **Mass is not frame-deterministic even under `-deterministic`** (wall-clock spawn/destroy budgets). Correctness is reported **per 1,000 representation transitions**, with the transition count of every run; a zero count is reported with its 95% upper bound (3 / transitions).
8. **Statistics:** median, p95 and p99 per run (linear-interpolation percentiles), then the median across runs with the [min–max] across runs. FPS is never used as evidence.
9. **The environment is recorded with every batch** (below), automatically, by `Tools/run_bench.py`.

## Folders

| Folder | What goes in it | Rule |
|---|---|---|
| `Raw\` | CSV profiler captures (`.csv`), the checker's incident logs, per-run hand-off timing summaries, game logs (`.log`), Unreal Insights traces (`.utrace`), one `_manifest.json` per batch | **Never edited.** Only the manifests are in git (exact command lines, environment, per-run statistics). Captures are too large for git, so every batch cited in the docs is attached as `<batch>_raw.zip` to this repository's **Releases** page |
| `Summaries\` | Computed statistics, SC1–SC8 pass/fail tables, charts | Each summary names the batch it was computed from |

## File naming

`<batch>_<Config>_N<agents>_P<pooling>[_<variant>]_r<run>.<ext>`, with `<batch>` = `YYYY-MM-DD_HHMMSS[_label]` (time-stamped, so two batches can never collide; the run script refuses to overwrite).

- `<Config>`: `B0` stock · `B1` lifecycle (pool-aware) · `B2` mirror · `B3` entity-owned · `SH` StateHandoff. Fault-injection runs: `SH-F<mask>`.
- `<variant>`: `K0` (`bKeepLowResActors` off), `park` (camera parked, no flips).
- Example: `2026-11-25_101500_correct_SH_N4000_P1_r3.csv` (+ `_incidents.csv` + `.log`), `2026-11-25_101500_correct_manifest.json` in `Raw\`, and `2026-11-25_101500_correct_summary.md` in `Summaries\`.
- `_r0_prime` files are the discarded priming run, kept for transparency and excluded from every summary.

## Environment record (written into every manifest and summary)

```text
Engine:            UE 5.8.4 (CL 58902052), launcher build
Build:             Development, standalone packaged (never PIE)
Compiler:          MSVC 14.44.35229 (VS 2022 17.14), Windows SDK 10.0.22621.0
CPU / GPU / RAM:   i7-14650HX (8P + 8E) / RTX 5070 Laptop (driver recorded per batch) / 32 GB
CPU affinity:      0xFFFF (performance cores only): the batch script pins itself; applied_mask() read back and recorded
Storage:           project on D: (Kingston NVMe), engine on C: (WD NVMe)
OS:                Windows 11 build 26200
Power:             AC, power plan recorded per batch
Resolution:        1280x720 windowed, default scalability, no VSync, no sound
Mass settings:     LOD counts/distances, pooling, bKeepLowResActors, spawn/destroy budgets: recorded per run
Command line:      exact, per run, in the manifest
Warm-up excluded:  10 s     Capture length: 60 s     Runs per combination: number of configs (interleaved) + 1 priming run per batch
```

## Run log

| Batch | Configs | N | Runs | Purpose | Summary |
|---|---|---|---|---|---|
| `2026-11-21_070851_s2b_orbit` | B0 | 4,000 | 1 + prime | S2b-7 functional check: B0 must show incidents (orbit + flips, checker on). Not a measurement | [summary](Summaries/2026-11-21_070851_s2b_orbit_summary.md) |
| `2026-11-21_070949_s2b_park` | B0 | 4,000 | 1 | S2b-8 functional check: camera parked, 0 incidents expected. Not a measurement | [summary](Summaries/2026-11-21_070949_s2b_park_summary.md) |
| `2026-11-21_071008_s2b_nocheck` | B0 | 4,000 | 1 | S2b-9 functional check: checker off (timing mode) runs end to end. Not a measurement | [summary](Summaries/2026-11-21_071008_s2b_nocheck_summary.md) |
