// P09 StateHandoff (demo) - the four state channels, the gameplay events that write them, and the oracle's rules.

#pragma once

#include "CoreMinimal.h"
#include "SHContractTypes.h"
#include "SHDemoState.generated.h"

class UScriptStruct;

/*
 * The demo's "actor-side systems" (01 section 6.1). Each channel stands in for state a real game keeps on the actor:
 * health (a GAS attribute set), cooldowns (GAS cooldown effects), AI memory (a blackboard / perception), a reserved
 * interaction slot (an actor-made Smart Object claim). They are modelled, not real GAS: a stated limit (01 section 8).
 *
 * Every channel derives from FSHChannelFragment, so the same type is the entity fragment, the actor component's
 * property and the oracle's expected copy. Every write goes through SHDemo::ApplyWrite, which bumps Version once.
 */

/** Health, as an attribute set would hold it. */
USTRUCT()
struct FSHHealthState : public FSHChannelFragment
{
	GENERATED_BODY()

	UPROPERTY()
	float Current = 100.f;

	UPROPERTY()
	float Max = 100.f;
};

/**
 * Two ability cooldowns, stored as absolute world times ("ready at"), not as seconds remaining: an absolute time needs
 * no simulation while the agent is dormant, so it is the same in every representation (01 section 6.1).
 */
USTRUCT()
struct FSHCooldownState : public FSHChannelFragment
{
	GENERATED_BODY()

	UPROPERTY()
	double ReadyAtA = 0.0;

	UPROPERTY()
	double ReadyAtB = 0.0;

	/** Successful ability uses: the decision outcome, so a wrong "ready" shows up even if the times match again later. */
	UPROPERTY()
	int32 Uses = 0;
};

/** What the agent's AI remembers about the player: where and when it was last seen, and how alert the agent is. */
USTRUCT()
struct FSHMemoryState : public FSHChannelFragment
{
	GENERATED_BODY()

	UPROPERTY()
	FVector LastSeenPosition = FVector::ZeroVector;

	UPROPERTY()
	double LastSeenTime = -1.0;

	/** 0 = calm .. SHDemo::MaxAlert. */
	UPROPERTY()
	uint8 Alert = 0;
};

/** The shared interaction slot the agent holds (INDEX_NONE = none). The slot registry holds the other side. */
USTRUCT()
struct FSHReservationState : public FSHChannelFragment
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Slot = INDEX_NONE;
};

/** The four channels, as an index. Count is the number of channels, and also means "no channel" in an FSHWrite. */
enum class ESHChannel : uint8
{
	Health,
	Cooldown,
	Memory,
	Reservation,
	Count
};

/** One agent's four channels together: the oracle's expected state. */
struct FSHAgentState
{
	FSHHealthState Health;
	FSHCooldownState Cooldown;
	FSHMemoryState Memory;
	FSHReservationState Reservation;

	FSHChannelFragment& Get(ESHChannel Channel);
	const FSHChannelFragment& Get(ESHChannel Channel) const;
};

/** What happens to an agent. One event touches one channel. */
enum class ESHEventType : uint8
{
	Damage,
	Heal,
	UseAbility,
	Perceive,
	Claim,
	Release,
	Count
};

/** Where in the frame an event is applied: before the representation is decided (entity-side gameplay), or after
 *  physics (actor-side gameplay, only for agents that have an actor). */
enum class ESHEventPoint : uint8
{
	Simulation,
	Actor
};

/** One generated event, before it is decided. */
struct FSHEvent
{
	ESHEventType Type = ESHEventType::Damage;
	/** Damage or heal amount: 1 .. 16. */
	float Amount = 0.f;
	/** Which ability (0 = A, 1 = B). */
	int32 Ability = 0;
	/** Where the player was seen. */
	FVector Position = FVector::ZeroVector;
	/** Where a claim starts looking for a free slot. */
	uint32 Salt = 0;
};

/** The decided effect of one event on one channel: exactly what both copies (expected and live) must apply. */
struct FSHWrite
{
	/** Count = the event changes nothing (an ability not ready, nothing to release, no free slot). */
	ESHChannel Channel = ESHChannel::Count;
	ESHEventType Type = ESHEventType::Damage;
	float Amount = 0.f;
	int32 Ability = 0;
	double Time = 0.0;
	FVector Position = FVector::ZeroVector;
	int32 Slot = INDEX_NONE;
};

/**
 * The interaction slots shared by all agents: one holder per slot. World-level gameplay state, so it never changes
 * representation; only the agents' own view of "which slot am I holding" (FSHReservationState) does.
 */
class FSHSlotRegistry
{
public:
	/** NumSlots free slots. */
	void Reset(int32 NumSlots);

	int32 Num() const { return Holders.Num(); }

	/** The agent holding Slot, or INDEX_NONE. */
	int32 GetHolder(int32 Slot) const { return Holders[Slot]; }

	/** Gives Agent the first free slot at or after Start (wrapping around); INDEX_NONE if every slot is taken. */
	int32 TakeFree(uint32 Start, int32 Agent);

	/** Frees Slot, if Agent is its holder. */
	void Free(int32 Slot, int32 Agent);

private:
	TArray<int32> Holders;
};

namespace SHDemo
{
	/** Cooldown of ability A and B (s). */
	constexpr double CooldownA = 2.0;
	constexpr double CooldownB = 5.0;
	/** Alert level cap. */
	constexpr uint8 MaxAlert = 3;

	/** The reflected type of a channel (for the checker). */
	const UScriptStruct& GetChannelStruct(ESHChannel Channel);

	/** A short name for logs and CSV columns. */
	const TCHAR* GetChannelName(ESHChannel Channel);

	/**
	 * The event agent Agent gets at this point of this frame, if any. Deterministic: a function of the arguments only,
	 * so every configuration sees exactly the same events (01 section 6.1). Rate = probability per agent per point.
	 */
	bool MakeEvent(uint32 Seed, uint32 Frame, ESHEventPoint Point, int32 Agent, float Rate, FSHEvent& OutEvent);

	/**
	 * Decides what Event does to agent Agent, from its expected state (the oracle) and the slot registry, at world time
	 * Now. Claims and releases update the registry here, once. Returns a write with Channel == Count if nothing changes.
	 */
	FSHWrite Decide(const FSHAgentState& Expected, const FSHEvent& Event, int32 Agent, double Now, FSHSlotRegistry& Registry);

	/** Applies a decided write to one copy of its channel and bumps that copy's version. Copy must be of Write's type. */
	void ApplyWrite(FSHChannelFragment& Copy, const FSHWrite& Write);
}
