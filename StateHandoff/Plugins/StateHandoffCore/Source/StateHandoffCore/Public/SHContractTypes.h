// StateHandoffCore - the vocabulary of the state-ownership contract.

#pragma once

#include "Mass/EntityElementTypes.h"
#include "SHContractTypes.generated.h"

/**
 * The base of every state channel: a piece of non-spatial gameplay state that must survive representation switches.
 * The same struct type is both the entity fragment (the entity's copy) and the actor component's property (the actor's
 * copy), so one type describes both homes of the state (02 section 4.1).
 *
 * Version is the sequence number of the last write this copy received. Every copy of one agent's channel must share
 * one numbering (the writer stamps each write with the next number), so versions of two copies are comparable: behind =
 * the copy missed the latest write(s), ahead = it holds a write the reference doesn't, equal with different values =
 * the same last write on a different base (FSHInvariantChecker).
 */
USTRUCT()
struct FSHChannelFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 Version = 0;
};

/**
 * How an observed copy of a channel differs from the expected one. Count is the number of values, not a class.
 *  Lost:       observed version behind: the latest write(s) are missing (includes a reset to defaults).
 *  Duplicated: observed version ahead: the copy holds a write the reference doesn't (one never decided, or a history
 *              the reference dropped at a re-sync coming back).
 *  Corrupted:  same version, different value: the same last write on a different base (an earlier write missed or
 *              applied twice), or another agent's state.
 */
UENUM()
enum class ESHDivergence : uint8
{
	None,        // identical
	Lost,        // version behind
	Duplicated,  // version ahead
	Corrupted,   // same version, different value
	Count UMETA(Hidden)
};
