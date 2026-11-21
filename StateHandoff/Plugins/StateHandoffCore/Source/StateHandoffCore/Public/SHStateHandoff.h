// StateHandoffCore - the commit-time hand-off: the contract's fragment and tag, the trait, the channel registry and
// the processor that moves ownership when Mass commits a representation (02 sections 2 and 4.1). Minimal v1 (S4-S5).

#pragma once

#include "Mass/EntityElementTypes.h"
#include "Mass/EntityHandle.h"
#include "MassEntityQuery.h"
#include "MassEntityTraitBase.h"
#include "MassProcessor.h"
#include "Subsystems/WorldSubsystem.h"
#include "SHStateHandoff.generated.h"

#define UE_API STATEHANDOFFCORE_API

class UActorComponent;
struct FMassEntityManager;

/** Which actor currently owns this entity's channels (ActorWhileBound): null while the entity owns them. */
USTRUCT()
struct FSHHandoffFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> BoundActor;
};

/** Marks the entities whose state StateHandoff manages; only they match the hand-off processor. */
USTRUCT()
struct FSHHandoffTag : public FMassTag
{
	GENERATED_BODY()
};

/** Adds the hand-off fragment and tag. The channels themselves are the game's fragments (FSHChannelFragment types). */
UCLASS(MinimalAPI, meta = (DisplayName = "SH State Handoff"))
class USHStateHandoffTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	UE_API virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

/**
 * The channel registry and the two copies. A channel is a UPROPERTY on an actor component whose struct type derives
 * from FSHChannelFragment; the entity holds a fragment of the same type. Registration resolves each property once by
 * reflection and refuses anything that doesn't fit, so the hand-off itself never searches by name.
 */
UCLASS(MinimalAPI)
class USHStateHandoffSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Registers the channels: Properties on ComponentClass. False (and an error per problem) if any doesn't fit. */
	UE_API bool RegisterChannels(TSubclassOf<UActorComponent> ComponentClass, const TArray<FName>& Properties);

	/** Restore: copies every channel from the entity's fragments into Actor's component (version included). */
	UE_API bool RestoreToActor(FMassEntityManager& EntityManager, FMassEntityHandle Entity, AActor& Actor) const;

	/** Capture: copies every channel from Actor's component into the entity's fragments (version included). */
	UE_API bool CaptureFromActor(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const AActor& Actor) const;

	/** This frame's hand-off counts and time, published once per frame by the processor (CSV category StateHandoff). */
	void AddFrameWork(int32 InBinds, int32 InUnbinds, double Seconds) { Binds += InBinds; Unbinds += InUnbinds; HandoffSeconds += Seconds; }
	UE_API void PublishFrame();

	int64 GetTotalBinds() const { return TotalBinds; }
	int64 GetTotalUnbinds() const { return TotalUnbinds; }

protected:
	//~ Begin UWorldSubsystem
	UE_API virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End UWorldSubsystem

private:
	struct FChannel
	{
		const FStructProperty* Property = nullptr;
		const UScriptStruct* Type = nullptr;
	};

	TSubclassOf<UActorComponent> Component;
	TArray<FChannel> Channels;

	int32 Binds = 0;
	int32 Unbinds = 0;
	double HandoffSeconds = 0.0;
	int64 TotalBinds = 0;
	int64 TotalUnbinds = 0;
};

/**
 * The commit-time hand-off (02 section 2): right after Mass has finalised this frame's representation (ExecuteAfter
 * Representation.VisualizationProcessing, MassRepresentationTypes.h:28-30), compare, per entity, the actor that should
 * own the state now (the actor, if the representation is an actor and the actor is valid) with the one bound before.
 * On a change: capture from the old actor (Unbind), restore into the new one (Bind). Queued releases are processed
 * later, at the end of the PrePhysics phase (MassActorSpawnerSubsystem.cpp:585-595), so a released actor is still
 * alive here and its state is captured before it is destroyed or pooled.
 */
UCLASS(MinimalAPI)
class USHHandoffProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UE_API USHHandoffProcessor();

protected:
	UE_API virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	UE_API virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};

#undef UE_API
