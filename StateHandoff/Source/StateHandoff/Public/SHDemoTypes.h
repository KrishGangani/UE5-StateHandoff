// P09 StateHandoff (demo) - the small Mass types every demo entity carries.

#pragma once

#include "Mass/EntityElementTypes.h"
#include "SHDemoTypes.generated.h"

/**
 * Which agent an entity is: its position in spawn order (0 .. N-1). Plain index, so the oracle, the checker and the run
 * report can keep their per-agent data in arrays instead of maps keyed by entity handle.
 */
USTRUCT()
struct FSHAgentIdFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	int32 AgentIndex = INDEX_NONE;
};

/**
 * Marks the demo's entities. Every Mass processor in this project requires it, so the project never touches entities
 * that other systems spawn (MassCrowd's FMassCrowdTag plays the same role).
 */
USTRUCT()
struct FSHDemoAgentTag : public FMassTag
{
	GENERATED_BODY()
};
