"""P09 StateHandoff - per-class incident table of one or more batches, from their manifests only.

The manifests are in git (Benchmarks/Raw/*_manifest.json), so every number this prints can be re-derived without the
raw captures. For each (configuration, N, pooling, keep-low-res) it prints the median across measured runs, with the
[min-max], of: transitions, incidents per 1,000 transitions, the four channel classes summed over the channels
(lost, duplicated, corrupted), slots leaked and double-held, and the baseline's own copies.

Usage:  python -I class_table.py Benchmarks/Raw/<batch>_manifest.json [more manifests...]
"""
from __future__ import annotations

import json
import statistics
import sys

CHANNELS = ("Health", "Cooldown", "Memory", "Reservation")


def channel_classes(checker: dict) -> dict:
    """Sums the report's per-channel 'Name:lost=a,dup=b,corrupt=c,checks=d' tokens (split by read_incident_summary
    into key 'Name:lost' and value 'a,dup=b,corrupt=c,checks=d')."""
    totals = {"lost": 0, "dup": 0, "corrupt": 0}
    for channel in CHANNELS:
        value = checker.get(f"{channel}:lost")
        if value is None:
            continue
        parts = value.split(",")
        totals["lost"] += int(parts[0])
        for part in parts[1:]:
            key, number = part.split("=")
            if key in totals:
                totals[key] += int(number)
    return totals


def cell(values: list[float], digits: int = 0) -> str:
    if not values:
        return "n/a"
    return f"{statistics.median(values):,.{digits}f} [{min(values):,.{digits}f}-{max(values):,.{digits}f}]"


def main(paths: list[str]) -> int:
    groups: dict[tuple, list[dict]] = {}
    for path in paths:
        with open(path, encoding="utf-8") as handle:
            manifest = json.load(handle)
        for run in manifest["runs"]:
            if run.get("priming") or "checker" not in run:
                continue
            checker = run["checker"]
            key = (run["config"], run["agents"], checker.get("pooling", "?"), checker.get("keeplowres", "?"))
            groups.setdefault(key, []).append(checker)

    print("| Config | N | Pooling | Keep low-res | Runs | Transitions | Incidents per 1,000 | Lost | Duplicated | Corrupted "
          "| Slots leaked | Slots double-held | Baseline copies |")
    print("|---|---|---|---|---|---|---|---|---|---|---|---|---|")
    for key in sorted(groups):
        runs = groups[key]
        classes = [channel_classes(c) for c in runs]
        print(f"| {key[0]} | {key[1]} | {key[2]} | {key[3]} | {len(runs)} "
              f"| {cell([float(c['transitions']) for c in runs])} | {cell([float(c['per1000']) for c in runs], 2)} "
              f"| {cell([k['lost'] for k in classes])} | {cell([k['dup'] for k in classes])} | {cell([k['corrupt'] for k in classes])} "
              f"| {cell([float(c['slotleaked']) for c in runs])} | {cell([float(c['slotdoubleheld']) for c in runs])} "
              f"| {cell([float(c.get('baselinecopies', 'nan')) for c in runs])} |")
    return 0


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(2)
    sys.exit(main(sys.argv[1:]))
