// P09 StateHandoff (demo) - which copy of an agent's state each configuration treats as the real one.

#pragma once

#include "SHDemoState.h"
#include "SHDemoTypes.h"

class USHAgentStateComponent;
class USHLifecycleComponent;

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

	/** The actor's B1 lifecycle component (null without an actor). Read only by the B1 rule. */
	const USHLifecycleComponent* Lifecycle = nullptr;

	/** SH only: StateHandoff has bound this actor (FSHHandoffFragment::BoundActor is the entity's actor). */
	bool bHandoffBound = false;

	/** The entity's copy of one channel. */
	FSHChannelFragment& Entity(ESHChannel Channel) const;
};

namespace SHRouter
{
	/**
	 * The copy that this configuration's gameplay reads and writes (02 section 9). The checker reads through the same
	 * function, so it judges what each configuration's game would actually see.
	 *   B0, B2: the actor's copy if the entity has an actor (even a disabled one), else the entity's.
	 *   B1:     the actor's copy if its lifecycle component has hydrated it, else the entity's.
	 *   B3:     always the entity's (entity-owned state; the actor's copy is never used).
	 *   SH:     the actor's copy while StateHandoff has bound the actor, else the entity's (the ownership contract).
	 */
	FSHChannelFragment& Authoritative(ESHConfig Config, ESHChannel Channel, const FSHAgentView& View);

	/** True once a configuration's rule is implemented (all five since S5). */
	bool IsImplemented(ESHConfig Config);
}
