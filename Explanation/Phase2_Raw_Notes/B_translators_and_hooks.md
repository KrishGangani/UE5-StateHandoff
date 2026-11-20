# B · Translators, hooks, and where Mass-native state lives (UE 5.8.4)

Short forms as in [README](README.md).

## B1. The translator base

`MG/MassSpawner/Public/MassTranslator.h`

- `EMassTranslationDirection { None, InitializationOnly, ActorToMass, MassToActor, BothWays }` (:12–21).
- `UMassTranslator : UMassProcessor`, abstract. Its only addition to a processor is `RequiredTags`, the tags an entity needs so the translator processes it (:28–60). `AppendRequiredTags` is deprecated in 5.8 (:42).
- A translator is therefore an ordinary per-frame processor. It copies values **every frame** for every matching entity. It is not an event at representation change.

## B2. Every translator shipped in the 5.8.4 engine (search: `public UMassTranslator` in `Engine/Plugins`)

| Class | Header:line | Direction | Fragments read / written | Evidence (.cpp) |
|---|---|---|---|---|
| `UMassCapsuleTransformToMassTranslator` | `MG/MassActors/Public/Translators/MassCapsuleComponentTranslators.h:38` | Actor→Mass | capsule wrapper → `FTransformFragment` (whole transform) | `MassCapsuleComponentTranslators.cpp:29–30, 43` |
| `UMassTransformToActorCapsuleTranslator` | same file :58 | Mass→Actor | `FTransformFragment` → capsule `SetWorldTransform` | `.cpp:65–66, 80` |
| `UMassCharacterMovementToMassTranslator` | `MassCharacterMovementTranslators.h:29` | Actor→Mass | CMC → `FTransformFragment` location (`GetActorNavLocation`) + `FMassVelocityFragment` | `MassCharacterMovementTranslators.cpp:30–32, 47–49` |
| `UMassCharacterMovementToActorTranslator` | same file :51 | Mass→Actor | `FMassVelocityFragment` → `RequestDirectMove` | `.cpp:71–72, 86` |
| `UMassCharacterOrientationToMassTranslator` | same file :73 | Actor→Mass | CMC updated component rotation → `FTransformFragment` rotation | `.cpp:107–108, 124` |
| `UMassCharacterOrientationToActorTranslator` | same file :95 | Mass→Actor | `FTransformFragment` rotation → CMC | `.cpp:147–148` |
| `UMassSceneComponentLocationToMassTranslator` | `MassSceneComponentLocationTranslator.h:27` | Actor→Mass | scene component location → `FTransformFragment` | `MassSceneComponentLocationTranslator.cpp:25–26, 40` |
| `UMassSceneComponentLocationToActorTranslator` | same file :49 | Mass→Actor | `FTransformFragment` → `SetWorldLocation` | `.cpp:62–63, 78` |
| `UMassSceneComponentVelocityToMassTranslator` | `MassSceneComponentVelocityTranslator.h:18` | Actor→Mass | component velocity → `FMassVelocityFragment` | `MassSceneComponentVelocityTranslator.cpp:26–27, 41` |
| `UMassTranslator_BehaviorTree` | `MassTranslators_BehaviorTree.h:23` | — | Declares `FDataFragment_BehaviorTreeComponentWrapper` (a weak pointer to the BT component, :15–20). **`Execute` is empty** (:32) and it is **not auto-registered** (`.cpp:12`). It syncs nothing | `MassTranslators_BehaviorTree.cpp:9–18` |
| `UMassNavMoverToMassTranslator`, `UMassToNavMoverTranslator`, `UMassNavMoverActorOrientationToMassTranslator`, `UMassOrientationToNavMoverActorOrientationTranslator` | `Plugins/Experimental/MoverIntegrations/Source/MoverMassIntegration/Public/MoverMassTranslators.h:33, 59, 84, 110` | both | transform, velocity, desired movement, orientation | `MoverMassTranslators.cpp:33–36, 97–105, 170–171, 210–211` |
| `UMassToMoverInputTranslator`, `UMoverInputToMassTranslator` | `MassMoverInputTranslator.h:35, 52` | both | Mover input wrapper ↔ velocity, transform | `MassMoverInputTranslator.cpp:19–21, 56–58` |

The MoverIntegrations plugin is flagged Experimental (`MoverIntegrations.uplugin:14`). No translator was found in MassAI, MassCrowd, InstancedActors, UAFMass or ZoneGraph (search for `UMassTranslator` in their sources).

**Answer to the README's claim.** "Shipped translators sync only transform, movement, orientation and collision": **confirmed, with two precisions.** (1) There is also a `UMassTranslator_BehaviorTree`, but it is an empty, unregistered stub that copies nothing. (2) "Collision" means the capsule *transform*, not collision settings. Mover's translators (Experimental) also move only movement data.

## B3. Sync traits

`MG/MassActors/Public/MassAgentTraits.h`: `UMassAgentSyncTrait` holds `SyncDirection`, default `BothWays` (:15–25); subclasses `UMassAgentCapsuleCollisionSyncTrait` (:31), `UMassAgentMovementSyncTrait` (:43), `UMassAgentOrientationSyncTrait` (:52), `UMassAgentFeetLocationSyncTrait` (:61). They add the translator tags and register **object fragment initializers** (`MassAgentTraits.cpp:53, 93, 156`). Two non-translator traits also register initializers: `MAI/MassAIBehavior/Private/MassLookAtTargetTrait.cpp:25` and `Plugins/Experimental/UAF/UAFMass/Source/UAFMass/Private/Traits/CharacterTrajectoryUAFTrait.cpp:25`.

## B4. Engine hooks that could carry state across a representation switch

| Hook | When it runs | Direction | Limits | Evidence |
|---|---|---|---|---|
| `UMassRepresentationActorManagement::OnPostActorSpawn` (virtual) | After a spawn **or pool retrieval**, inside `ProcessPendingSpawningRequest` | entity → actor | Not called when an existing (kept, disabled) actor is simply re-enabled | `MassRepresentationActorManagement.h:111`; `MassActorSpawnerSubsystem.cpp:480–482`; re-enable path `MassRepresentationProcessor.cpp:256–264` |
| `OnPreActorSpawn` (virtual) | Before the spawn; releases the old actor immediately | could capture actor → entity before that release | Only covers the template-swap release | `MassRepresentationActorManagement.cpp:63–90` |
| Object fragment initializers (traits) | On agent creation (`ActorToMass`) and puppet initialisation (`MassToActor`) | one-shot | Only for actors with a `UMassAgentComponent`. **Puppet unregistration removes the puppet's fragments without any copy back** | `MG/MassActors/Private/MassAgentSubsystem.cpp:346–353, 394–399`; removal :196–219 |
| `UMassAgentSubsystem::GetOnMassAgentComponentEntityDetaching()` | When an agent component clears its entity handle, e.g. inside `DestroyActor` (which unregisters the component immediately) | actor → entity | Only for actors with a `UMassAgentComponent`; fires at release time, not at commit time | `MassAgentComponent.cpp:329–336`; `MassAgentSubsystem.cpp:439–441`; `MassActorSpawnerSubsystem.cpp:94–99` |
| `IMassActorPoolableInterface::PrepareForPooling` / `PrepareForGame` | On release to pool / retrieval from pool | either | Only when pooling is on and the actor opts in | `MassActorPoolableInterface.h:22–29`; `MassActorSpawnerSubsystem.cpp:132, 255` |
| `AActor::EndPlay` / `BeginPlay`, `AActor::OnEndPlay` delegate | Actor destruction / spawn | either | Skipped for pooled actors; `EndPlay` may run frames after the representation changed (destruction budget) | `Source/Runtime/Engine/Classes/GameFramework/Actor.h:2339`; A4 in [A](A_representation_internals.md) |
| A processor with `ExecuteAfter Representation.VisualizationProcessing` | Every frame, after the representation is final for the frame | both | Must detect changes itself; the actor is still alive (destruction is queued to the end of the phase) except after an immediate release | `MassRepresentationTypes.h:28–30`; `MassActorSpawnerSubsystem.cpp:585–595` |
| Instanced Actors: `UInstancedActorsComponent::OnServerPreSpawnInitForInstance`, `InitializeComponentForInstance`, `AddOrUpdateFragmentsDeferred`, `AddOrUpdatePersistentFragmentsDeferred`, `SerializeInstancePersistenceData` | Spawn of an instanced actor; on demand; save/load | both | Per-component, hand-written. Header note: the persistent fragments "are not automatically persisted (yet)" | `IA/Public/InstancedActorsComponent.h:41, 48, 82–102, 138–149 (note :145–146)` |

**Conclusion:** the engine offers *hooks*, scattered across five mechanisms with different timing. None states who owns a piece of state, and nothing checks that state survived.

## B5. Mass-native state already survives a switch (it never lived on the actor)

| State | Where it lives | Freed when | Evidence |
|---|---|---|---|
| Mass StateTree instance data (AI state, task memory) | `UMassStateTreeSubsystem::InstanceDataArray`, referenced by `FMassStateTreeInstanceFragment::InstanceHandle` | The fragment is removed (entity destroyed), by an observer | `MAI/MassAIBehavior/Public/MassStateTreeFragments.h:11–22`; `MassStateTreeSubsystem.h:33, 91`; `Private/MassStateTreeProcessors.cpp:103–109` |
| Data a Mass StateTree can bind to | Fragments, shared fragments, const shared fragments, world subsystems; **not** actors or components | — | `MAI/MassAIBehavior/Private/MassStateTreeExecutionContext.cpp:39, 58, 77, 96` |
| Mass Smart Object claim and interaction cooldown | `FMassSmartObjectUserFragment::InteractionHandle`, `InteractionCooldownEndTime` | Released by `UMassSmartObjectUserFragmentDeinitializer` on fragment removal | `MG/MassSmartObjects/Public/MassSmartObjectFragments.h:15–37`; `Private/MassSmartObjectProcessor.cpp:428–429`; `MassSmartObjectHandler.cpp:306–329` |

So **state owned by Mass-native systems is safe by construction**. The problem is state owned by **actor-side** systems: components, GAS (`UAbilitySystemComponent`), AI controllers and blackboards, perception, timers, actor-made Smart Object claims. Epic's own statement about GAS is in [D](D_existing_work.md) (D2).
