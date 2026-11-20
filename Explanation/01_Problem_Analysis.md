# 01 · Problem Analysis: StateHandoff

**Phase:** 1 (Problem Analysis), written 2026-11-21 and revised the same day after Phase 2 (evidence in [01b_Research_Findings.md](01b_Research_Findings.md); references like `01b #3` point to its §1 table)
**Engine:** UE **5.8.4** (CL 58902052). Mass is **Experimental** in this version and was restructured in 5.7 and 5.8, so every engine fact below is pinned to 5.8.4 and must be re-checked before any engine upgrade.
**Status of every claim:** **[verified]** (read in engine source or a cited source), **[hypothesis]** (to be confirmed by measurement), or **[target]** (a pass/fail threshold, not a result).

---

## 1. The problem in one paragraph

Unreal's Mass framework lets a game simulate thousands of agents as lightweight **entities** and draw each one in the cheapest way that still looks right: a full **actor** when close, an **instanced static mesh** (ISM) further away, and **nothing** when far or hidden. Mass switches each entity between these representations every time its level of detail (LOD) changes, which in an open world happens constantly. The engine copies the agent's **position, movement and orientation** across every switch [verified, `01b #1`]. Everything else an actor-side system keeps is the game's problem: health in an ability system component, cooldowns, what an AI controller remembers about the player, the interaction slot it reserved. If that state lives on the actor, it can be **lost** when the actor goes away, **duplicated** or **rolled back** when two copies exist, or **leaked** to another entity when the actor is pooled and reused. Today each project writes its own hand-off code per actor class, and nothing checks that it is correct.

## 2. Where this happens in real games

| Evidence | What it shows | Source |
|---|---|---|
| Epic, on GAS with Mass (May 2025) | "There are issues with needing to track the state of the ASC, effects, abilities, and stats persist between LOD changes since Actor representation changes." | [D1](Phase2_Raw_Notes/D_existing_work.md) [verified, forum opened] |
| Epic, on hydrate/dehydrate (Feb 2026) | "I do not believe we have any example of moving data from the Actor back to Mass when destroying the Actor." | [D1](Phase2_Raw_Notes/D_existing_work.md) [verified] |
| Epic's own 2026 talk and sample | "The Player Was Here" (Unreal Fest Chicago 2026) and PersistenceLab show a hand-written hydrate/dehydrate pattern for one NPC class; "not battle tested in a shipped title" | [D2](Phase2_Raw_Notes/D_existing_work.md) [verified] |
| CitySample; Lego Fortnite | Mass crowds and traffic; Lego Fortnite uses Instanced Actors and MassGameplay | [research/02](Research/02_technical_challenges.md) [T2-C]; [research/03](Research/03_existing_solutions_gap_analysis.md) §3.1 |
| The Witcher 4 UE5 tech demo (2025) | Mass as one of five showcased systems; a market scene with 300 NPCs | [research/02](Research/02_technical_challenges.md) [T2-D] (snippet only for the NPC count) |
| Hitman: Absolution; AC Unity | Crowd LOD and AI "recycling" at scale: 40 real AIs presenting 10,000 crowd NPCs | [research/02](Research/02_technical_challenges.md) [T2-A], [T2-B] |
| Job postings, 2026 | CDPR's three Mass/ECS roles (vehicle AI; navigation at scale; "spawning and systemic open world"); Ubisoft and WB Montréal ask for behavioural LOD | [research/01](Research/01_job_market.md) §2 (C2, C3, C4, U12, WB2) |

**What these share:** representation LOD is standard for large agent counts, and the moment an actor-side system holds gameplay state, the switch becomes a correctness problem, not just a rendering one.

## 3. What already exists, and what it doesn't do

| Existing work | What it does | What it doesn't do | Evidence |
|---|---|---|---|
| **Mass representation LOD** (`UMassVisualizationProcessor`, `FMassRepresentationParameters`) | Picks the representation per LOD; spawns, re-enables, disables, pools and releases actors; time-slices spawning (1.5 ms) and destruction (0.5 ms) | Moves no gameplay state. Actor lifetime is decoupled from the representation (deferred, budgeted destruction; kept-disabled actors) | `01b #3–5` |
| **Translators** (`UMassTranslator` subclasses, sync traits) | Copy transform, location, orientation, velocity, capsule transform every frame | Nothing non-spatial. The only other translator (`UMassTranslator_BehaviorTree`) is an empty stub | `01b #1` |
| **Engine hooks**: `OnPreActorSpawn`/`OnPostActorSpawn`, trait object initializers, the "entity detaching" delegate, pool callbacks | Points where a game *could* move state | Scattered, with different timing and conditions; none defines an owner; none is called on every path | `01b #9` |
| **Mass-native systems**: Mass StateTree, Mass Smart Objects | Keep their state in entity fragments or entity-keyed subsystems | Nothing to fix: this state already survives a switch. Out of scope | `01b #2` |
| **Instanced Actors** | Hydrates stationary instances into actors; per-component hooks to write fragments and persistence data | Per-component, hand-written; for stationary props; unchecked | `01b` §3 |
| **MassInsights** | Shows phases, processor chunks and archetype moves in Unreal Insights | Knows nothing about gameplay state or ownership | `01b` M-1 |
| **Epic PersistenceLab pattern** (2026) | Per actor class: hydrate in `BeginPlay` (with a retry), dehydrate in `EndPlay`; a "hydrated" tag | Not generic; not tied to the representation commit; pooling and kept-disabled actors not covered in the files read; no checks, no numbers | `01b #10` |
| **CitySample `OnPostActorSpawn`** | Pushes entity data into a freshly spawned actor | One direction only; not called when a kept actor is re-enabled | `01b` §5 |
| **Fab: Scalable Modular Behaviour** | Advertises "Zero-Stutter Mass/Actor Syncing" | Doesn't say which state; in development; no correctness numbers | `01b` §5 |

**The gap after Phase 2** (`01b` §6): the hooks and a worked per-class pattern exist. Among the public sources reviewed, none offers (1) a per-field **ownership contract** tied to the **representation commit**, (2) a **generic** hand-off, (3) an **automated checker** that counts divergence across thousands of transitions, or (4) a **controlled measurement** of the existing patterns. Gap strength: **medium** for the mechanism, **strong** for the contract, checker and measurement. We present StateHandoff as building on Epic's hooks and pattern, never as a new idea.

## 4. Root-cause hypotheses

The suspected root cause is **implicit ownership**: two copies of the same state exist (on the entity and on the actor), nothing says which one is authoritative at a given moment, and the code that copies between them runs on events (`BeginPlay`, `EndPlay`, spawn, every frame) that don't coincide with the moment Mass actually commits a new representation. Each hypothesis below gets a test, and the baselines run before any StateHandoff code.

| # | Hypothesis | Why we suspect it (Phase 2 evidence) | How we test it |
|---|---|---|---|
| H1 | **Hand-off tied to actor lifetime runs at the wrong moment.** `EndPlay` can run frames after the representation changed, and never runs for pooled actors | Release is queued to the end of the phase and budgeted; past the budget, actors are destroyed on a later frame; pooled actors get no `BeginPlay`/`EndPlay` (`01b #3–4`) | B1 (lifecycle pattern, pool-aware) with flips that return within the destruction window, pooling off and on. Count incidents by class |
| H2 | **Kept-disabled actors keep a second live copy** | With the default `bKeepLowResActors`, an actor survives ISM periods; only its actor tick is off, its components still tick (`01b #5`) | B0–B2 and SH with `bKeepLowResActors` on vs off. Count incidents that start during ISM periods |
| H3 | **Per-frame mirroring loses the writes made after the copy in the release frame, and costs per actor per frame** | Translators are per-frame processors (`01b #1`); a user measured about 3 ms per frame for one transform translator at about 500 entities (their conditions, `01b` §7) | B2: incidents in release frames; game-thread cost vs actor count with zero transitions (camera parked) |
| H4 | **Pooled actors carry the previous entity's state** | Retrieval calls `PrepareForGame`, not `BeginPlay`; a push-on-spawn hand-off that skips some field leaves the old value (`01b #4`) | B0 and B1 with pooling on: incidents classified **Corrupted** (version matches, payload doesn't) |
| H5 | **LOD flips concentrate transitions into single frames**, so hand-off cost shows up as frame spikes | Spawns are budgeted, but disable and release decisions are made for every entity in the frame its LOD changes (`01b` §7) | Scripted camera teleports. Frame time and hand-off µs in the frames after each flip, per config |

**Not a cause we accept without checking:** "Mass loses state randomly". Every incident the checker reports is classified (lost, duplicated, corrupted, reservation double-held or leaked) and traced to a transition kind, so a fix addresses a mechanism, not a symptom.

## 5. The cost of leaving it unsolved

These are the bug shapes this problem produces. They are illustrations, not measurements.
- **Lost state:** an enemy you wounded is back at full health after you looked away and back; an AI forgets it saw you.
- **Rolled back or duplicated state:** an ability is usable again before its cooldown ended, or a damage event is applied twice.
- **Leaked state:** a pooled actor reused for a different NPC carries the first one's health or its interaction slot; two NPCs "own" one bench, or a slot stays reserved forever and the queue never moves.

They are expensive because they are **rare, timing-dependent and not reproducible**: they depend on budgets measured in wall-clock time, pooling, and how fast the player turns the camera (`01b #8`). Without an automated check, a team finds them in playtests, one report at a time.

## 6. What "solved" means: success criteria, defined now

### 6.1 The workload (identical in every run)

- **Agents:** N ∈ {1,000, 4,000} Mass entities placed by a **fixed random seed** on a jittered grid in a flat arena. Agents stand still, so translators are not needed and only the representation changes. (Movement is what Mass already handles, `01b #1`.)
- **Representation:** `UMassMovableVisualizationTrait` with a demo actor (one template for high and low resolution, so the kept-disabled path is exercised), an engine cube as ISM, and LOD counts and distances fixed by calibration in Phase 5 so that each run produces **thousands of actor transitions**. The default counts allow only about 150 actors (`01b #14`).
- **State per agent (four channels, the demo's "actor-side systems"):**
  1. **Health:** current and max; damage and heal events.
  2. **Cooldowns:** two abilities, stored as absolute "ready at" world times, so no simulation is needed while dormant; "use ability" succeeds only when ready.
  3. **AI memory:** last-seen target position and time, alert level; "perceive" events.
  4. **Reservation:** the id of the interaction slot the agent holds, from a shared pool of M slots; "claim" and "release" events. Cross-agent rule: a slot has at most one holder, and the holder agrees it holds it.
  Each channel carries a **version number** incremented by every write. That lets the checker classify divergence exactly (§6.3).
- **Events:** each frame, a seeded fraction of agents receives one event at each of **two points**: a *simulation* point (a Mass processor before the representation group: entity-side gameplay) and an *actor* point (after physics: actor-side gameplay such as hits). Each configuration routes the event to the copy its own rule says is authoritative.
- **Camera:** a fixed path, plus **flips**: at fixed times the camera teleports between two hot spots and back, so many entities change representation in the same frame, and some return within a few frames (H1, H5).
- **Pooling:** a parameter, off or on (the demo actor implements `IMassActorPoolableInterface`).

### 6.2 Compared configurations (all in the same build, chosen with `-SHConfig=`)

| ID | Configuration | Why it is included |
|---|---|---|
| B0 | **Stock:** state on the actor component, initialised from defaults on every spawn; no hand-off | What Mass gives you out of the box (only spatial data is translated) |
| B1 | **Lifecycle hand-off, pool-aware:** hydrate in `BeginPlay` (retry until the entity mapping exists) and in `PrepareForGame`; dehydrate in `EndPlay` and in `PrepareForPooling`; a "hydrated" flag routes writes | Epic's 2026 PersistenceLab pattern, made pool-aware so it is not a strawman (`01b #10`) |
| B2 | **Mirror:** a translator-style processor copies every channel actor → entity every frame; `OnPostActorSpawn` pushes entity → actor | The translator pattern applied to gameplay state, plus CitySample's push on spawn |
| B3 | **Entity-owned:** state only in fragments; actor-side code reads and writes the entity | Epic's "keep the entity" advice. The reference for when you can rewrite the actor-side system; zero hand-off by construction |
| **SH** | **StateHandoff** (all four channels `ActorWhileBound`, so the ownership is the same as B0–B2) | Our system: contract + commit-time hand-off + checker |

### 6.3 Metrics

| Metric | Unit | Tool |
|---|---|---|
| **Divergence incidents per 1,000 representation transitions**, split by class: *lost* (version behind), *duplicated* (version ahead), *corrupted* (version equal, value different), *reservation double-held*, *reservation leaked* | count / 1,000 | Our checker; transitions counted by the checker itself from `FMassRepresentationFragment`, identically in every config |
| Transitions per run, by kind (actor→ISM, ISM→actor, ISM→none, none→ISM, actor→none, none→actor, template swap) | count | Checker CSV stats |
| **Hand-off cost per transition** (SH): median, p95, p99, max | µs | Timestamps around each capture/restore, collected per transition and summarised at the end of the run |
| **Frame spikes during flips:** frame time and game-thread time, median/p95/p99/max in the 60 frames after each flip; frames > 33.3 ms | ms, count | CSV profiler; our `BenchFlip` CSV events mark the windows |
| Steady-state cost (camera parked, no transitions): game-thread median/p95 | ms | CSV profiler |
| **Memory per agent per representation:** archetype bytes per entity; actor + component KB per live actor; ISM instance data | bytes, KB | `mass.LogArchetypes`, `obj list class=…` (`01b` M-2) |
| Mass's own actor counters | count | CSV `MassActors/NumSpawned`, `NumPooled` (`01b` M-3) |

### 6.4 Pass/fail criteria [target]

| # | Criterion | Threshold |
|---|---|---|
| SC1 | **Correctness** | SH shows **0** divergence incidents in every correctness run, at N = 4,000, pooling off and on. Each config must accumulate **≥ 20,000 transitions** per batch; with 0 incidents in 20,000 transitions, the 95% upper bound is about 0.15 per 1,000 ("rule of three": 3/n). Fewer transitions = "insufficient sample", not a pass |
| SC2 | **The checker detects real faults** | Fault injection: disabling each SH hand-off path in turn (commit capture, commit restore, pre-spawn capture, end-play safety net) produces **≥ 1 incident per 1,000 transitions** in a run that exercises that path. **100%** of injected faults detected. B0 must also show incidents (sanity) |
| SC3 | **The problem is real (decided by data)** | At least one established pattern, B1 or B2, shows > 0 incidents per 1,000 transitions under default engine settings. **If neither does, we report it plainly** and the contribution is the checker and the measurements, not a fix (`01b` §6) |
| SC4 | **Transition cost** | SH hand-off per transition (four channels, capture or restore): **median ≤ 2 µs, p99 ≤ 10 µs** |
| SC5 | **Flip spikes** | At N = 4,000 with flips: SH's p99 frame time in flip windows ≤ B0's + **0.5 ms**, and SH has no more frames > 33.3 ms than B0 |
| SC6 | **Steady-state overhead** | Camera parked, no transitions, N = 4,000: SH game-thread median ≤ B0 + **0.10 ms**. B2's mirror cost is reported against the same bound |
| SC7 | **Memory** | SH's bookkeeping adds **≤ 32 bytes per entity** over B3 (archetype "Bytes / Entity") and nothing per actor beyond B0. Memory per agent per representation is reported for every config |
| SC8 | **Lifetime safety** | **0** crashes, ensures or errors from our log categories across all runs and the edge-case tests: entity destroyed while bound; actor destroyed by gameplay while bound; world torn down mid-flip; pooled actor reused for a different entity |

**Why these numbers.** SC4: capturing four small structs is a few copies and lookups, so 2 µs leaves room for the engine lookups; at 2 µs, 1,000 transitions in one frame cost 2 ms, which SC5 then bounds. SC5: 0.5 ms is 3% of a 60 fps frame. SC6: the per-frame detection scan must cost no more than about 25 ns per entity. SC7: a weak actor pointer (8 bytes) plus flags fits in 16; 32 leaves room for alignment.

## 7. Measurement plan (summary; the full protocol goes in `09_Testing_And_Benchmarks.md`)

- **Build:** packaged **Development**, standalone, never PIE. The launcher engine can't build Test (established in UE5-UpdateBudget), and Development keeps the CSV profiler and the Mass trace (`01b` M-1). Editor numbers, if any, are labelled "editor".
- **Determinism:** fixed seed and fixed camera path; **fixed step `-deterministic -FPS=60`**, real GPU, windowed 1280×720, `-novsync`, `-nosound`. The remaining non-determinism (Mass's wall-clock spawn and destroy budgets, `01b #8`) is why every result is normalised per 1,000 transitions and the transition count is reported per run.
- **Capture window:** opened and closed by the game with CSV events (`BenchStart`, `BenchEnd`, plus `BenchFlip` at each flip): `-csvStartOnEvent=BenchStart -csvCaptureOnEventFrameCount=N -ExitAfterCsvProfiling`, `-traceautostart=0`.
- **CPU pinning:** this i7-14650HX is hybrid. Every timed run is pinned to the **performance cores** (logical 0–15, mask `0xFFFF`). The run script pins **itself** once with `pin_self(performance_core_mask())` from `Tools\cpu_affinity.py` (copied from UE5-UpdateBudget), before it records the environment or starts anything, so every game it starts inherits the mask from its first instruction. It records `applied_mask()` (read back from Windows) in the environment, starts the real game executable (`Build\Windows\StateHandoff\Binaries\Win64\StateHandoff.exe`) directly with `subprocess.Popen`, waits with a timeout and kills the process tree in a `finally` block. Not `cmd /c start /wait`, which always returns exit code 0 (found in UE5-TestTrust). Reason for pinning: UE5-UpdateBudget's baselines showed Windows placing the game thread on an E-core (runs about 1.6× slower and bimodal).
- **Run protocol:** one discarded priming run per batch; then interleaved runs with the config order rotated every round; **runs per combination = the number of configs (5)**, so every config sits in every order position once. Combinations no criterion needs are skipped (planned matrix in `03`).
- **Two kinds of runs:**
  - *correctness runs* with the checker on (SC1–SC3, SC8);
  - *timing runs* with the checker off (SC4–SC6), so the instrument doesn't add cost.
- **Statistics:** median, p95, p99 per run (linear-interpolation percentiles), then the median across runs with [min–max]. Improvements are claimed only when the ranges don't overlap. FPS is never evidence.
- **Environment record with every batch:** CPU, GPU and driver, RAM, OS build, engine version and CL, build configuration, resolution, scalability, power plan, AC power, affinity mask, exact command lines.
- **Raw data:** `Benchmarks\Raw\`, untouched; summaries in `Benchmarks\Summaries\`; naming rules in [Benchmarks/README.md](../Benchmarks/README.md).

## 8. Limits we accept up front

- **Single-player only.** There is no Iris path for Mass entity replication in 5.8.4 (`01b #12`).
- **Actor-side systems are modelled, not real GAS or perception.** The demo's four channels stand in for them; real systems would need a small adapter each (`02`). This keeps the measurement about the hand-off, not about GAS.
- **Mass-native state (StateTree, Smart Object claims) is out of scope:** it already survives switches (`01b #2`).
- **Experimental APIs.** Mass representation and MassGameplay are Experimental; the design is pinned to 5.8.4 and may need changes on any upgrade.
- **The checker needs a version field per channel.** That is a test-time requirement on the demo state, documented as such; production code could compile it out.
- **A zero-divergence result is bounded by the sample.** SC1 reports the 95% upper bound, never "proven correct".
