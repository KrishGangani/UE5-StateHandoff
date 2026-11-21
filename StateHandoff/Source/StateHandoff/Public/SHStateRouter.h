// P09 StateHandoff (demo) - which copy of an agent's state each configuration treats as the real one.

#pragma once

#include "SHDemoState.h"
#include "SHDemoTypes.h"

class USHAgentStateComponent;

/**
 * Everything needed to find one agent's state this frame: its entity copies (fragments) and, if the entity has an
 * actor, the actor's copy. Built by the processors from their chunk views, one agent at a time. Plain pointers:
 * valid only for the current chunk iteration, never stored.
 */
struct FSHAgentView
{
	FSHHealthState* Health = nullptr;
	FSHCooldownState* Cooldown = nullptr;
	FSHMemoryState* Memory = nullptr;
	FSHReservationState* Reservation = nullptr;

	/** The actor's state component, or null when the entity has no actor (or the actor has no such component). */
	USHAgentStateComponent* ActorState = nullptr;

	/** The entity's copy of one channel. */
	FSHChannelFragment& Entity(ESHChannel Channel) const;
};

namespace SHRouter
{
	/**
	 * The copy that this configuration's gameplay reads and writes (02 section 9). The checker reads through the same
	 * function, so it judges what each configuration's game would actually see.
	 *   B0: the actor's copy if the entity has an actor (even a disabled one), else the entity's.
	 *   B1-B3 (S3) and SH (S5) are added with their implementations; until then they use the B0 rule, and the
	 *   workload subsystem refuses to start them.
	 */
	FSHChannelFragment& Authoritative(ESHConfig Config, ESHChannel Channel, const FSHAgentView& View);

	/** True once a configuration's rule is implemented (S2: B0 only). */
	bool IsImplemented(ESHConfig Config);
}
