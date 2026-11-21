// P09 StateHandoff (demo) - the actor-side copy of an agent's state.

#pragma once

#include "Components/ActorComponent.h"
#include "SHDemoState.h"
#include "SHAgentStateComponent.generated.h"

/**
 * The demo's actor-side system: while an agent has an actor, this is where the stock patterns (B0-B2) keep its state,
 * the way a game keeps health in an attribute set or memory in a blackboard. A fresh actor starts with defaults; a
 * pooled one keeps whatever its previous agent left (01b #4).
 *
 * One UPROPERTY per channel, by name, so StateHandoff's reflection-based channels (S4) can find each one with
 * FindPropertyByName. No tick, no logic: every write comes from the workload, through the configuration's router.
 */
UCLASS(ClassGroup = "StateHandoff")
class USHAgentStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USHAgentStateComponent();

	/** The copy of one channel held by this actor. */
	FSHChannelFragment& Get(ESHChannel Channel);

	UPROPERTY(VisibleAnywhere, Category = "StateHandoff")
	FSHHealthState Health;

	UPROPERTY(VisibleAnywhere, Category = "StateHandoff")
	FSHCooldownState Cooldown;

	UPROPERTY(VisibleAnywhere, Category = "StateHandoff")
	FSHMemoryState Memory;

	UPROPERTY(VisibleAnywhere, Category = "StateHandoff")
	FSHReservationState Reservation;
};
