# 07 · Common Mistakes: StateHandoff

**Rule:** for every module, the mistakes that are easy to make here, **how to spot them, how to prevent them and how to debug them**. Mistakes the code reviews **actually catch in this project** are marked ⚠ caught. The module sections are filled in during Phase 5, step by step; the Mass-specific hazards found in Phase 2 are listed now, because the design is built around them.

---

## Foundation (Phase 4)

| Mistake | How to spot it | How to prevent it | How to debug it |
|---|---|---|---|
| Using a Mass type without enabling the **Experimental, off-by-default** MassGameplay plugin | `cannot open include file 'MassRepresentationFragments.h'`, or the module doesn't load | Enable it in the `.uproject` (and as a plugin dependency of `StateHandoffCore`) | Check `MassGameplay.uplugin:13` (`EnabledByDefault: false`) |
| Confusing a plugin dependency with a module dependency | The plugin is enabled but the include still fails | Plugin in the descriptor *and* module in `Build.cs` (`06` §2, §4) | UBT's error names the missing include path |
| Looking for `MassEntity.uplugin` | It doesn't exist in 5.8 | Depend on the engine modules `MassEntity`/`MassCore` instead | `01b` §4 |
| Leaving the default-enabled Android File Server plugin on | A `[/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings]` block with a random `SecurityToken` appears in `DefaultEngine.ini` after the editor runs | `"AndroidFileServer", "Enabled": false` in the `.uproject` (`05` A1) | `git diff Config/DefaultEngine.ini` before every commit; grep for `SecurityToken` |
| Passing command-line switches the engine doesn't parse (e.g. `-ExitAfterSeconds` in 5.8.4) | Nothing happens, silently | Grep the engine source for the switch before relying on it | Search `Source` and `Plugins` `.cpp` files for the switch name |
| No `DefaultInput.ini` | A start-up warning "does not have a valid PlayerInput object" from Enhanced Input | The two keys from Epic's template (`05` A10) | Grep the smoke log for `LogEnhancedInput` |
| Starting the packaged **bootstrap** exe (or `cmd /c start /wait`) in a timed run | Exit code 0 even when the game failed | Start `Build\Windows\StateHandoff\Binaries\Win64\StateHandoff.exe` directly (`05` A11–A12) | Compare the exit code with the log's last lines |

## Game module: workload (S1)

| Mistake | How to spot it | How to prevent it | How to debug it |
|---|---|---|---|
| ⚠ caught: including a Mass header that 5.8 moved (`MassEntityElementTypes.h` → MassCore's `Mass/EntityElementTypes.h`) | Build warning C4996 "has moved to the MassCore module" (`MassEntityElementTypes.h:5–6`) | Include the MassCore header and depend on MassCore; read every warning, don't silence it | The warning names the new header |
| Not registering the visualization, visualization-LOD or LOD-collector processor | Entities exist but never get actors or instanced meshes; or the LOD collector trait logs "Using … trait while the required processor … is not enabled by default" (`MassLODTrait.cpp:64`) | Subclass all three with `bAutoRegisterWithProcessingPhases = true` (`05` B4) | The director's representation counts stay at `none` for every entity |
| A `FilterTag` on the LOD processor that differs from the trait's `LODParams.FilterTag` | The LOD is never computed: every entity stays at its initial LOD | Set both to the demo tag, together (`05` B4, B5) | Compare the two tags; `MassVisualizationLODProcessor.cpp:72, 98` is the comparison |
| A required fragment no trait adds (transform, actor fragment) | Template validation errors at spawn | The demo trait adds both (`05` B2) | The error names the missing type |
| Calling the editor-only `UMassEntityConfigAsset::AddTrait(TSubclassOf)` | Compiles in the editor target, fails in the game target | Use `FMassEntityConfig::AddTrait(Trait&)` on `NewObject`-made traits | Build the game target, not only the editor |
| Letting the code-built config be garbage-collected | Crash or invalid template after the next GC | Keep it in a `UPROPERTY` (`05` B5) | `obj refs` on the config; force a GC with `obj gc` |
| Releasing the spawn's creation context before filling the fragments | Creation notifications see default transforms | Hold the returned context until the fragments are written | Log the first frame's representation and positions |
| Spawning in `OnWorldBeginPlay` and depending on the Mass subsystems' own `OnWorldBeginPlay` having run | Works or fails depending on subsystem order | Spawn on the director's first tick | — |
| Assuming the default `LODMaxCount` | Only about 150 actors ever exist | Set the counts explicitly; calibrate (S3) | The representation counts in the log |
| A mesh referenced only from C++ | Invisible actors or cubes in the packaged build, a load warning in its log | `DirectoriesToAlwaysCook` (`05` B7) | Grep the packaged log for the asset path |

## Mass representation hazards (from Phase 2, to be turned into tests)

| Mistake | How to spot it | How to prevent it | How to debug it |
|---|---|---|---|
| Handing state off in `BeginPlay` | The actor→entity lookup fails on the spawn frame | Hand off at the commit (`02` §2); never in `BeginPlay` | The mapping is set after spawn (`MassActorSpawnerSubsystem.cpp:334` vs `MassRepresentationActorManagement.cpp:108`) |
| Saving state only in `EndPlay` | Lost or stale state with pooling, or after a quick LOD return | Capture at the commit; keep `OnEndPlay` only as a safety net | Toggle `mass.spawning.actorpooling`; watch the checker's *lost* count |
| Treating a "disabled" Mass actor as gone | Its components keep ticking and change their copy | The entity owns state while the actor is disabled; restore on re-enable | `Actor.cpp:1754–1760`; break in a component tick while the entity is on ISM |
| Trusting `OnPostActorSpawn` as "the actor is back" | A kept actor re-enabled without a spawn keeps stale state | Detect binding from `CurrentRepresentation` + the actor fragment | `MassRepresentationProcessor.cpp:256–264` |
| Holding a fragment reference across a release | Crash or garbage after the entity moves archetype | Store handles; re-read fragments | Engine warning `MassRepresentationActorManagement.h:115–116` |
| Expecting identical frames under `-deterministic` | Transition frames differ run to run | Judge per transition; normalise per 1,000 transitions | Spawn/destroy budgets are wall-clock (`01b #8`) |
| Using the soft-deprecated `UMassVisualizationTrait`, or forgetting to register the visualization processors | No representation switching at all | `UMassMovableVisualizationTrait` + MassCrowd-style processor subclasses | `01b #13` |
