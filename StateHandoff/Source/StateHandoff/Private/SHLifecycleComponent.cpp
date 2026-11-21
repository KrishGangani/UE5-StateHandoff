// P09 StateHandoff (demo) - USHLifecycleComponent (baseline B1).

#include "SHLifecycleComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "MassActorSubsystem.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "SHAgentStateComponent.h"
#include "SHDemoState.h"
#include "SHWorkloadSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SHLifecycleComponent)

USHLifecycleComponent::USHLifecycleComponent()
{
	// Ticks only while it waits for the actor -> entity mapping, then turns its tick off again.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

bool USHLifecycleComponent::IsActiveBaseline() const
{
	const UWorld* World = GetWorld();
	const USHWorkloadSubsystem* Workload = World ? World->GetSubsystem<USHWorkloadSubsystem>() : nullptr;
	return Workload && Workload->IsRunning() && Workload->GetConfig() == ESHConfig::B1;
}

void USHLifecycleComponent::BeginPlay()
{
	Super::BeginPlay();
	BeginHydrate();
}

void USHLifecycleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// By design, this is the weakness B1 exists to measure (01 H1): Mass queues and budgets the destruction of a
	// released actor, so EndPlay can run frames after the entity lost its actor, and the copy below can then overwrite
	// writes the entity received in between. A lifecycle hand-off has no earlier moment to hook into.
	// Mass destroys a released actor (Destroyed). At world teardown or quit there is nothing to hand back: Mass's
	// entities are going away too, so the copy is skipped rather than touching a manager that is shutting down.
	if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		Dehydrate();
	}
	else
	{
		bHydrated = false;
		SetComponentTickEnabled(false);
	}
	Super::EndPlay(EndPlayReason);
}

void USHLifecycleComponent::OnPrepareForPooling()
{
	// A pooled actor skips EndPlay (01b #4): this is the pool-aware half of the baseline.
	Dehydrate();
}

void USHLifecycleComponent::OnPrepareForGame()
{
	// A pooled actor skips BeginPlay too. The mapping to its new entity is set after this call, so this usually retries.
	BeginHydrate();
}

void USHLifecycleComponent::BeginHydrate()
{
	if (IsActiveBaseline() && !TryHydrate())
	{
		SetComponentTickEnabled(true);
	}
}

void USHLifecycleComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!IsActiveBaseline() || TryHydrate())
	{
		SetComponentTickEnabled(false);
	}
}

bool USHLifecycleComponent::TryHydrate()
{
	UWorld* World = GetWorld();
	UMassActorSubsystem* ActorSubsystem = World ? World->GetSubsystem<UMassActorSubsystem>() : nullptr;
	UMassEntitySubsystem* EntitySubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	USHAgentStateComponent* State = GetOwner()->FindComponentByClass<USHAgentStateComponent>();
	if (!ActorSubsystem || !EntitySubsystem || !State)
	{
		return false;
	}
	const FMassEntityHandle Found = ActorSubsystem->GetEntityHandleFromActor(GetOwner());
	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	if (!EntityManager.IsEntityValid(Found))
	{
		return false; // not mapped yet: Mass maps the actor after it has spawned
	}
	for (int32 Index = 0; Index < static_cast<int32>(ESHChannel::Count); ++Index)
	{
		const ESHChannel Channel = static_cast<ESHChannel>(Index);
		const UScriptStruct& Type = SHDemo::GetChannelStruct(Channel);
		const FStructView Fragment = EntityManager.GetFragmentDataStruct(Found, &Type);
		Type.CopyScriptStruct(&State->Get(Channel), Fragment.GetMemory());
	}
	Entity = Found;
	bHydrated = true;
	if (USHWorkloadSubsystem* Workload = World->GetSubsystem<USHWorkloadSubsystem>())
	{
		Workload->NoteBaselineCopy();
	}
	return true;
}

void USHLifecycleComponent::Dehydrate()
{
	SetComponentTickEnabled(false);
	if (!bHydrated)
	{
		return;
	}
	bHydrated = false;
	UWorld* World = GetWorld();
	UMassEntitySubsystem* EntitySubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	USHAgentStateComponent* State = GetOwner()->FindComponentByClass<USHAgentStateComponent>();
	if (!EntitySubsystem || !State)
	{
		return;
	}
	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	if (EntityManager.IsEntityValid(Entity))
	{
		for (int32 Index = 0; Index < static_cast<int32>(ESHChannel::Count); ++Index)
		{
			const ESHChannel Channel = static_cast<ESHChannel>(Index);
			const UScriptStruct& Type = SHDemo::GetChannelStruct(Channel);
			const FStructView Fragment = EntityManager.GetFragmentDataStruct(Entity, &Type);
			Type.CopyScriptStruct(Fragment.GetMemory(), &State->Get(Channel));
		}
		if (USHWorkloadSubsystem* Workload = World->GetSubsystem<USHWorkloadSubsystem>())
		{
			Workload->NoteBaselineCopy();
		}
	}
	Entity.Reset();
}
