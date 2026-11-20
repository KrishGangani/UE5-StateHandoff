// P09 StateHandoff (demo) - the actor Mass spawns for a demo agent when it is close to the camera.

#pragma once

#include "GameFramework/Actor.h"
#include "SHDemoAgent.generated.h"

class UStaticMeshComponent;

/**
 * The actor representation of one demo agent: Mass spawns it (high and low resolution use this same class) and
 * releases it again as the agent's LOD changes. S1: a mesh only. S2 adds the actor-side state component; S3 adds
 * pooling and the lifecycle baseline's component.
 */
UCLASS()
class ASHDemoAgent : public AActor
{
	GENERATED_BODY()

public:
	ASHDemoAgent();

private:
	/** A cylinder, so actors are easy to tell from the instanced cubes in screenshots. No collision: nothing collides. */
	UPROPERTY(VisibleAnywhere, Category = "StateHandoff")
	TObjectPtr<UStaticMeshComponent> Mesh;
};
