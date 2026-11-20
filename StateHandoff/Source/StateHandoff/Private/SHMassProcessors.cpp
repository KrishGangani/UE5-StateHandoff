// P09 StateHandoff (demo) - USHLODCollectorProcessor, USHVisualizationLODProcessor, USHVisualizationProcessor.

#include "SHMassProcessors.h"

#include "SHDemoTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SHMassProcessors)

namespace SHMassProcessors
{
	/** Single-player only (01b #12): run in standalone games; Client keeps the engine's own setting for these processors. */
	constexpr int32 ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
}

//----------------------------------------------------------------------//
// USHLODCollectorProcessor
//----------------------------------------------------------------------//
USHLODCollectorProcessor::USHLODCollectorProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
}

void USHLODCollectorProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::ConfigureQueries(EntityManager);

	EntityQuery_VisibleRangeAndOnLOD.AddTagRequirement<FSHDemoAgentTag>(EMassFragmentPresence::All);
	EntityQuery_VisibleRangeOnly.AddTagRequirement<FSHDemoAgentTag>(EMassFragmentPresence::All);
	EntityQuery_OnLODOnly.AddTagRequirement<FSHDemoAgentTag>(EMassFragmentPresence::All);
	EntityQuery_NotVisibleRangeAndOffLOD.AddTagRequirement<FSHDemoAgentTag>(EMassFragmentPresence::All);
}

//----------------------------------------------------------------------//
// USHVisualizationLODProcessor
//----------------------------------------------------------------------//
USHVisualizationLODProcessor::USHVisualizationLODProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = SHMassProcessors::ExecutionFlags;
	// Group LOD, after LODCollector: set by the engine constructor (MassVisualizationLODProcessor.cpp:14-15).
}

void USHVisualizationLODProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::ConfigureQueries(EntityManager);

	CloseEntityQuery.AddTagRequirement<FSHDemoAgentTag>(EMassFragmentPresence::All);
	CloseEntityAdjustDistanceQuery.AddTagRequirement<FSHDemoAgentTag>(EMassFragmentPresence::All);
	FarEntityQuery.AddTagRequirement<FSHDemoAgentTag>(EMassFragmentPresence::All);
	DebugEntityQuery.AddTagRequirement<FSHDemoAgentTag>(EMassFragmentPresence::All);

	// Only the LOD settings whose FilterTag is the demo tag are prepared and count-adjusted by this processor
	// (MassVisualizationLODProcessor.cpp:72, 98). The director sets the same tag in the trait's LOD parameters.
	FilterTag = FSHDemoAgentTag::StaticStruct();
}

//----------------------------------------------------------------------//
// USHVisualizationProcessor
//----------------------------------------------------------------------//
USHVisualizationProcessor::USHVisualizationProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = SHMassProcessors::ExecutionFlags;
	// It spawns, enables, disables and releases actors: game thread only.
	bRequiresGameThreadExecution = true;
	// Group Representation.VisualizationProcessing, after LOD: set by the engine constructors (MassRepresentationProcessor.cpp:56-57, 523).
}

void USHVisualizationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::ConfigureQueries(EntityManager);

	EntityQuery.AddTagRequirement<FSHDemoAgentTag>(EMassFragmentPresence::All);
}
