// StateHandoffCore - FSHInvariantChecker.

#include "SHInvariantChecker.h"

#include "UObject/Class.h"

ESHDivergence FSHInvariantChecker::Classify(const UScriptStruct& Type, const FSHChannelFragment& Expected, const FSHChannelFragment& Observed)
{
	// A wrong type would make the property comparison read past the end of the structs.
	checkf(Type.IsChildOf(FSHChannelFragment::StaticStruct()), TEXT("%s is not a state channel (FSHChannelFragment)"), *Type.GetName());

	if (Observed.Version < Expected.Version)
	{
		return ESHDivergence::Lost;
	}
	if (Observed.Version > Expected.Version)
	{
		return ESHDivergence::Duplicated;
	}
	// Same number of writes: every property must be equal, exactly (== for floats, UnrealType.h:1760-1769, so +0 equals
	// -0 and NaN never equals itself). Both copies ran the same writes, so equal values are what a correct copy gives.
	// FSHChannelFragment is the first (and only) base of every channel, so its address is the struct's address.
	return Type.CompareScriptStruct(&Expected, &Observed, PPF_None) ? ESHDivergence::None : ESHDivergence::Corrupted;
}

int64 FSHInvariantChecker::GetIncidents() const
{
	int64 Incidents = 0;
	for (int32 Index = static_cast<int32>(ESHDivergence::None) + 1; Index < static_cast<int32>(ESHDivergence::Count); ++Index)
	{
		Incidents += Counts[Index];
	}
	return Incidents;
}

void FSHInvariantChecker::Reset()
{
	for (int64& Count : Counts)
	{
		Count = 0;
	}
}
