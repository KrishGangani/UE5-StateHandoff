// P09 StateHandoff (demo) - USHSimEventProcessor and USHFrameEndProcessor.

#include "SHWorkloadProcessors.h"

#include "Engine/World.h"
#include "MassActorSubsystem.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"
#include "MassRepresentationFragments.h"
#include "MassRepresentationTypes.h"
#include "SHAgentStateComponent.h"
#include "SHDemoTypes.h"
#include "SHLifecycleComponent.h"
#include "SHMassProcessors.h"
#include "SHStateRouter.h"
#include "SHWorkloadSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SHWorkloadProcessors)

namespace SHWorkloadProcessors
{
	/** The requirements both processors share: the agent, its four entity copies, its actor fragment, the tag. */
	void AddAgentRequirements(FMassEntityQuery& Query)
	{
		Query.AddRequirement<FSHAgentIdFragment>(EMassFragmentAccess::ReadOnly);
		Query.AddRequirement<FSHHealthState>(EMassFragmentAccess::ReadWrite);
		Query.AddRequirement<FSHCooldownState>(EMassFragmentAccess::ReadWrite);
		Query.AddRequirement<FSHMemoryState>(EMassFragmentAccess::ReadWrite);
		Query.AddRequirement<FSHReservationState>(EMassFragmentAccess::ReadWrite);
		// ReadWrite only to get a mutable actor pointer: the actor's component is written, the fragment never is.
		Query.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
		Query.AddTagRequirement<FSHDemoAgentTag>(EMassFragmentPresence::All);
	}

	/** One chunk's fragment views, fetched once per chunk; Make(Index) builds one agent's view from them. */
	struct FChunkViews
	{
		TConstArrayView<FSHAgentIdFragment> Ids;
		TArrayView<FSHHealthState> Health;
		TArrayView<FSHCooldownState> Cooldown;
		TArrayView<FSHMemoryState> Memory;
		TArrayView<FSHReservationState> Reservation;
		TArrayView<FMassActorFragment> Actors;

		explicit FChunkViews(FMassExecutionContext& Context)
			: Ids(Context.GetFragmentView<FSHAgentIdFragment>())
			, Health(Context.GetMutableFragmentView<FSHHealthState>())
			, Cooldown(Context.GetMutableFragmentView<FSHCooldownState>())
			, Memory(Context.GetMutableFragmentView<FSHMemoryState>())
			, Reservation(Context.GetMutableFragmentView<FSHReservationState>())
			, Actors(Context.GetMutableFragmentView<FMassActorFragment>())
		{
		}

		FSHAgentView Make(const int32 Index) const
		{
			FSHAgentView View;
			View.Health = &Health[Index];
			View.Cooldown = &Cooldown[Index];
			View.Memory = &Memory[Index];
			View.Reservation = &Reservation[Index];
			if (AActor* Actor = Actors[Index].GetMutable())
			{
				// Both lookups in every configuration, so every configuration pays the same per-agent cost.
				View.ActorState = Actor->FindComponentByClass<USHAgentStateComponent>();
				View.Lifecycle = Actor->FindComponentByClass<USHLifecycleComponent>();
			}
			return View;
		}
	};
}

//----------------------------------------------------------------------//
// USHSimEventProcessor
//----------------------------------------------------------------------//
USHSimEventProcessor::USHSimEventProcessor()
	: EntityQuery(*this)
{
	// It writes actor components (through the router) and uses a game-thread subsystem.
	bRequiresGameThreadExecution = true;
	ExecutionFlags = SHMassProcessors::ExecutionFlags;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Behavior;
	ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::Representation);
}

void USHSimEventProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	SHWorkloadProcessors::AddAgentRequirements(EntityQuery);
	ProcessorRequirements.AddSubsystemRequirement<USHWorkloadSubsystem>(EMassFragmentAccess::ReadWrite);
}

void USHSimEventProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	USHWorkloadSubsystem* Workload = Context.GetMutableSubsystem<USHWorkloadSubsystem>();
	if (!Workload || !Workload->IsRunning())
	{
		return;
	}
	Workload->BeginFrame();
	const double Now = Context.GetWorld()->GetTimeSeconds();

	EntityQuery.ForEachEntityChunk(Context, [Workload, Now](FMassExecutionContext& ChunkContext)
	{
		const SHWorkloadProcessors::FChunkViews Views(ChunkContext);
		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			Workload->ApplyEvent(ESHEventPoint::Simulation, Views.Ids[It].AgentIndex, Views.Make(It), Now);
		}
	});
}

//----------------------------------------------------------------------//
// USHFrameEndProcessor
//----------------------------------------------------------------------//
USHFrameEndProcessor::USHFrameEndProcessor()
	: EntityQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::FrameEnd;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = SHMassProcessors::ExecutionFlags;
}

void USHFrameEndProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	SHWorkloadProcessors::AddAgentRequirements(EntityQuery);
	EntityQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FSHObservedFragment>(EMassFragmentAccess::ReadWrite);
	ProcessorRequirements.AddSubsystemRequirement<USHWorkloadSubsystem>(EMassFragmentAccess::ReadWrite);
}

void USHFrameEndProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	USHWorkloadSubsystem* Workload = Context.GetMutableSubsystem<USHWorkloadSubsystem>();
	if (!Workload || !Workload->IsRunning())
	{
		return;
	}
	const double Now = Context.GetWorld()->GetTimeSeconds();
	const bool bCheck = Workload->IsCheckEnabled();

	EntityQuery.ForEachEntityChunk(Context, [Workload, Now, bCheck](FMassExecutionContext& ChunkContext)
	{
		const SHWorkloadProcessors::FChunkViews Views(ChunkContext);
		const TConstArrayView<FMassRepresentationFragment> Representations = ChunkContext.GetFragmentView<FMassRepresentationFragment>();
		const TArrayView<FSHObservedFragment> Observed = ChunkContext.GetMutableFragmentView<FSHObservedFragment>();
		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			const int32 Agent = Views.Ids[It].AgentIndex;
			const FSHAgentView View = Views.Make(It);
			// Every agent can get an actor-point event (01 section 6): the configuration's router decides which copy it
			// writes, exactly as at the simulation point. Only the timing differs: after physics and the director.
			Workload->ApplyEvent(ESHEventPoint::Actor, Agent, View, Now);
			Workload->ObserveAgent(Representations[It].CurrentRepresentation, View.ActorState != nullptr, Observed[It]);
			if (bCheck)
			{
				Workload->CheckAgent(Agent, View, Observed[It]);
			}
		}
	});
	Workload->EndFrame();
}
