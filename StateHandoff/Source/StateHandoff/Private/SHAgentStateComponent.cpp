// P09 StateHandoff (demo) - USHAgentStateComponent.

#include "SHAgentStateComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SHAgentStateComponent)

USHAgentStateComponent::USHAgentStateComponent()
{
	// A data holder: nothing to do per frame, and nothing to initialise when it starts playing.
	PrimaryComponentTick.bCanEverTick = false;
}

FSHChannelFragment& USHAgentStateComponent::Get(const ESHChannel Channel)
{
	switch (Channel)
	{
	case ESHChannel::Health:   return Health;
	case ESHChannel::Cooldown: return Cooldown;
	case ESHChannel::Memory:   return Memory;
	default:                   check(Channel == ESHChannel::Reservation); return Reservation;
	}
}
