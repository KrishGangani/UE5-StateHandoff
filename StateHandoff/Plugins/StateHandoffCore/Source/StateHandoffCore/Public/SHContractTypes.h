// StateHandoffCore - the vocabulary of the state-ownership contract.

#pragma once

#include "Mass/EntityElementTypes.h"
#include "SHContractTypes.generated.h"

/**
 * The base of every state channel: a piece of non-spatial gameplay state that must survive representation switches.
 * The same struct type is both the entity fragment (the entity's copy) and the actor component's property (the actor's
 * copy), so one type describes both homes of the state (02 section 4.1).
 *
 * Version is bumped by every write. It is what lets the checker tell a lost update (version behind), a duplicated one
 * (version ahead) and a corrupted copy (same version, different value) apart (FSHInvariantChecker).
 */
USTRUCT()
struct FSHChannelFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 Version = 0;
};

/** How an observed copy of a channel differs from the expected one. Count is the number of values, not a class. */
UENUM()
enum class ESHDivergence : uint8
{
	None,        // identical
	Lost,        // observed version behind: one or more writes are missing (includes a reset to defaults)
	Duplicated,  // observed version ahead: a write was applied more than once
	Corrupted,   // same version, different value: the copy holds someone else's state or a stale value
	Count UMETA(Hidden)
};
