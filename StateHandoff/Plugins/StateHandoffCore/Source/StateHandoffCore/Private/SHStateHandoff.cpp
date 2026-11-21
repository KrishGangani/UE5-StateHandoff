// StateHandoffCore - USHStateHandoffTrait, USHStateHandoffSubsystem, USHHandoffProcessor.

#include "SHStateHandoff.h"

#include "Components/ActorComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "MassActorSubsystem.h"
#include "MassEntityManager.h"
#include "MassEntityTemplateRegistry.h"
#include "MassExecutionContext.h"
#include "MassRepresentationFragments.h"
#include "MassRepresentationTypes.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "SHContractTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SHStateHandoff)

DEFINE_LOG_CATEGORY_STATIC(LogSHHandoff, Log, All);

// Per-frame hand-off counts and time.
CSV_DEFINE_CATEGORY(StateHandoff, true);

//----------------------------------------------------------------------//
// USHStateHandoffTrait
//----------------------------------------------------------------------//
void USHStateHandoffTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FSHHandoffFragment>();
	BuildContext.AddTag<FSHHandoffTag>();
}

//----------------------------------------------------------------------//
// USHStateHandoffSubsystem
//----------------------------------------------------------------------//
bool USHStateHandoffSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool USHStateHandoffSubsystem::RegisterChannels(const TSubclassOf<UActorComponent> ComponentClass, const TArray<FName>& Properties)
{
	Component = nullptr;
	Channels.Reset();
	if (!ComponentClass)
	{
		UE_LOG(LogSHHandoff, Error, TEXT("RegisterChannels: no component class."));
		return false;
	}
	bool bValid = true;
	for (const FName Name : Properties)
	{
		const FStructProperty* Property = CastField<FStructProperty>(ComponentClass->FindPropertyByName(Name));
		if (!Property || !Property->Struct || !Property->Struct->IsChildOf(FSHChannelFragment::StaticStruct()))
		{
			UE_LOG(LogSHHandoff, Error, TEXT("RegisterChannels: %s.%s is not a struct property deriving from FSHChannelFragment."), *ComponentClass->GetName(), *Name.ToString());
			bValid = false;
			continue;
		}
		if (Channels.ContainsByPredicate([Property](const FChannel& Channel) { return Channel.Type == Property->Struct; }))
		{
			UE_LOG(LogSHHandoff, Error, TEXT("RegisterChannels: two channels of type %s; the entity holds one fragment per type."), *Property->Struct->GetName());
			bValid = false;
			continue;
		}
		Channels.Add({ Property, Property->Struct });
	}
	if (!bValid)
	{
		Channels.Reset();
		return false;
	}
	Component = ComponentClass;
	UE_LOG(LogSHHandoff, Display, TEXT("StateHandoff: %d channels on %s."), Channels.Num(), *ComponentClass->GetName());
	return true;
}

bool USHStateHandoffSubsystem::RestoreToActor(FMassEntityManager& EntityManager, const FMassEntityHandle Entity, AActor& Actor) const
{
	UActorComponent* Target = Actor.FindComponentByClass(Component);
	if (!Target || !EntityManager.IsEntityValid(Entity))
	{
		return false;
	}
	for (const FChannel& Channel : Channels)
	{
		const FStructView Fragment = EntityManager.GetFragmentDataStruct(Entity, Channel.Type);
		// The whole struct, version included: a pooled actor's leftovers from another entity never survive a bind (H4).
		Channel.Type->CopyScriptStruct(Channel.Property->ContainerPtrToValuePtr<void>(Target), Fragment.GetMemory());
	}
	return true;
}

bool USHStateHandoffSubsystem::CaptureFromActor(FMassEntityManager& EntityManager, const FMassEntityHandle Entity, const AActor& Actor) const
{
	const UActorComponent* Source = Actor.FindComponentByClass(Component);
	if (!Source || !EntityManager.IsEntityValid(Entity))
	{
		return false;
	}
	for (const FChannel& Channel : Channels)
	{
		const FStructView Fragment = EntityManager.GetFragmentDataStruct(Entity, Channel.Type);
		Channel.Type->CopyScriptStruct(Fragment.GetMemory(), Channel.Property->ContainerPtrToValuePtr<void>(Source));
	}
	return true;
}

void USHStateHandoffSubsystem::PublishFrame()
{
	CSV_CUSTOM_STAT(StateHandoff, Binds, Binds, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(StateHandoff, Unbinds, Unbinds, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(StateHandoff, HandoffMs, static_cast<float>(HandoffSeconds * 1000.0), ECsvCustomStatOp::Set);
	TotalBinds += Binds;
	TotalUnbinds += Unbinds;
	Binds = 0;
	Unbinds = 0;
	HandoffSeconds = 0.0;
}

//----------------------------------------------------------------------//
// USHHandoffProcessor
//----------------------------------------------------------------------//
USHHandoffProcessor::USHHandoffProcessor()
	: EntityQuery(*this)
{
	bRequiresGameThreadExecution = true; // reads and writes actor components
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Representation;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::VisualizationProcessing);
}

void USHHandoffProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FSHHandoffFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FSHHandoffTag>(EMassFragmentPresence::All);
	ProcessorRequirements.AddSubsystemRequirement<USHStateHandoffSubsystem>(EMassFragmentAccess::ReadWrite);
}

void USHHandoffProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	USHStateHandoffSubsystem* Handoff = Context.GetMutableSubsystem<USHStateHandoffSubsystem>();
	if (!Handoff)
	{
		return;
	}
	const double Start = FPlatformTime::Seconds();
	int32 Binds = 0;
	int32 Unbinds = 0;
	EntityQuery.ForEachEntityChunk(Context, [&EntityManager, Handoff, &Binds, &Unbinds](FMassExecutionContext& ChunkContext)
	{
		const TArrayView<FSHHandoffFragment> Bindings = ChunkContext.GetMutableFragmentView<FSHHandoffFragment>();
		const TConstArrayView<FMassActorFragment> Actors = ChunkContext.GetFragmentView<FMassActorFragment>();
		const TConstArrayView<FMassRepresentationFragment> Representations = ChunkContext.GetFragmentView<FMassRepresentationFragment>();
		for (FMassExecutionContext::FEntityIterator It = ChunkContext.CreateEntityIterator(); It; ++It)
		{
			// The owner now: the actor while the committed representation is an actor; else the entity. A kept,
			// disabled actor (the entity shown as a mesh) is not an owner (02 section 7).
			const bool bActorRepresentation = UE::Mass::Representation::IsValidActorRepresentation(Representations[It].CurrentRepresentation);
			AActor* Now = bActorRepresentation ? const_cast<AActor*>(Actors[It].Get()) : nullptr;
			FSHHandoffFragment& Binding = Bindings[It];
			AActor* Before = Binding.BoundActor.Get();
			if (Now == Before)
			{
				continue;
			}
			const FMassEntityHandle Entity = ChunkContext.GetEntity(It);
			if (Before)
			{
				Handoff->CaptureFromActor(EntityManager, Entity, *Before); // Unbind: the entity owns the state again
				++Unbinds;
			}
			Binding.BoundActor = Now;
			if (Now)
			{
				Handoff->RestoreToActor(EntityManager, Entity, *Now); // Bind: the actor owns it while bound
				++Binds;
			}
		}
	});
	Handoff->AddFrameWork(Binds, Unbinds, FPlatformTime::Seconds() - Start);
	Handoff->PublishFrame();
}
