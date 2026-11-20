# D · Existing work (web), read 2026-11-21

Evidence tags as in the portfolio research: **[opened]** = I read the page or file; **(snippet only)** = seen as a search summary only; **(unverified)** = could not be confirmed.

## D1. Epic staff on actor ↔ Mass hand-off

**James Keeling (Epic), forum thread "Mass Entity framework questions", reply of 13 Feb 2026** [opened]
URL: https://forums.unrealengine.com/t/mass-entity-framework-questions/2706080
- The question (Danylo, 20 Jan 2026): hydrating and dehydrating NPCs between an actor (AI controller + StateTree) and Mass (+ MassStateTree), syncing data back to Mass when the actor is destroyed. The poster already used a custom `RepresentationActorManagement` with `OnPostActorSpawn` "same as in CitySample demo" and looked at MassTranslator + MassTrait.
- Reply, on moving data back: "I do not believe we have any example of moving data from the Actor back to Mass when destroying the Actor." He suggests `GetOnMassAgentComponentEntityDetaching` to send data back.
- On behaviour across a swap: use a task to transition into the right state "when swapping representation"; continuing a Smart Object interaction across a swap is hard; "You may also want a larger hysteresis buffer for swapping representation."

**James Keeling (Epic), forum thread "Synchronising and updating actor transforms in Mass Entity performance", reply of 15 May 2025** [opened]
URL: https://forums.unrealengine.com/t/synchronising-and-updating-actor-transforms-in-mass-entity-performance/2573360
- "The main issue is the current GAS implementation is built around the AbilitySystemComponent." … "There are issues with needing to track the state of the ASC, effects, abilities, and stats persist between LOD changes since Actor representation changes."
- The original poster (UE 5.5.4, about 500 moving entities) measured about **3 ms of game-thread time per frame** in `MassTransformToActorCapsuleTranslator`. A user's number, conditions as stated by them; not reproduced here.

**James Keeling (Epic), Apr 25 2025** (from [research/03](../Research/03_existing_solutions_gap_analysis.md) §3.4): "Our approach has been to keep the entity while removing the Actor representation when switching LOD"; BehaviorTree and GAS will likely not be ported to Mass.

## D2. Epic's "The Player Was Here" talk and the PersistenceLab sample

- **Talk:** Zhi Kang Shao (Epic Developer Relations), "The Player Was Here: Persisting World State and Actor LOD Systems", Unreal Fest Chicago 2026. Recording: https://www.youtube.com/watch?v=KBn62trwkLw (not watched). The search summary says it covers persistence *and* "hydration", meaning switching far entities to lighter representations, with examples of restoring GAS and StateTree state; the speaker describes it as existing UE tech "collected into one spot" (snippet only).
- **Text version:** https://dev.epicgames.com/community/learning/tutorials/1mj4/unreal-engine-saving-world-state-with-actors-instanced-actors-and-mass. The page body did not render when fetched (snippet only). Search summary: on dehydration the GAS health is synced into a `FHealthFragment` because the attribute set is destroyed with the actor, and copied back on hydration; the code covers both Instanced-Actors-spawned actors and Mass high-LOD actors.
- **Sample:** https://github.com/ZKShao-EpicGames/Sample-PersistenceLab [opened: README, file tree, one source file]. "UE 5.8+"; "compatible with UE 5.8.0"; "This code has not been battle tested in a shipped title." **No license file** in the repository root, so **no code is copied from it**; only the pattern is described.
  - `Plugins/PersistenceExamples/Source/PersistenceExamples/Private/Mass/NPCMassActorComponent.cpp` [opened via raw.githubusercontent.com]:
    - **Hydrate in `BeginPlay`:** look up the owner's entity through `UMassActorSubsystem`; if not found yet, **retry on the next tick**, giving up after a retry budget. Copy `FHealthFragment` (Current, Max), `FNPCNavTargetFragment`, `FNPCStateFragment` into the actor; seed the GAS `UHealthAttributeSet`; add `FNPCHydratedTag` to the entity; signal the StateTree.
    - **Dehydrate in `EndPlay`:** write the GAS health and the nav/state values back to the fragments, remove the tag, signal the StateTree.
    - A save-time flush writes the same values without leaving.
  - The engine facts in [A](A_representation_internals.md) explain the retry: `BeginPlay` runs inside `SpawnActor`, **before** `OnPostActorSpawn` stores the actor→entity mapping (`MassActorSpawnerSubsystem.cpp:334` vs `MassRepresentationActorManagement.cpp:108`).
  - **What the sample does not show (from the files opened):** any handling of pooled actors (no `BeginPlay`/`EndPlay` for them, A4), of kept-disabled low-res actors, of destruction deferred by the budget, an ownership contract, or any check or measurement.

## D3. CitySample's push-on-spawn pattern

- vrealmatic, "Crowd Visualization Trait in Mass Framework" [opened]: https://vrealmatic.com/unreal-engine/mass/crowd-visualization. Shows a representation-actor-management subclass whose `OnPostActorSpawn` calls `Super`, then casts the actor to `IMassCrowdActorInterface` and calls `OnGetOrSpawn(EntityManager, MassAgent)`; also overrides `TeleportActor`. Written "on the UE 5.3 API". **No despawn code** is shown. The class name in the page's header (`USampleMassCrowdRepresentationActorManagement`) differs from the `.cpp` (`UCitySample…`), so it is a re-typed example, not the CitySample source.
- CitySample itself was **not downloaded** (project rule: reference only, no large samples).

## D4. Community and Fab

| Item | What it claims | Evidence |
|---|---|---|
| Fab "Scalable Modular Behaviour – Mass Entity Framework" | "Zero-Stutter Mass/Actor Syncing: Seamless interaction transitions"; v0.1.2 "Blueprint actors controlled by Mass now have no transition". Which state is synced is **not stated**. "Still in development"; "Do not use Mass BP Actors with replication yet"; benchmark claims about animation time, not hand-off correctness | [opened] https://www.fab.com/listings/e6e24564-47af-4cdf-84cd-bbded2c2cb9c |
| Fab "MassAPI Plugin" | A Blueprint/C++ wrapper API around Mass; actor syncing is not its focus | (snippet only) https://www.fab.com/listings/0d985811-5744-45de-9a5e-be71e8336fc3 |
| "Mass Battle" crowd plugin | "auto swap between particle and actor" | (snippet only) forum thread https://forums.unrealengine.com/t/i-made-a-plugin-that-makes-creating-high-performance-crowd-battle-super-easy/2674491 |
| MassSample (Megafunk), MassCommunitySample (getnamo), x157 notes | Teaching samples; representation switching explained; no hand-off checking or numbers | [research/02](../Research/02_technical_challenges.md) [T2-C]; [research/03](../Research/03_existing_solutions_gap_analysis.md) §3.3 |
| Hitman: Absolution crowds; AC Unity AI recycling | LOD and recycling of agent representations at scale; studio-internal hand-off not public | [research/02](../Research/02_technical_challenges.md) [T2-A], [T2-B] |

Fab coverage is not exhaustive (the same limit as research/03 §13). Studio-internal systems are not public, and a web search can't prove absence.

## D5. What remains after this survey

| Piece | Exists? |
|---|---|
| Hooks for moving state at spawn/despawn | **Yes**, several, in the engine (B4 in [B](B_translators_and_hooks.md)) |
| A worked pattern for one actor class (hydrate on `BeginPlay`, dehydrate on `EndPlay`) | **Yes**: Epic's PersistenceLab (D2) |
| A per-frame mirror pattern (translators) and a push-on-spawn pattern | **Yes**: engine translators for spatial data (B2); CitySample-style `OnPostActorSpawn` (D3) |
| An explicit, per-field **ownership contract** whose hand-off is tied to the representation **commit**, not to actor lifetime | **Not found** |
| A **generic** hand-off (declare a state field once; no per-actor-class code) | **Not found** for Mass. Instanced Actors offers per-component, hand-written hooks |
| An **automated checker** counting divergence over thousands of transitions, and a measured comparison of the patterns (correctness, cost, flip spikes, memory) | **Not found** |
