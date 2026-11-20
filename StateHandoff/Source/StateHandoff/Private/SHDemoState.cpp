// P09 StateHandoff (demo) - channels, events, slot registry and the oracle's rules.

#include "SHDemoState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SHDemoState)

//----------------------------------------------------------------------//
// FSHAgentState
//----------------------------------------------------------------------//
FSHChannelFragment& FSHAgentState::Get(const ESHChannel Channel)
{
	return const_cast<FSHChannelFragment&>(static_cast<const FSHAgentState&>(*this).Get(Channel));
}

const FSHChannelFragment& FSHAgentState::Get(const ESHChannel Channel) const
{
	switch (Channel)
	{
	case ESHChannel::Health:   return Health;
	case ESHChannel::Cooldown: return Cooldown;
	case ESHChannel::Memory:   return Memory;
	default:                   check(Channel == ESHChannel::Reservation); return Reservation;
	}
}

//----------------------------------------------------------------------//
// FSHSlotRegistry
//----------------------------------------------------------------------//
void FSHSlotRegistry::Reset(const int32 NumSlots)
{
	Holders.Init(INDEX_NONE, FMath::Max(NumSlots, 0));
}

int32 FSHSlotRegistry::TakeFree(const uint32 Start, const int32 Agent)
{
	const int32 NumSlots = Holders.Num();
	for (int32 Step = 0; Step < NumSlots; ++Step)
	{
		const int32 Slot = static_cast<int32>((Start + static_cast<uint32>(Step)) % static_cast<uint32>(NumSlots));
		if (Holders[Slot] == INDEX_NONE)
		{
			Holders[Slot] = Agent;
			return Slot;
		}
	}
	return INDEX_NONE;
}

void FSHSlotRegistry::Free(const int32 Slot, const int32 Agent)
{
	if (Holders.IsValidIndex(Slot) && Holders[Slot] == Agent)
	{
		Holders[Slot] = INDEX_NONE;
	}
}

//----------------------------------------------------------------------//
// SHDemo
//----------------------------------------------------------------------//
namespace SHDemo
{
	namespace
	{
		/** The 32-bit finaliser of MurmurHash3: every input bit affects every output bit. Written out here, not taken
		 *  from the engine, so the event sequence can't change with an engine version. */
		uint32 Mix(uint32 Hash)
		{
			Hash ^= Hash >> 16;
			Hash *= 0x85ebca6bu;
			Hash ^= Hash >> 13;
			Hash *= 0xc2b2ae35u;
			Hash ^= Hash >> 16;
			return Hash;
		}
	}

	const UScriptStruct& GetChannelStruct(const ESHChannel Channel)
	{
		switch (Channel)
		{
		case ESHChannel::Health:   return *FSHHealthState::StaticStruct();
		case ESHChannel::Cooldown: return *FSHCooldownState::StaticStruct();
		case ESHChannel::Memory:   return *FSHMemoryState::StaticStruct();
		default:                   check(Channel == ESHChannel::Reservation); return *FSHReservationState::StaticStruct();
		}
	}

	const TCHAR* GetChannelName(const ESHChannel Channel)
	{
		switch (Channel)
		{
		case ESHChannel::Health:      return TEXT("Health");
		case ESHChannel::Cooldown:    return TEXT("Cooldown");
		case ESHChannel::Memory:      return TEXT("Memory");
		case ESHChannel::Reservation: return TEXT("Reservation");
		default:                      return TEXT("None");
		}
	}

	bool MakeEvent(const uint32 Seed, const uint32 Frame, const ESHEventPoint Point, const int32 Agent, const float Rate, FSHEvent& OutEvent)
	{
		// Chained mixing: each argument changes the whole hash. The first hash decides "event or not" and the type;
		// a second, independent hash gives the parameters, so type and parameters aren't correlated.
		const uint32 Hash = Mix(Seed ^ Mix(Frame ^ Mix(static_cast<uint32>(Point) ^ Mix(static_cast<uint32>(Agent)))));
		const uint32 Threshold = static_cast<uint32>(FMath::Clamp(Rate, 0.f, 1.f) * 65536.f);
		if ((Hash & 0xFFFFu) >= Threshold)
		{
			return false;
		}
		const uint32 Params = Mix(Hash);
		OutEvent.Type = static_cast<ESHEventType>((Hash >> 16) % static_cast<uint32>(ESHEventType::Count));
		OutEvent.Amount = 1.f + static_cast<float>(Params & 15u);
		OutEvent.Ability = static_cast<int32>((Params >> 4) & 1u);
		OutEvent.Position = FVector(static_cast<double>((Params >> 8) & 0xFFu) * 100.0, static_cast<double>((Params >> 16) & 0xFFu) * 100.0, 0.0);
		OutEvent.Salt = Params;
		return true;
	}

	FSHWrite Decide(const FSHAgentState& Expected, const FSHEvent& Event, const int32 Agent, const double Now, FSHSlotRegistry& Registry)
	{
		FSHWrite Write;
		Write.Type = Event.Type;
		Write.Time = Now;
		switch (Event.Type)
		{
		case ESHEventType::Damage:
		case ESHEventType::Heal:
			Write.Channel = ESHChannel::Health;
			Write.Amount = Event.Amount;
			break;

		case ESHEventType::UseAbility:
		{
			const double ReadyAt = Event.Ability == 0 ? Expected.Cooldown.ReadyAtA : Expected.Cooldown.ReadyAtB;
			if (Now >= ReadyAt)
			{
				Write.Channel = ESHChannel::Cooldown;
				Write.Ability = Event.Ability;
			}
			break;
		}

		case ESHEventType::Perceive:
			Write.Channel = ESHChannel::Memory;
			Write.Position = Event.Position;
			break;

		case ESHEventType::Claim:
			if (Expected.Reservation.Slot == INDEX_NONE && Registry.Num() > 0)
			{
				Write.Slot = Registry.TakeFree(Event.Salt, Agent);
				if (Write.Slot != INDEX_NONE)
				{
					Write.Channel = ESHChannel::Reservation;
				}
			}
			break;

		case ESHEventType::Release:
			if (Expected.Reservation.Slot != INDEX_NONE)
			{
				Registry.Free(Expected.Reservation.Slot, Agent);
				Write.Channel = ESHChannel::Reservation;
				Write.Slot = INDEX_NONE;
			}
			break;

		default:
			break;
		}
		return Write;
	}

	void ApplyWrite(FSHChannelFragment& Copy, const FSHWrite& Write)
	{
		switch (Write.Channel)
		{
		case ESHChannel::Health:
		{
			FSHHealthState& Health = static_cast<FSHHealthState&>(Copy);
			Health.Current = Write.Type == ESHEventType::Damage
				? FMath::Max(0.f, Health.Current - Write.Amount)
				: FMath::Min(Health.Max, Health.Current + Write.Amount);
			break;
		}
		case ESHChannel::Cooldown:
		{
			FSHCooldownState& Cooldown = static_cast<FSHCooldownState&>(Copy);
			(Write.Ability == 0 ? Cooldown.ReadyAtA : Cooldown.ReadyAtB) = Write.Time + (Write.Ability == 0 ? CooldownA : CooldownB);
			++Cooldown.Uses;
			break;
		}
		case ESHChannel::Memory:
		{
			FSHMemoryState& Memory = static_cast<FSHMemoryState&>(Copy);
			Memory.LastSeenPosition = Write.Position;
			Memory.LastSeenTime = Write.Time;
			Memory.Alert = static_cast<uint8>(FMath::Min<int32>(Memory.Alert + 1, MaxAlert));
			break;
		}
		case ESHChannel::Reservation:
			static_cast<FSHReservationState&>(Copy).Slot = Write.Slot;
			break;
		default:
			return; // nothing to write: no version bump
		}
		++Copy.Version;
	}
}
