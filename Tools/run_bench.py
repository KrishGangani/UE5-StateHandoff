"""P09 StateHandoff - run a benchmark batch on the packaged build and collect every artefact.

Adapted from UE5-UpdateBudget's Tools/run_bench.py on 2026-11-21 (copied, so this repository stands alone).

Usage (from anywhere):
    python -I run_bench.py --configs B0,SH --agents 4000 [--runs N] [--capture 60] [--warmup 10] [--flip-every 4]
                           [--event-rate 0.02] [--check 1] [--pooling 0|1] [--keep-low-res 0|1] [--parked]
                           [--label NAME] [--extra "..."]
                           [--no-prime] [--dry-run] [--expect-incidents some|none]

What one batch does:
  1. Pins this batch process to the performance cores (Tools/cpu_affinity.py: pin_self), before anything is recorded
     or started, so every game it starts inherits the mask; records the mask read back from Windows (applied_mask).
  2. Records the environment once (CPU, GPU + driver, OS build, power plan, AC power, engine, resolution, mask).
  3. Runs one PRIMING run first (discarded), so file, driver and PSO caches are warm for every measured run.
  4. Runs every (config x agents) combination --runs times, INTERLEAVED: the config order rotates every round, so
     thermal drift on this laptop spreads over all configs. Default --runs = the number of configs, so every config
     sits in every order position exactly once (Benchmarks/README.md, measurement rules).
  5. Starts the REAL game executable with subprocess.Popen (never `cmd /c start`, which always returns 0), waits with
     a timeout, and kills its process tree in a `finally` block. Deletes GameUserSettings.ini before each run.
  6. Moves the CSV capture, the checker's incident report and the run's log into Benchmarks/Raw as
     <batch>_<Config>_N<agents>_P<pooling>[_K0][_park]_r<run>.<ext> (Benchmarks/README.md, file naming), <batch> =
     YYYY-MM-DD_HHMMSS[_label]. Never overwrites. The manifest is
     rewritten after every run, so a crash loses nothing.
  7. Writes Benchmarks/Summaries/<batch>_summary.md: per combination, the median across runs of each run's
     median / p95 / p99 [min-max across runs], the flip-window statistics, transitions and incidents per 1,000.

Every run uses: fixed step (-deterministic -FPS=60), real GPU at 1280x720 windowed, no VSync, no sound,
-traceautostart=0, and a capture window started and stopped by the game's BenchStart/BenchEnd events.
"""
from __future__ import annotations

import argparse
import datetime as dt
import glob
import json
import os
import shutil
import statistics
import subprocess
import sys
import time

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, TOOLS_DIR)
import cpu_affinity  # noqa: E402  (same folder)
import csv_stats  # noqa: E402  (same folder)

PROJECT_DIR = os.path.dirname(TOOLS_DIR)
BUILD_DIR = os.path.join(PROJECT_DIR, "Build", "Windows")
# The real game executable, launched directly: the game is then a direct child, so its exit code and its kill are
# reliable. (The bootstrap Build\Windows\StateHandoff.exe only starts this one.)
EXE = os.path.join(BUILD_DIR, "StateHandoff", "Binaries", "Win64", "StateHandoff.exe")
SAVED = os.path.join(BUILD_DIR, "StateHandoff", "Saved")
RAW_DIR = os.path.join(PROJECT_DIR, "Benchmarks", "Raw")
SUMMARY_DIR = os.path.join(PROJECT_DIR, "Benchmarks", "Summaries")
ENGINE_BUILD_VERSION = r"C:\Program Files\Epic Games\UE_5.8\Engine\Build\Build.version"
STARTUP_MARGIN_SECONDS = 180.0  # generous allowance for engine start, spawning and shutdown before a run is declared hung
FLIP_WINDOW_FRAMES = 60         # the frames after each BenchFlip that form a flip window (01 section 6.3)

COMMON_ARGS = ("-windowed -ResX=1280 -ResY=720 -nosound -unattended -traceautostart=0 "
               "-deterministic -FPS=60 -novsync -SHExit -csvStartOnEvent=BenchStart -csvStopOnEvent=BenchEnd")

CONFIGS = ("B0", "B1", "B2", "B3", "SH")   # 01 section 6.2; the game reads -SHConfig=
SUMMARY_COLUMNS = ("FrameTime", "GameThreadTime", "SHWorkload/Transitions", "SHWorkload/Incidents",
                   "SHWorkload/AgentsWithActor", "SHWorkload/BaselineCopies", "MassActors/NumSpawned")
COPYING_BASELINES = ("B1", "B2")   # their own copies (hydrate/dehydrate, push) must happen in every run


def powershell(command: str) -> str:
    result = subprocess.run(["powershell", "-NoProfile", "-Command", command], capture_output=True, text=True)
    return result.stdout.strip()


def environment_record() -> dict:
    """Facts that must accompany every result (Benchmarks/README.md, environment record)."""
    with open(ENGINE_BUILD_VERSION, encoding="utf-8") as handle:
        build = json.load(handle)
    return {
        "date": dt.datetime.now().isoformat(timespec="seconds"),
        "engine": f"{build['MajorVersion']}.{build['MinorVersion']}.{build['PatchVersion']} (CL {build['Changelist']})",
        "build_config": "Development (packaged)",
        "compiler": "MSVC 14.44.35229, Windows SDK 10.0.22621.0",
        "cpu": powershell("(Get-CimInstance Win32_Processor).Name"),
        "gpus": powershell("(Get-CimInstance Win32_VideoController | ForEach-Object { $_.Name + ' (driver ' + $_.DriverVersion + ')' }) -join '; '"),
        "ram_gb": powershell("[math]::Round((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory/1GB,1)"),
        "os": powershell("$o=Get-CimInstance Win32_OperatingSystem; $o.Caption + ' build ' + $o.BuildNumber"),
        "power_plan": powershell("powercfg /getactivescheme"),
        "on_ac_power": powershell("(Get-CimInstance Win32_Battery | Select-Object -First 1).BatteryStatus") == "2",
        "resolution": "1280x720 windowed, default scalability",
        "common_args": COMMON_ARGS,
        "cpu_affinity": cpu_affinity.applied_mask(),
    }


def unique_path(path: str) -> str:
    """Raw data is never overwritten (Benchmarks/README.md)."""
    if os.path.exists(path):
        raise FileExistsError(f"refusing to overwrite raw data: {path}")
    return path


def launch(name: str, config: str, agents: int, args) -> tuple[str, int | None, float, float]:
    """Starts one run and waits for it. Returns (command, exit code or None on timeout, start time, wall seconds)."""
    sh_args = (f"-SHConfig={config} -SHAgents={agents} -SHSeed={args.seed} -SHWarmup={args.warmup} "
               f"-SHCapture={args.capture} -SHFlipEvery={args.flip_every} -SHEventRate={args.event_rate} "
               f"-SHCheck={args.check} -SHPooling={args.pooling} -SHKeepLowRes={args.keep_low_res} -SHRunTag={name}")
    command = f'"{EXE}" {COMMON_ARGS} {sh_args} {args.extra}'.strip()
    if args.dry_run:
        print("DRY:", command)
        return command, 0, time.time(), 0.0

    gus = os.path.join(SAVED, "Config", "Windows", "GameUserSettings.ini")
    if os.path.exists(gus):
        os.remove(gus)
    started = time.time()
    process = subprocess.Popen(command)   # inherits this process's CPU affinity (main: pin_self)
    try:
        exit_code = process.wait(timeout=args.warmup + args.capture + STARTUP_MARGIN_SECONDS)
    except subprocess.TimeoutExpired:
        exit_code = None
    finally:
        if process.poll() is None:   # timeout, Ctrl+C or any other error: never leave a game running into the next run
            subprocess.run(["taskkill", "/F", "/T", "/PID", str(process.pid)], capture_output=True)
    return command, exit_code, started, time.time() - started


def read_incident_summary(path: str) -> dict:
    """The '# key=value' header lines of the checker's report (USHWorkloadSubsystem::WriteReport)."""
    summary: dict = {}
    with open(path, encoding="utf-8", errors="replace") as handle:
        for line in handle:
            if not line.startswith("#"):
                break
            for token in line[1:].split():
                if "=" in token:
                    key, value = token.split("=", 1)
                    summary[key] = value
    return summary


def collect(name: str, started: float) -> dict:
    """Moves this run's artefacts into Benchmarks/Raw and returns what was found."""
    record: dict = {}
    os.makedirs(RAW_DIR, exist_ok=True)

    csv_files = [p for p in glob.glob(os.path.join(SAVED, "Profiling", "CSV", "*.csv")) if os.path.getmtime(p) >= started]
    if csv_files:
        destination = unique_path(os.path.join(RAW_DIR, name + ".csv"))
        shutil.move(max(csv_files, key=os.path.getmtime), destination)
        record["capture"] = os.path.basename(destination)
        try:
            analysis = csv_stats.analyse(destination, SUMMARY_COLUMNS, 0, None, "BenchFlip", FLIP_WINDOW_FRAMES)
            record["stats"] = analysis["columns"]
            record["finalised"] = analysis["finalised"]
            record["events"] = analysis["events"]
            if "window" in analysis:
                record["flip_windows"] = analysis["window"]
        except (ValueError, IndexError) as error:
            record["analysis_error"] = str(error)

    report = os.path.join(SAVED, "SHRuns", name + "_incidents.csv")
    if os.path.exists(report):
        destination = unique_path(os.path.join(RAW_DIR, name + "_incidents.csv"))
        shutil.move(report, destination)
        record["incidents_report"] = os.path.basename(destination)
        record["checker"] = read_incident_summary(destination)

    log = os.path.join(SAVED, "Logs", "StateHandoff.log")
    if os.path.exists(log) and os.path.getmtime(log) >= started:   # only this run's log, never a stale one
        with open(log, encoding="utf-8", errors="replace") as handle:
            text = handle.read()
        record["log_errors"] = text.count("Error:")
        record["log_warnings"] = text.count("Warning:")
        shutil.copy(log, unique_path(os.path.join(RAW_DIR, name + ".log")))
    else:
        record["log_missing"] = True
    return record


def is_failure(record: dict) -> bool:
    baseline_idle = (record.get("config") in COPYING_BASELINES
                     and int(record.get("checker", {}).get("baselinecopies", "0")) == 0)
    return (record.get("exit_code") != 0 or "stats" not in record or record.get("log_errors")
            or record.get("log_missing") or record.get("finalised") is False or "analysis_error" in record
            or "checker" not in record or baseline_idle)


def write_manifest(batch: str, environment: dict, records: list[dict]) -> None:
    os.makedirs(RAW_DIR, exist_ok=True)
    with open(os.path.join(RAW_DIR, batch + "_manifest.json"), "w", encoding="utf-8") as handle:
        json.dump({"environment": environment, "runs": records}, handle, indent=2)


def median_range(values: list[float], digits: int = 3) -> str:
    if not values:
        return "n/a"
    return f"{statistics.median(values):.{digits}f} [{min(values):.{digits}f}-{max(values):.{digits}f}]"


def write_summary(batch: str, environment: dict, records: list[dict]) -> str:
    os.makedirs(SUMMARY_DIR, exist_ok=True)
    groups: dict[tuple, list[dict]] = {}
    for record in records:
        if "stats" in record and not record.get("priming"):
            groups.setdefault((record["config"], record["agents"]), []).append(record)

    lines = [f"# Benchmark batch {batch}", "", "## Environment", "", "```json", json.dumps(environment, indent=2), "```", "",
             "## Results", "",
             "Each cell: median across runs of each run's statistic, then [min-max across runs]. Flip windows: the "
             f"{FLIP_WINDOW_FRAMES} frames from each BenchFlip, pooled per run. Incidents per 1,000 transitions: from the "
             "checker's report of each run (0 when the checker is off).", ""]
    header = ("| Config | N | Runs | FrameTime median / p95 / p99 (ms) | GameThreadTime median / p95 / p99 (ms) "
              "| Flip windows: FrameTime p99 / max (ms) | Transitions per run | Incidents per 1,000 | Baseline copies per run | Log errors |")
    lines += [header, "|" + "---|" * (header.count("|") - 1)]
    for (config, agents), group in sorted(groups.items(), key=lambda kv: (kv[0][1], kv[0][0])):
        cells = []
        for column in ("FrameTime", "GameThreadTime"):
            parts = [median_range([g["stats"][column][stat] for g in group if column in g["stats"]]) for stat in ("median", "p95", "p99")]
            cells.append(" / ".join(parts))
        flip_p99 = [g["flip_windows"]["columns"]["FrameTime"]["p99"] for g in group if "FrameTime" in g.get("flip_windows", {}).get("columns", {})]
        flip_max = [g["flip_windows"]["columns"]["FrameTime"]["max"] for g in group if "FrameTime" in g.get("flip_windows", {}).get("columns", {})]
        transitions = [float(g["checker"].get("transitions", "nan")) for g in group if "checker" in g]
        per_thousand = [float(g["checker"].get("per1000", "nan")) for g in group if "checker" in g]
        copies = [float(g["checker"].get("baselinecopies", "nan")) for g in group if "checker" in g]
        errors = sum(g.get("log_errors", 0) for g in group)
        lines.append(f"| {config} | {agents} | {len(group)} | " + " | ".join(cells)
                     + f" | {median_range(flip_p99)} / {median_range(flip_max)} | {median_range(transitions, 0)} | {median_range(per_thousand, 4)}"
                     + f" | {median_range(copies, 0)} | {errors} |")
    lines += ["", f"Raw files: `Benchmarks/Raw/{batch}_*` (CSV, incident report and log per run) and `{batch}_manifest.json`."]
    path = os.path.join(SUMMARY_DIR, batch + "_summary.md")
    with open(path, "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")
    return path


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--configs", default="B0")
    parser.add_argument("--agents", default="4000")
    parser.add_argument("--runs", type=int, default=0, help="runs per combination; 0 (default) = the number of configs")
    parser.add_argument("--warmup", type=float, default=10.0)
    parser.add_argument("--capture", type=float, default=60.0)
    parser.add_argument("--flip-every", type=float, default=4.0)
    parser.add_argument("--event-rate", type=float, default=0.02)
    parser.add_argument("--check", type=int, choices=(0, 1), default=1)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--parked", action="store_true", help="camera parked: no orbit, no flips (-SHParked)")
    parser.add_argument("--pooling", type=int, choices=(0, 1), default=0, help="Mass actor pooling for the demo actors")
    parser.add_argument("--keep-low-res", type=int, choices=(0, 1), default=1,
                        help="bKeepLowResActors (1 = the engine's default; 0 = the H2 variant, named _K0)")
    parser.add_argument("--extra", default="", help="extra game arguments, the same for every run")
    parser.add_argument("--cooldown", type=float, default=5.0)
    parser.add_argument("--label", default="")
    parser.add_argument("--no-prime", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--expect-incidents", choices=("some", "none"),
                        help="functional checks: fail unless every measured run's checker found some / no incidents")
    args = parser.parse_args(argv)

    if args.parked:
        args.flip_every = 0.0            # the game ignores flips when parked; recorded the same way here
        args.extra = (args.extra + " -SHParked").strip()
    configs = [c.strip() for c in args.configs.split(",") if c.strip()]
    unknown = [c for c in configs if c not in CONFIGS]
    if unknown:
        parser.error(f"unknown config(s) {unknown}; available: {list(CONFIGS)}")
    agent_counts = [int(a) for a in args.agents.split(",") if a.strip()]
    if args.runs <= 0:
        args.runs = len(configs)
    elif args.runs % len(configs):
        print(f"WARNING: --runs {args.runs} is not a multiple of {len(configs)} configs; the rotation is unbalanced")
    if not os.path.exists(EXE) and not args.dry_run:
        parser.error(f"packaged build not found: {EXE} (run Tools\\package.bat first)")

    if not args.dry_run and cpu_affinity.KERNEL32 is not None:
        # Keep the display and the system awake while the batch runs (ES_CONTINUOUS | ES_SYSTEM_REQUIRED |
        # ES_DISPLAY_REQUIRED): an occluded or locked desktop can throttle a windowed game. No power settings change.
        cpu_affinity.KERNEL32.SetThreadExecutionState(0x80000000 | 0x00000001 | 0x00000002)
        mask = cpu_affinity.performance_core_mask()
        if mask and not cpu_affinity.pin_self(mask):
            parser.error(f"could not pin the batch to the performance cores (0x{mask:X}); refusing to measure without it")

    batch = dt.datetime.now().strftime("%Y-%m-%d_%H%M%S") + (f"_{args.label}" if args.label else "")
    environment = environment_record() if not args.dry_run else {}
    environment["batch_args"] = vars(args)
    records: list[dict] = []

    def run(config: str, agents: int, run_index: int, priming: bool = False) -> None:
        name = (f"{batch}_{config}_N{agents}_P{args.pooling}" + ("_K0" if not args.keep_low_res else "")
                + ("_park" if args.parked else "") + f"_r{run_index}" + ("_prime" if priming else ""))
        command, exit_code, started, wall = launch(name, config, agents, args)
        record = {"name": name, "config": config, "agents": agents, "run": run_index, "priming": priming,
                  "exit_code": exit_code, "wall_seconds": round(wall, 1), "command": command}
        if not args.dry_run:
            record.update(collect(name, started))
            records.append(record)
            write_manifest(batch, environment, records)
            frame = record.get("stats", {}).get("FrameTime", {})
            checker = record.get("checker", {})
            print(f"  {name}: exit {exit_code}, {wall:.0f} s, FrameTime median {frame.get('median', float('nan')):.3f} ms "
                  f"p99 {frame.get('p99', float('nan')):.3f} ms, transitions {checker.get('transitions', '?')}, "
                  f"incidents {checker.get('incidents', '?')} ({checker.get('per1000', '?')} per 1,000), "
                  f"log errors {record.get('log_errors')}" + ("  [PRIMING, discarded]" if priming else ""))
            time.sleep(args.cooldown)

    if not args.no_prime:
        run(configs[0], min(agent_counts), 0, priming=True)

    for run_index in range(1, args.runs + 1):
        rotation = (run_index - 1) % len(configs)
        round_configs = configs[rotation:] + configs[:rotation]       # interleave: rotate the order every round
        for agents in agent_counts:
            for config in round_configs:
                run(config, agents, run_index)

    if args.dry_run:
        return 0
    print("summary:", write_summary(batch, environment, records))
    failures = [r["name"] for r in records if not r.get("priming") and is_failure(r)]
    if failures:
        print("ATTENTION - runs with a non-zero exit, timeout, no/partial capture, missing report or log, log errors,"
              " or a B1/B2 baseline that made no copies:", failures)
        return 1
    if args.expect_incidents:
        wrong = [r["name"] for r in records if not r.get("priming")
                 and (int(r["checker"].get("incidents", "0")) > 0) != (args.expect_incidents == "some")]
        if wrong:
            print(f"EXPECTATION FAILED - expected {args.expect_incidents} incidents in:", wrong)
            return 2
        print(f"expectation met: {args.expect_incidents} incidents in every measured run")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
