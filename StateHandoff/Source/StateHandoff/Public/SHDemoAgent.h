// P09 StateHandoff (demo) - the actor Mass spawns for a demo agent when it is close to the camera.

#pragma once

#include "GameFramework/Actor.h"
#include "MassActorPoolableInterface.h"
#include "SHDemoAgent.generated.h"

class UStaticMeshComponent;
class USHAgentStateComponent;
class USHLifecycleComponent;

/**
 * The actor representation of one demo agent: Mass spawns it (high and low resolution use this same class) and
 * releases it again as the agent's LOD changes. A mesh, the actor-side state component (S2b) and baseline B1's
 * lifecycle component (S3a). Poolable: Mass pools only actors that implement IMassActorPoolableInterface and answer
 * CanBePooled (MassActorSpawnerSubsystem.cpp:126-127); this one answers what the run's -SHPooling says.
 */
UCLASS()
class ASHDemoAgent : public AActor, public IMassActorPoolableInterface
{
	GENERATED_BODY()

public:
	ASHDemoAgent();

	//~ Begin IMassActorPoolableInterface (BlueprintNativeEvents: the engine calls Execute_*, which calls these)
	virtual bool CanBePooled_Implementation() override;
	virtual void PrepareForPooling_Implementation() override;
	virtual void PrepareForGame_Implementation() override;
	//~ End IMassActorPoolableInterface

private:
	/** A cylinder, so actors are easy to tell from the instanced cubes in screenshots. No collision: nothing collides. */
	UPROPERTY(VisibleAnywhere, Category = "StateHandoff")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** The actor's copy of the agent's state (the demo's actor-side system). */
	UPROPERTY(VisibleAnywhere, Category = "StateHandoff")
	TObjectPtr<USHAgentStateComponent> State;

	/** Baseline B1's hand-off; on every actor, active only in B1 runs. */
	UPROPERTY(VisibleAnywhere, Category = "StateHandoff")
	TObjectPtr<USHLifecycleComponent> Lifecycle;
};
