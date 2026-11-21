// P09 StateHandoff (demo) - FSHAgentView and SHRouter.

#include "SHStateRouter.h"

#include "SHAgentStateComponent.h"
#include "SHLifecycleComponent.h"

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
		case ESHConfig::B1:
			// The lifecycle pattern: the actor's components hold the state once BeginPlay / PrepareForGame hydrated them.
			return View.ActorState && View.Lifecycle && View.Lifecycle->IsHydrated() ? View.ActorState->Get(Channel) : View.Entity(Channel);
		case ESHConfig::B3:
			// Entity-owned: actor-side code would read and write the entity directly.
			return View.Entity(Channel);
		case ESHConfig::B0:
		case ESHConfig::B2:
		default:
			// What stock Mass leads a game to write: "if there's an actor, its components hold the state". B2 adds the
			// mirror and the push; the rule is the same.
			return View.ActorState ? View.ActorState->Get(Channel) : View.Entity(Channel);
		}
	}

	bool IsImplemented(const ESHConfig Config)
	{
		return Config == ESHConfig::B0 || Config == ESHConfig::B1 || Config == ESHConfig::B2 || Config == ESHConfig::B3;
	}
}
