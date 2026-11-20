# 00 · Project Overview: StateHandoff

> *"This was the problem. This was my investigation. This was my solution. These were my technical decisions. This was how I measured the results."*

**Status (2026-11-21):** Phases 1–4 are done: problem analysis (`01`), verified research (`01b`), final architecture v1.0 (`02`), build steps (`03`), and the UE project foundation (empty modules, built and packaged with Mass enabled, `09`). No gameplay code exists yet, and no results exist yet.

---

## A. Identity

| | |
|---|---|
| **Name** | StateHandoff: gameplay state that survives LOD switches |
| **Technical category** | Large-scale gameplay architecture · data-oriented design · correctness under representation LOD |
| **Main objective** | When Mass switches an agent between actor, instanced mesh and nothing, the agent's non-spatial gameplay state (health, cooldowns, AI memory, reservations) must stay exactly right: never lost, duplicated, rolled back or leaked to another agent. Prove it with an automated checker over tens of thousands of transitions, at a measured cost |
| **Difficulty** | ★★★★★ Expert: Experimental engine APIs, an engine whose actor lifetime is decoupled from the representation, and a correctness claim that needs its own measuring instrument |
| **Core UE technologies** | MassEntity (5.8 layout: `MassEntity`, `MassCore`, `MassSignals`), MassGameplay (`MassRepresentation`, `MassActors`, `MassLOD`, `MassSpawner`), `UMassRepresentationActorManagement`, actor pooling (`IMassActorPoolableInterface`), Mass processors and traits, CSV profiler, Unreal Insights with the Mass trace (MassInsights) |
| **Relation to existing work** | Builds on the engine's representation hooks and on Epic's 2026 PersistenceLab hydrate/dehydrate pattern, which it credits and measures as a baseline. What it adds: a per-field ownership contract tied to the representation commit, a generic hand-off, an invariant checker, and a controlled comparison. See `01` §3 and `01b` §6 |

## B–D. Problem, solution, architecture

- Problem, root-cause hypotheses, success criteria and the measurement plan: **[01_Problem_Analysis.md](01_Problem_Analysis.md)**
- What the engine really does, and what that changed: **[01b_Research_Findings.md](01b_Research_Findings.md)**
- Solution and architecture (final v1.0): **[02_System_Architecture.md](02_System_Architecture.md)**

**The solution in three sentences.** Each piece of state declares an **owner per representation**: the actor while the entity is bound to an enabled actor, the entity otherwise. Ownership moves only at the **representation commit**, the point in the frame where Mass has decided the representation, through one generic capture/restore path that covers every way Mass releases an actor. An **invariant checker** replays the same gameplay events on an independent reference model and classifies every mismatch.

## E. Technical demonstration

**What the viewer sees**
- An arena with thousands of Mass agents. Near ones are actors, mid-range ones are instanced cubes, far ones are not drawn.
- Each agent is tinted by health. A number above each actor shows its cooldown and alert level.
- A counter overlay: transitions so far, divergence incidents by class, the active configuration (B0, B1, B2, B3 or SH).
- The camera flips between two hot spots, so hundreds of agents change representation at once.

**Before** (B0 stock, then B1 and B2)
- B0: agents come back at full health after every round trip through ISM. The incident counter climbs with the transition counter.
- B1 and B2: whatever the measurements show, shown exactly as measured. If they are clean, the demo says so.

**After** (SH)
- The same flips. Health tints and cooldowns carry over every switch.
- The incident counter stays at 0 while the transition counter passes tens of thousands.
- A fault-injection toggle switches off one hand-off path, and the counter starts climbing: the checker is really checking.

**Recorded material:**
- a side-by-side video of B0 and SH on the same camera path;
- incidents per 1,000 transitions per configuration, with the transition counts;
- hand-off cost per transition (median, p99) and flip-window frame times;
- a memory table per agent per representation;
- an Unreal Insights capture of one flip frame.

## G. Portfolio presentation (produced in Phase 10)

1. Problem statement, one paragraph (from `01` §1), with Epic's own words on GAS and LOD changes.
2. The investigation: what Phase 2 found in the engine (actor lifetime vs representation, pooling, kept-disabled actors) and what that changed. Including the honest correction: the hand-off idea is not new.
3. The architecture diagram (`02` §5) and the per-frame flow.
4. The key decisions and rejected alternatives: commit-time vs lifecycle hand-off, generic reflection channels vs per-class code, the version-number checker.
5. Correctness and cost against **all four** baselines, with the full environment and method.
6. What didn't work or wasn't better, stated plainly.
7. Video, source repository and the complete `Explanation/` folder.

## H. Industry relevance

**Skills demonstrated:**
- data-oriented design with Mass: fragments, traits, processors, archetypes, phases and processor groups;
- reading engine source to find where lifetime and ownership really change, and designing around Experimental APIs;
- object-lifetime safety (weak pointers, pooling, deferred destruction, world teardown);
- designing a **test oracle** and invariant checker, and proving the checker catches faults (fault injection);
- statistical benchmarking of rare events (rates per 1,000 transitions with an upper bound for zero counts);
- explaining trade-offs: entity-owned vs actor-owned state, per-frame mirroring vs event-driven hand-off.

**Studios and roles that ask for this:**
- CD PROJEKT RED's Mass/ECS roles: vehicle AI, navigation at scale, "spawning and systemic open world", "debug tools … for non-deterministic systemic gameplay";
- Ubisoft and WB Games Montréal: behavioural LOD and agent counts;
- teams shipping open-world crowds on UE5 (Epic's Witcher 4 demo used Mass).

Sources: [research/01](Research/01_job_market.md) (C2, C3, C4, U12, WB2), [research/02](Research/02_technical_challenges.md) [T2-A–D].

**Interview questions it prepares you for:**
- "What does Mass do when an agent goes from actor to instanced mesh? What happens to the actor?"
- "Your NPC's health resets when the player looks away. How do you find the cause, and how do you make sure it never comes back?"
- "Where would you store gameplay state for an agent that can be an actor, an ISM or nothing?"
- "Why is `EndPlay` the wrong place to save an agent's state in Mass?"
- "How do you test something that fails once in ten thousand transitions and depends on timing?"
- "How do you prove that a checker that reports zero errors can actually see errors?"
- "What does pooling change about an actor's lifecycle?"
- "When would you not use this system and keep the state on the entity instead?"
