// P09 StateHandoff (demo) - automation tests for the event generator, the oracle's rules and the slot registry.
// No world needed. Run: Tools\run_tests.bat (editor, headless), filter "StateHandoff.Demo".

#include "Misc/AutomationTest.h"
#include "SHDemoState.h"
#include "SHInvariantChecker.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace SHDemoTests
{
	/** Editor and game contexts; one filter (ProductFilter), as the macro requires (AutomationTest.h:4126-4133). */
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	/** Decides an event of one type for agent 0 and applies the write to the expected state, as the workload does. */
	FSHWrite Run(FSHAgentState& Expected, ESHEventType Type, double Now, FSHSlotRegistry& Registry, float Amount = 10.f, int32 Ability = 0, uint32 Salt = 0)
	{
		FSHEvent Event;
		Event.Type = Type;
		Event.Amount = Amount;
		Event.Ability = Ability;
		Event.Salt = Salt;
		const FSHWrite Write = SHDemo::Decide(Expected, Event, 0, Now, Registry);
		if (Write.Channel != ESHChannel::Count)
		{
			SHDemo::ApplyWrite(Expected.Get(Write.Channel), Write);
		}
		return Write;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHDemoEventsDeterministicTest, "StateHandoff.Demo.EventsDeterministic", SHDemoTests::Flags)
bool FSHDemoEventsDeterministicTest::RunTest(const FString& Parameters)
{
	// Same arguments, same event: every configuration of a run sees the same events.
	int32 Events = 0;
	int32 Mismatches = 0;
	for (uint32 Frame = 0; Frame < 200; ++Frame)
	{
		for (int32 Agent = 0; Agent < 200; ++Agent)
		{
			FSHEvent A, B;
			const bool bA = SHDemo::MakeEvent(7, Frame, ESHEventPoint::Simulation, Agent, 0.05f, A);
			const bool bB = SHDemo::MakeEvent(7, Frame, ESHEventPoint::Simulation, Agent, 0.05f, B);
			Events += bA ? 1 : 0;
			Mismatches += (bA != bB || (bA && (A.Type != B.Type || A.Amount != B.Amount || A.Salt != B.Salt))) ? 1 : 0;
		}
	}
	TestEqual(TEXT("identical twice"), Mismatches, 0);
	// 40,000 draws at 5%: 2,000 expected. Generous bounds: this checks the rate is wired, not the hash's statistics.
	TestTrue(FString::Printf(TEXT("rate near 5%% (%d of 40000)"), Events), Events > 1600 && Events < 2400);

	FSHEvent Unused;
	TestFalse(TEXT("rate 0: never"), SHDemo::MakeEvent(7, 1, ESHEventPoint::Actor, 3, 0.f, Unused));
	TestTrue(TEXT("rate 1: always"), SHDemo::MakeEvent(7, 1, ESHEventPoint::Actor, 3, 1.f, Unused));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHDemoHealthTest, "StateHandoff.Demo.Health", SHDemoTests::Flags)
bool FSHDemoHealthTest::RunTest(const FString& Parameters)
{
	FSHAgentState State;
	FSHSlotRegistry Registry;
	SHDemoTests::Run(State, ESHEventType::Damage, 1.0, Registry, 30.f);
	TestEqual(TEXT("damage"), State.Health.Current, 70.f);
	SHDemoTests::Run(State, ESHEventType::Damage, 1.0, Registry, 500.f);
	TestEqual(TEXT("damage clamps at 0"), State.Health.Current, 0.f);
	SHDemoTests::Run(State, ESHEventType::Heal, 1.0, Registry, 500.f);
	TestEqual(TEXT("heal clamps at max"), State.Health.Current, 100.f);
	TestEqual(TEXT("one version per write"), static_cast<int32>(State.Health.Version), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHDemoCooldownTest, "StateHandoff.Demo.Cooldown", SHDemoTests::Flags)
bool FSHDemoCooldownTest::RunTest(const FString& Parameters)
{
	FSHAgentState State;
	FSHSlotRegistry Registry;
	TestTrue(TEXT("first use succeeds"), SHDemoTests::Run(State, ESHEventType::UseAbility, 10.0, Registry).Channel == ESHChannel::Cooldown);
	TestEqual(TEXT("ready again after the cooldown"), State.Cooldown.ReadyAtA, 10.0 + SHDemo::CooldownA);
	TestTrue(TEXT("second use too early: no write"), SHDemoTests::Run(State, ESHEventType::UseAbility, 11.0, Registry).Channel == ESHChannel::Count);
	TestTrue(TEXT("ability B is independent"), SHDemoTests::Run(State, ESHEventType::UseAbility, 11.0, Registry, 0.f, 1).Channel == ESHChannel::Cooldown);
	TestTrue(TEXT("A ready again at 12.0"), SHDemoTests::Run(State, ESHEventType::UseAbility, 12.0, Registry).Channel == ESHChannel::Cooldown);
	TestEqual(TEXT("uses"), State.Cooldown.Uses, 3);
	TestEqual(TEXT("a failed use writes nothing"), static_cast<int32>(State.Cooldown.Version), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHDemoReservationTest, "StateHandoff.Demo.Reservation", SHDemoTests::Flags)
bool FSHDemoReservationTest::RunTest(const FString& Parameters)
{
	FSHSlotRegistry Registry;
	Registry.Reset(2);
	FSHAgentState State;

	SHDemoTests::Run(State, ESHEventType::Claim, 1.0, Registry, 0.f, 0, /*Salt=*/1);
	TestEqual(TEXT("claim takes the slot at the salt"), State.Reservation.Slot, 1);
	TestEqual(TEXT("registry holder"), Registry.GetHolder(1), 0);
	TestTrue(TEXT("a second claim while holding: no write"), SHDemoTests::Run(State, ESHEventType::Claim, 1.0, Registry).Channel == ESHChannel::Count);

	SHDemoTests::Run(State, ESHEventType::Release, 2.0, Registry);
	TestEqual(TEXT("release clears the agent's slot"), State.Reservation.Slot, int32(INDEX_NONE));
	TestEqual(TEXT("release frees the registry slot"), Registry.GetHolder(1), int32(INDEX_NONE));
	TestTrue(TEXT("release with nothing held: no write"), SHDemoTests::Run(State, ESHEventType::Release, 2.0, Registry).Channel == ESHChannel::Count);

	// Fill both slots with other agents: a claim then finds nothing.
	Registry.TakeFree(0, 5);
	Registry.TakeFree(0, 6);
	TestTrue(TEXT("full registry: no write"), SHDemoTests::Run(State, ESHEventType::Claim, 3.0, Registry).Channel == ESHChannel::Count);
	Registry.Free(0, 99);
	TestEqual(TEXT("only the holder can free a slot"), Registry.GetHolder(0), 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHDemoCopiesAgreeTest, "StateHandoff.Demo.CopiesAgree", SHDemoTests::Flags)
bool FSHDemoCopiesAgreeTest::RunTest(const FString& Parameters)
{
	// The workload decides once and applies the same write to two copies: the checker must find them identical.
	// One copy that misses a write must be classified Lost.
	FSHSlotRegistry Registry;
	Registry.Reset(16);
	FSHAgentState Expected;
	FSHAgentState Live;
	FSHAgentState Stale;
	for (uint32 Frame = 0; Frame < 2000; ++Frame)
	{
		FSHEvent Event;
		if (SHDemo::MakeEvent(3, Frame, ESHEventPoint::Simulation, 0, 0.5f, Event))
		{
			const FSHWrite Write = SHDemo::Decide(Expected, Event, 0, Frame / 60.0, Registry);
			if (Write.Channel != ESHChannel::Count)
			{
				SHDemo::ApplyWrite(Expected.Get(Write.Channel), Write);
				SHDemo::ApplyWrite(Live.Get(Write.Channel), Write);
				if (Frame < 1000)
				{
					SHDemo::ApplyWrite(Stale.Get(Write.Channel), Write); // stops receiving writes half-way
				}
			}
		}
	}
	int32 Differences = 0;
	int32 Lost = 0;
	for (int32 Index = 0; Index < static_cast<int32>(ESHChannel::Count); ++Index)
	{
		const ESHChannel Channel = static_cast<ESHChannel>(Index);
		const UScriptStruct& Type = SHDemo::GetChannelStruct(Channel);
		Differences += FSHInvariantChecker::Classify(Type, Expected.Get(Channel), Live.Get(Channel)) != ESHDivergence::None ? 1 : 0;
		Lost += FSHInvariantChecker::Classify(Type, Expected.Get(Channel), Stale.Get(Channel)) == ESHDivergence::Lost ? 1 : 0;
	}
	TestEqual(TEXT("same writes: identical copies"), Differences, 0);
	TestEqual(TEXT("missed writes: every channel Lost"), Lost, static_cast<int32>(ESHChannel::Count));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
