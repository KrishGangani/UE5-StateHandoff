# Phase 2 raw notes

These are the working notes behind [01b_Research_Findings.md](../01b_Research_Findings.md). They keep every statement in the findings traceable to the exact engine line or web page it came from.

| File | Scope |
|---|---|
| [A_representation_internals.md](A_representation_internals.md) | How Mass switches an entity between actor, instanced mesh and nothing: the representation processor, actor management, the actor spawner, pooling, destruction budgets |
| [B_translators_and_hooks.md](B_translators_and_hooks.md) | Every shipped translator and the fragments it syncs; every engine hook that could carry state across a switch; where Mass StateTree and Smart Object state lives |
| [C_framework_and_measurement.md](C_framework_and_measurement.md) | Plugin and module layout in 5.8.4, processor registration, phases, determinism, the Mass trace channel, MassInsights, memory commands, CSV categories, replication (Iris) |
| [D_existing_work.md](D_existing_work.md) | Existing solutions and patterns on the web, with sources |

**Engine:** UE 5.8.4 (CL 58902052), read from `C:\Program Files\Epic Games\UE_5.8\Engine` on 2026-11-21. Paths are relative to `Engine/`. Plugin paths are shortened as follows:

| Short form | Full path under `Engine/` |
|---|---|
| `MG/` | `Plugins/Runtime/MassGameplay/Source/` |
| `MAI/` | `Plugins/AI/MassAI/Source/` |
| `IA/` | `Plugins/Runtime/InstancedActors/Source/InstancedActors/` |
| `ME/` | `Source/Runtime/MassEntity/` |

**The findings file is authoritative.** These notes are the audit trail.
