# P09 · StateHandoff: gameplay state that survives LOD switches

**Author:** Krish Gangani · [github.com/KrishGangani](https://github.com/KrishGangani)

**Domain:** Large-scale gameplay architecture · data-oriented design · **Difficulty:** ★★★★★ · **Engine:** UE 5.8.4

> **Status: in progress. There are no results yet.** Phases 1–3 are done (problem analysis, verified research, final architecture); Phase 4 (the UE project foundation) is being built and checked. Sections marked *(pending)* are filled in only from real measurements, in the phase named. The success criteria are **targets fixed before any code exists, not results**.

| Phase | 1 Problem | 2 Research | 3 Architecture | 4 Foundation | 5 Implementation | 6 Explanation | 7 Testing | 8 Profiling | 9 Review | 10 Portfolio |
|---|---|---|---|---|---|---|---|---|---|---|
| State | ✓ | ✓ | ✓ final | in progress | — | — | — | — | — | — |

---

## Problem statement

Unreal's Mass framework switches each agent between a full **actor**, an **instanced mesh** and **nothing** as its level of detail changes. The engine carries the agent's position, movement and orientation across every switch. It does not carry the state that actor-side systems keep: health, cooldowns, AI memory, reserved interaction slots. Each project hand-writes that hand-off per actor class, and nothing checks it. Epic, on its Gameplay Ability System with Mass: "There are issues with needing to track the state of the ASC, effects, abilities, and stats persist between LOD changes since Actor representation changes." Details: [Explanation/01_Problem_Analysis.md](Explanation/01_Problem_Analysis.md).

## What already exists (and what Phase 2 changed)

- The engine's translators copy **only spatial data** (transform, location, orientation, velocity, capsule transform). Verified in the 5.8.4 source; the one other translator, for BehaviorTree, is an empty stub.
- State owned by Mass-native systems (Mass StateTree, Mass Smart Object claims) **already survives** a switch, so it is out of scope.
- The engine offers **hooks** (spawn callbacks, pooling callbacks, trait initializers), and Epic published a hand-written "hydrate in `BeginPlay`, dehydrate in `EndPlay`" pattern in 2026 (the PersistenceLab sample). This project credits it and measures it as a baseline.
- What is still missing, among the public sources reviewed: an explicit **ownership contract** tied to the representation **commit** (Mass decouples actor lifetime from the representation: queued and budgeted destruction, pooling without `EndPlay`, kept-disabled actors), a **generic** hand-off, an **automated checker**, and a **controlled measurement**. The gap is medium for the mechanism and strong for the contract, checker and measurement. Details: [Explanation/01b_Research_Findings.md](Explanation/01b_Research_Findings.md) §6.

## Engineering approach

Each state field declares its owner: the actor while the entity is **bound** to an enabled actor, the entity otherwise. A Mass processor that runs right after the engine finalises the representation moves ownership (capture actor → entity, restore entity → actor) through one generic, reflection-based path. Two more paths cover the releases that processor can't see in time: an immediate template swap, and an actor ending play behind Mass's back. An **invariant checker** replays the same gameplay events on an independent reference model and classifies every mismatch as lost, duplicated, corrupted, or a reservation double-held or leaked. Fault injection proves the checker sees real faults. Details: [Explanation/02_System_Architecture.md](Explanation/02_System_Architecture.md).

## Success criteria (targets, defined before implementation)

| # | Criterion | Target |
|---|---|---|
| SC1 | Divergence incidents per 1,000 transitions, StateHandoff | **0**, over ≥ 20,000 transitions per config per batch (95% upper bound reported) |
| SC2 | The checker detects injected faults | 100% of injected faults detected |
| SC3 | The problem is real | At least one established pattern shows > 0 incidents; if none does, that is reported plainly |
| SC4 | Hand-off cost per transition | median ≤ 2 µs, p99 ≤ 10 µs |
| SC5 | Frame spikes during mass LOD flips | p99 ≤ stock + 0.5 ms; no extra frames > 33.3 ms |
| SC6 | Steady-state overhead | ≤ +0.10 ms game thread at N = 4,000 |
| SC7 | Memory per agent | bookkeeping ≤ 32 bytes per entity; a full table per representation |
| SC8 | Lifetime safety | 0 crashes, ensures or errors |

Full definitions and reasons: [01_Problem_Analysis.md §6.4](Explanation/01_Problem_Analysis.md).

## Architecture diagram

Final (v1.0): [Explanation/02_System_Architecture.md](Explanation/02_System_Architecture.md) §5–6 (mermaid).

## Before / after comparison

*(pending, Phase 8)*. Five configurations on one fixed workload: B0 stock, B1 Epic's lifecycle pattern (pool-aware), B2 per-frame mirror, B3 entity-owned, SH StateHandoff.

## Benchmark results

*(pending, Phase 8)*. Raw data and protocol: [Benchmarks/](Benchmarks/README.md).

## Recorded demonstration

*(pending, Phase 10)*. B0 vs SH on the same camera flips, with a live counter of transitions and divergence incidents.

## Scope and limits

Single-player only: there is no Iris path for Mass entity replication in UE 5.8.4 (verified in the source). Mass is **Experimental** and was restructured in 5.7 and 5.8, so everything is pinned to 5.8.4. CitySample is used as a reference only.

## Source code

The UE project is in [StateHandoff/](StateHandoff/) (UE 5.8.4, C++):
- the reusable system is the plugin `StateHandoffCore` (it depends on the MassGameplay plugin);
- the workload, baselines and benchmark code go in the game module `StateHandoff`.

## Documentation

| File | Contents |
|---|---|
| [00_Project_Overview](Explanation/00_Project_Overview.md) | Identity, demonstration, presentation, industry relevance |
| [01_Problem_Analysis](Explanation/01_Problem_Analysis.md) | Problem, existing work, root-cause hypotheses, success criteria, measurement plan |
| [01b_Research_Findings](Explanation/01b_Research_Findings.md) | Phase 2: verified engine behaviour (UE 5.8.4), existing work, the revised gap |
| [02_System_Architecture](Explanation/02_System_Architecture.md) | Final v1.0: modules, classes, commit-time hand-off, data flow, safety rules, rejected alternatives |
| [03_Implementation_Roadmap](Explanation/03_Implementation_Roadmap.md) | The small, ordered build steps; the checker and the baselines come before the hand-off |
| [04](Explanation/04_CPP_Fundamentals.md) · [05](Explanation/05_Line_By_Line_Code_Explanation.md) · [06](Explanation/06_Unreal_APIs_And_Macros.md) · [07](Explanation/07_Common_Mistakes.md) | C++ fundamentals, line-by-line code, APIs and macros, common mistakes. Current to Phase 4 |
| [09](Explanation/09_Testing_And_Benchmarks.md) | Every check and benchmark batch, with its raw-data batch name |
| [08](Explanation/08_Debugging_And_Profiling.md) · [10](Explanation/10_Interview_Questions.md) · [11](Explanation/11_Optimization_Decisions.md) · [12](Explanation/12_Future_Scalability.md) | Filled in during Phases 7–10 |
| [Phase2_Raw_Notes/](Explanation/Phase2_Raw_Notes/README.md) | The audit trail behind `01b` |
| [Research/](Explanation/Research/) | The portfolio-wide research this project draws on: job market, technical challenges, existing solutions, local engine verification |

## Folder layout

```text
P09_StateHandoff\
├── README.md        ← this page
├── Explanation\     ← 00–12, Phase2_Raw_Notes\, Research\
├── Benchmarks\      ← Raw\ + Summaries\, and the run protocol
├── Media\           ← diagrams, screenshots, demo video
├── Tools\           ← build/package/smoke scripts; CPU pinning; later the stats and run scripts
└── StateHandoff\    ← the Unreal Engine project (.uproject, Source, Plugins, Config)
```
