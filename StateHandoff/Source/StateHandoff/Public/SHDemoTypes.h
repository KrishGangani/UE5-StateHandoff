// P09 StateHandoff (demo) - the small Mass types every demo entity carries, and the configuration and transition ids.

#pragma once

#include "Mass/EntityElementTypes.h"
#include "MassRepresentationTypes.h"
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

/** The kinds of representation change the checker counts (01 section 6.3). */
enum class ESHTransitionKind : uint8
{
	ActorToMesh,
	MeshToActor,
	MeshToNone,
	NoneToMesh,
	ActorToNone,
	NoneToActor,
	ActorSwap,   // high-res <-> low-res actor
	Count        // also "no transition yet"
};

/**
 * The checker's own record of each entity's representation, updated every frame in every configuration, so the
 * transition count never depends on the system under test (02 section 9).
 */
USTRUCT()
struct FSHObservedFragment : public FMassFragment
{
	GENERATED_BODY()

	/** The representation seen on the previous frame. Entities start as None (MassRepresentationFragments.h:59). */
	EMassRepresentationType LastRepresentation = EMassRepresentationType::None;

	/** The kind of the latest transition, and the run frame it happened on: an incident is reported against them. */
	ESHTransitionKind LastKind = ESHTransitionKind::Count;
	uint32 LastTransitionFrame = 0;
};

/** The five compared configurations (01 section 6.2), chosen with -SHConfig=. */
enum class ESHConfig : uint8
{
	B0,   // stock: state on the actor component, no hand-off
	B1,   // lifecycle hand-off, pool-aware (S3)
	B2,   // per-frame mirror + push on spawn (S3)
	B3,   // entity-owned (S3)
	SH,   // StateHandoff (S5)
	Count
};

namespace SHDemo
{
	/** Parses "B0" .. "SH" (case-insensitive); false if the text is none of them. */
	bool ParseConfig(const FString& Text, ESHConfig& OutConfig);

	const TCHAR* GetConfigName(ESHConfig Config);

	/** The kind of the change From -> To, or Count if it isn't a representation change the checker counts. */
	ESHTransitionKind ClassifyTransition(EMassRepresentationType From, EMassRepresentationType To);

	const TCHAR* GetTransitionName(ESHTransitionKind Kind);
}
