// P09 StateHandoff (demo) - ASHDemoAgent.

#include "SHDemoAgent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "SHAgentStateComponent.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SHDemoAgent)

ASHDemoAgent::ASHDemoAgent()
{
	// The actor has nothing to do per frame; Mass drives everything.
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false);

	State = CreateDefaultSubobject<USHAgentStateComponent>(TEXT("State"));

	// Looked up once per class, not once per spawned actor. Cooked through DirectoriesToAlwaysCook (DefaultGame.ini).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		Mesh->SetStaticMesh(Cylinder.Object);
	}
}
