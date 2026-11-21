// P09 StateHandoff (demo) - FSHAgentView and SHRouter.

#include "SHStateRouter.h"

#include "SHAgentStateComponent.h"

FSHChannelFragment& FSHAgentView::Entity(const ESHChannel Channel) const
{
	switch (Channel)
	{
	case ESHChannel::Health:   return *Health;
	case ESHChannel::Cooldown: return *Cooldown;
	case ESHChannel::Memory:   return *Memory;
	default:                   check(Channel == ESHChannel::Reservation); return *Reservation;
	}
}

namespace SHRouter
{
	FSHChannelFragment& Authoritative(const ESHConfig Config, const ESHChannel Channel, const FSHAgentView& View)
	{
		switch (Config)
		{
		case ESHConfig::B0:
		default:
			// What stock Mass leads a game to write: "if there's an actor, its components hold the state".
			return View.ActorState ? View.ActorState->Get(Channel) : View.Entity(Channel);
		}
	}

	bool IsImplemented(const ESHConfig Config)
	{
		return Config == ESHConfig::B0;
	}
}
