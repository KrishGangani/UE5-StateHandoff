// StateHandoffCore - the invariant checker's core: classify one expected/observed pair, count the results.

#pragma once

#include "SHContractTypes.h"

#define UE_API STATEHANDOFFCORE_API

class UScriptStruct;

/**
 * Plain C++ (no UObject, no world), so it is unit-tested with hand-made structs (Tests/SHInvariantCheckerTests.cpp).
 * It is the measuring instrument: the workload (game module) decides what is expected and where to look; this class
 * only says how the two differ and keeps the counts. It is built and proven on the baselines before any hand-off code
 * exists (03, step S2).
 */
class FSHInvariantChecker
{
public:
	/**
	 * Compares two copies of the same channel. Both must be instances of Type, and Type must derive from
	 * FSHChannelFragment. Versions decide Lost and Duplicated; equal versions fall back to a full property comparison
	 * (UScriptStruct::CompareScriptStruct), which separates None from Corrupted.
	 */
	static UE_API ESHDivergence Classify(const UScriptStruct& Type, const FSHChannelFragment& Expected, const FSHChannelFragment& Observed);

	/** Counts one comparison result (None included, so the number of comparisons is known too). Count is not a result. */
	void Record(const ESHDivergence Result) { check(Result < ESHDivergence::Count); ++Counts[static_cast<int32>(Result)]; }

	/** How many results of this class were recorded. */
	int64 GetCount(const ESHDivergence Result) const { check(Result < ESHDivergence::Count); return Counts[static_cast<int32>(Result)]; }

	/** Divergences of any class (everything but None). */
	UE_API int64 GetIncidents() const;

	/** Forgets every count. */
	UE_API void Reset();

private:
	int64 Counts[static_cast<int32>(ESHDivergence::Count)] = {};
};

#undef UE_API
