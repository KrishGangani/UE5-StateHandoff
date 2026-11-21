// P09 StateHandoff (demo) - USHB2MirrorProcessor and USHB2ActorManagement (baseline B2).

#include "SHB2Mirror.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "MassActorSpawnerSubsystem.h"
#include "MassActorSubsystem.h"
#include "MassCommonTypes.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "SHAgentStateComponent.h"
#include "SHDemoState.h"
#include "SHDemoTypes.h"
#include "SHMassProcessors.h"
#include "SHWorkloadSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SHB2Mirror)

//----------------------------------------------------------------------//
// USHB2MirrorProcessor
//----------------------------------------------------------------------//
USHB2MirrorProcessor::USHB2MirrorProcessor()
	: EntityQuery(*this)
{
	bRequiresGameThreadExecution = true; // reads actor components
	ExecutionFlags = SHMassProcessors::ExecutionFlags;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::SyncWorldToMass;
	ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::Behavior);
}

void USHB2MirrorProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FSHHealthState>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FSHCooldownState>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FSHMemoryState>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FSHReservationState>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FSHDemoAgentTag>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FSHMirrorTag>(EMassFragmentPresence::All);
}

void USHB2MirrorProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& ChunkContext)
	{
		const TArrayView<FSHHealthState> Health = ChunkContext.GetMutableFragmentView<FSHHealthState>();
		const TArrayView<FSHCooldownState> Cooldown = ChunkContext.GetMutableFragmentView<FSHCooldownState>();
		const TArrayView<FSHMemoryState> Memory = ChunkContext.GetMutableFragmentView<FSHMemoryState>();
		const TArrayView<FSHReservationState> Reservation = ChunkContext.GetMutableFragmentView<FSHReservationState>();
		const TConstArrayView<FMassActorFragment> Actors = ChunkContext.GetFragmentView<FMassActorFragment>();
		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			// Read-only access is enough: the actor's component is only read here.
			const AActor* Actor = Actors[It].Get();
			const USHAgentStateComponent* State = Actor ? Actor->FindComponentByClass<USHAgentStateComponent>() : nullptr;
			if (State)
			{
				// The whole struct, version included: the entity becomes an exact copy of the actor's state.
				Health[It] = State->Health;
				Cooldown[It] = State->Cooldown;
				Memory[It] = State->Memory;
				Reservation[It] = State->Reservation;
			}
		}
	});
}

//----------------------------------------------------------------------//
// USHB2ActorManagement
//----------------------------------------------------------------------//
EMassActorSpawnRequestAction USHB2ActorManagement::OnPostActorSpawn(const FMassActorSpawnRequestHandle& SpawnRequestHandle,
	FConstStructView SpawnRequest, TSharedRef<FMassEntityManager> EntityManager) const
{
	const EMassActorSpawnRequestAction Action = Super::OnPostActorSpawn(SpawnRequestHandle, SpawnRequest, EntityManager);

	const FMassActorSpawnRequest& Request = SpawnRequest.Get<const FMassActorSpawnRequest>();
	USHAgentStateComponent* State = Request.SpawnedActor ? Request.SpawnedActor->FindComponentByClass<USHAgentStateComponent>() : nullptr;
	if (State && EntityManager->IsEntityValid(Request.MassAgent))
	{
		// Push: a fresh actor's defaults, or a pooled actor's leftovers from another agent, are overwritten.
		for (int32 Index = 0; Index < static_cast<int32>(ESHChannel::Count); ++Index)
		{
			const ESHChannel Channel = static_cast<ESHChannel>(Index);
			const UScriptStruct& Type = SHDemo::GetChannelStruct(Channel);
			const FStructView Fragment = EntityManager->GetFragmentDataStruct(Request.MassAgent, &Type);
			Type.CopyScriptStruct(&State->Get(Channel), Fragment.GetMemory());
		}
		UWorld* World = EntityManager->GetWorld();
		if (USHWorkloadSubsystem* Workload = World ? World->GetSubsystem<USHWorkloadSubsystem>() : nullptr)
		{
			Workload->NoteBaselineCopy();
		}
	}
	return Action;
}
