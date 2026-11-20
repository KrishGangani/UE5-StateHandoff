# P09 · StateHandoff: gameplay state that survives LOD switches

**Domain:** Large-scale gameplay architecture · data-oriented design · **Difficulty:** ★★★★★ · **Engine:** UE 5.8.4

> **Status: planned; Phases 1–3 (problem analysis, research, architecture) are starting.** The ten projects are developed in parallel, and every Unreal Engine build and benchmark runs one at a time on the same machine, so measurements stay clean. Everything below is the plan. The success criteria are **targets fixed before any code exists, not results**. Real numbers will only come from controlled before/after runs, each traceable to a raw benchmark batch.

## The problem

Mass switches agents between actor, instanced mesh and nothing. Shipped translators sync only transform, movement, orientation and collision, so health, AI memory, cooldowns and reservations are hand-written per project with nothing checking them

## Proposed solution

An explicit state-ownership contract per representation, a generic hand-off, and an **automated invariant checker** that counts divergence across thousands of transitions

## Main Unreal Engine technologies

MassEntity, MassGameplay representation LOD, translators, Instanced Actors, MassInsights, StateTree (Mass). Experimental, version-pinned

## Success criteria (targets, defined before implementation)

Divergence errors per 1,000 transitions (target 0); transition cost µs; frame spikes during mass LOD flips; memory per agent per representation

## Existing work it must be compared against

Mass representation LOD, translators, Instanced Actors, MassInsights, CitySample

**The gap it targets:** A generic hand-off contract for non-spatial state with an invariant checker (gap strength: Strong).

**Main risk:** Mass is Experimental and restructured in 5.7 and 5.8. Single-player only (no Iris for Mass in 5.8)

## Why studios care

CDPR's three Mass/ECS AI and vehicle roles; Witcher 4 demo (Mass, 300 NPCs); data-oriented crowd talks [T2-A, T2-B]

## Method

Every project in the portfolio follows the same ten phases: problem analysis → research → architecture → foundation → implementation → explanation → testing → profiling → review → portfolio. Each phase is documented in `Explanation/` (files 00–12, including a line-by-line code walkthrough), and every claimed improvement must be measured against the existing solutions above on the same workload. Engine: Unreal Engine 5.8.4, C++.

## Folder layout

```text
P09_StateHandoff\
├── README.md        ← this page (becomes the project's portfolio page)
├── Explanation\     ← 00–12, created when the project starts
├── Benchmarks\      ← Raw\ + Summaries\
├── Media\           ← diagrams, screenshots, demo video
├── Tools\           ← analysis and run scripts
└── StateHandoff\  ← the Unreal Engine project (created in its Phase 4)
```
