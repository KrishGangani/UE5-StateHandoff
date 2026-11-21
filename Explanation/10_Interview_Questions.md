# 10 · Interview Questions: StateHandoff

Answers grounded in this project's code (`05`) and data (`09`). Numbers are from single functional runs unless stated.

**1. What problem does this solve?** Mass switches an agent between an actor, an instanced mesh and nothing. The engine moves the transform across a switch, not the state that actor-side systems keep (health, cooldowns, AI memory, reservations). In the stock setup (B0) the checker found about 912–978 divergence incidents per 1,000 transitions: almost every actor appearing or going away loses or rolls back some state.

**2. Why not just hand off in `BeginPlay` / `EndPlay`?** That's baseline B1. Mass maps the actor to its entity only after `BeginPlay` (so hydration must retry), and destroys released actors in a time-sliced queue at the end of PrePhysics; actors past the slice are destroyed on later frames. So `EndPlay` runs late: B1 without pooling showed 524 incidents per 1,000, half *lost* (the game read the stale entity), half *duplicated* (the late copy brought back an old history). With pooling it showed 0, because Mass offers every queued actor to the pool in the release frame and a pool-aware `PrepareForPooling` runs in time.

**3. What does StateHandoff do instead?** One processor runs right after Mass commits the representation (`ExecuteAfter VisualizationProcessing`) and before releases are processed. Per entity it compares the actor that should own the state now with the one bound before; on a change it captures from the old actor and restores into the new one, by reflection over registered channels. Result: 0 incidents in 29,871 transitions.

**4. How do you know the checker is right?** Three ways: unit tests with hand-made structs (Lost / Duplicated / Corrupted / Identical); two configurations where nothing can go wrong show 0 (a parked camera in S2b; B3, entity-owned, in S3a over 60,213 transitions); and it does find the known failures (B0, B1 without pooling, B2). Fault injection, the planned fourth proof, was not built.

**5. Why versions, and why one numbering per agent?** A version per channel turns "are these equal?" into three answers: behind = lost writes, ahead = a write the reference doesn't have, equal but different = corrupted. The code review found that per-copy counters became incomparable after the checker re-synced its oracle, so each write is stamped with a per-agent, per-channel sequence number instead (test `StateHandoff.Demo.SwitchBack`).

**6. Why is the checker's denominator its own?** Transitions are counted from the checker's own fragment in every configuration, so the system under test can't influence "per 1,000 transitions".

**7. What would you measure next?** The planned batches: several rotated runs per configuration with pooling off and on, timing with the checker off, a parked camera for steady-state cost, per-transition hand-off times, and the archetype memory table (`03`).

**8. What are the limits?** Single-player only; channels must be `UPROPERTY` structs on a component; an actor released immediately (template swap) or destroyed by gameplay while bound is not captured in v1; the demo's channels are not real GAS or blackboards (`02` §12).
