// P09 StateHandoff (demo) - ASHDemoAgent.

#include "SHDemoAgent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "SHAgentStateComponent.h"
#include "SHLifecycleComponent.h"
#include "SHWorkloadSubsystem.h"
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
	Lifecycle = CreateDefaultSubobject<USHLifecycleComponent>(TEXT("Lifecycle"));

	// Looked up once per class, not once per spawned actor. Cooked through DirectoriesToAlwaysCook (DefaultGame.ini).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		Mesh->SetStaticMesh(Cylinder.Object);
	}
}

bool ASHDemoAgent::CanBePooled_Implementation()
{
	// The engine's default answer is "no" (the interface's generated default), which is pooling off.
	const UWorld* World = GetWorld();
	const USHWorkloadSubsystem* Workload = World ? World->GetSubsystem<USHWorkloadSubsystem>() : nullptr;
	return Workload && Workload->IsPoolingEnabled();
}

void ASHDemoAgent::PrepareForPooling_Implementation()
{
	Lifecycle->OnPrepareForPooling();
}

void ASHDemoAgent::PrepareForGame_Implementation()
{
	Lifecycle->OnPrepareForGame();
}
