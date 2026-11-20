// StateHandoffCore - a state channel used only by the automation tests.

#pragma once

#include "SHContractTypes.h"
#include "SHTestTypes.generated.h"

/** A channel with a float and an integer, like the demo's channels, so every comparison path is exercised. */
USTRUCT()
struct FSHTestChannel : public FSHChannelFragment
{
	GENERATED_BODY()

	UPROPERTY()
	float Value = 100.f;

	UPROPERTY()
	int32 Slot = INDEX_NONE;
};
