// P09 StateHandoff (demo) - the two processors that run the workload's event points and the checker's sweep.

#pragma once

#include "MassEntityQuery.h"
#include "MassProcessor.h"
#include "SHWorkloadProcessors.generated.h"

/**
 * The simulation event point: entity-side gameplay, applied before the representation is decided this frame
 * (PrePhysics phase, group Behavior, before Representation). Also starts the run frame.
 */
UCLASS()
class USHSimEventProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	USHSimEventProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};

/**
 * The end of the frame (FrameEnd phase: Mass runs it in TG_LastDemotable, after physics and after tickable objects such
 * as the director): the actor event point (actor-side gameplay such as hits), the transition count, and the checker's
 * sweep. One pass over the agents for all three.
 */
UCLASS()
class USHFrameEndProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	USHFrameEndProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery;
};
