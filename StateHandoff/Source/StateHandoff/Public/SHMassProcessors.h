// P09 StateHandoff (demo) - the three engine processors that make Mass representation LOD run for the demo agents.

#pragma once

#include "MassLODCollectorProcessor.h"
#include "MassRepresentationProcessor.h"
#include "MassVisualizationLODProcessor.h"
#include "SHMassProcessors.generated.h"

/*
 * The engine's LOD collector, visualization-LOD and visualization processors are not auto-registered
 * (MassLODCollectorProcessor.cpp:14, MassVisualizationLODProcessor.cpp:13, MassRepresentationProcessor.cpp:54).
 * A game subclasses each one, filters it to its own tag and turns registration on, exactly as MassCrowd does
 * (MassCrowdVisualizationProcessor.cpp:20-35, MassCrowdVisualizationLODProcessor.cpp:35-55 and 112-125).
 * The subclasses add no logic of their own: the representation switching under test is the engine's.
 */

/** Collects each demo entity's distance and visibility to the viewers (the input of the LOD calculation). */
UCLASS()
class USHLODCollectorProcessor : public UMassLODCollectorProcessor
{
	GENERATED_BODY()

public:
	USHLODCollectorProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
};

/** Turns distance and visibility into a visualization LOD (High, Medium, Low, Off) per demo entity. */
UCLASS()
class USHVisualizationLODProcessor : public UMassVisualizationLODProcessor
{
	GENERATED_BODY()

public:
	USHVisualizationLODProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
};

/** Switches each demo entity's representation (actor, instanced mesh, none) from its LOD: the switch under test. */
UCLASS()
class USHVisualizationProcessor : public UMassVisualizationProcessor
{
	GENERATED_BODY()

public:
	USHVisualizationProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
};
