// P09 StateHandoff (demo) - baseline B2: a per-frame mirror of the actor's state into the entity, plus a push on spawn.

#pragma once

#include "MassEntityQuery.h"
#include "MassProcessor.h"
#include "MassRepresentationActorManagement.h"
#include "SHB2Mirror.generated.h"

/**
 * Baseline B2, half 1 (02 section 9): every frame, before any gameplay, copy each agent's actor state into its entity
 * fragments, the way Mass's own translators copy transforms (01b #1). Only entities with FSHMirrorTag match, and the
 * director adds that tag only in B2, so this processor does nothing in the other configurations.
 * SyncWorldToMass group, explicitly before Behavior (so before the simulation event point); game thread (actors).
 */
UCLASS()
class USHB2MirrorProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	USHB2MirrorProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};

/**
 * Baseline B2, half 2: when Mass hands an entity an actor (fresh or from the pool), push the entity's state into the
 * actor's component, after the engine has recorded the actor in the entity's actor fragment (Super). Mass uses the
 * class default object of the actor-management class (MassRepresentationFragments.h:290), so this class keeps no state.
 */
UCLASS()
class USHB2ActorManagement : public UMassRepresentationActorManagement
{
	GENERATED_BODY()

public:
	//~ Begin UMassRepresentationActorManagement (public in the engine class, MassRepresentationActorManagement.h:27, 111)
	virtual EMassActorSpawnRequestAction OnPostActorSpawn(const FMassActorSpawnRequestHandle& SpawnRequestHandle,
		FConstStructView SpawnRequest, TSharedRef<FMassEntityManager> EntityManager) const override;
	//~ End UMassRepresentationActorManagement
};
