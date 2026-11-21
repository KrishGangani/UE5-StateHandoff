// P09 StateHandoff (demo) - automation tests for the baselines' routing rules (SHRouter). No world needed.
// Run: Tools\run_tests.bat (editor, headless), filter "StateHandoff.Router".

#include "Misc/AutomationTest.h"
#include "SHAgentStateComponent.h"
#include "SHLifecycleComponent.h"
#include "SHStateRouter.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

/** The friend that USHLifecycleComponent declares for these tests. */
struct FSHRouterTestAccess
{
	static void SetHydrated(USHLifecycleComponent& Lifecycle, const bool bHydrated) { Lifecycle.bHydrated = bHydrated; }
};

namespace SHRouterTests
{
	/** Editor and game contexts; one filter (ProductFilter), as the macro requires (AutomationTest.h:4126-4133). */
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	/** The entity's four copies, as a chunk would hold them. */
	struct FEntityCopies
	{
		FSHHealthState Health;
		FSHCooldownState Cooldown;
		FSHMemoryState Memory;
		FSHReservationState Reservation;

		FSHAgentView View()
		{
			FSHAgentView Result;
			Result.Health = &Health;
			Result.Cooldown = &Cooldown;
			Result.Memory = &Memory;
			Result.Reservation = &Reservation;
			return Result;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHRouterRulesTest, "StateHandoff.Router.Rules", SHRouterTests::Flags)
bool FSHRouterRulesTest::RunTest(const FString& Parameters)
{
	SHRouterTests::FEntityCopies Entity;
	USHAgentStateComponent* ActorState = NewObject<USHAgentStateComponent>(GetTransientPackage());
	USHLifecycleComponent* Lifecycle = NewObject<USHLifecycleComponent>(GetTransientPackage());

	FSHAgentView NoActor = Entity.View();
	FSHAgentView WithActor = Entity.View();
	WithActor.ActorState = ActorState;
	WithActor.Lifecycle = Lifecycle;

	for (int32 Index = 0; Index < static_cast<int32>(ESHChannel::Count); ++Index)
	{
		const ESHChannel Channel = static_cast<ESHChannel>(Index);
		const FSHChannelFragment* EntityCopy = &NoActor.Entity(Channel);
		const FSHChannelFragment* ActorCopy = &ActorState->Get(Channel);
		const FString Name = SHDemo::GetChannelName(Channel);

		// Without an actor, every rule falls back to the entity's copy.
		for (const ESHConfig Config : { ESHConfig::B0, ESHConfig::B1, ESHConfig::B2, ESHConfig::B3 })
		{
			TestTrue(FString::Printf(TEXT("%s, %s, no actor: entity"), SHDemo::GetConfigName(Config), *Name), &SHRouter::Authoritative(Config, Channel, NoActor) == EntityCopy);
		}
		TestTrue(FString::Printf(TEXT("B0, %s, actor: actor"), *Name), &SHRouter::Authoritative(ESHConfig::B0, Channel, WithActor) == ActorCopy);
		TestTrue(FString::Printf(TEXT("B2, %s, actor: actor"), *Name), &SHRouter::Authoritative(ESHConfig::B2, Channel, WithActor) == ActorCopy);
		FSHRouterTestAccess::SetHydrated(*Lifecycle, false);
		TestTrue(FString::Printf(TEXT("B1, %s, actor not hydrated: entity"), *Name), &SHRouter::Authoritative(ESHConfig::B1, Channel, WithActor) == EntityCopy);
		FSHRouterTestAccess::SetHydrated(*Lifecycle, true);
		TestTrue(FString::Printf(TEXT("B1, %s, actor hydrated: actor"), *Name), &SHRouter::Authoritative(ESHConfig::B1, Channel, WithActor) == ActorCopy);
		FSHRouterTestAccess::SetHydrated(*Lifecycle, false);
		TestTrue(FString::Printf(TEXT("B3, %s, actor: still entity"), *Name), &SHRouter::Authoritative(ESHConfig::B3, Channel, WithActor) == EntityCopy);
		WithActor.bHandoffBound = false;
		TestTrue(FString::Printf(TEXT("SH, %s, actor not bound: entity"), *Name), &SHRouter::Authoritative(ESHConfig::SH, Channel, WithActor) == EntityCopy);
		WithActor.bHandoffBound = true;
		TestTrue(FString::Printf(TEXT("SH, %s, actor bound: actor"), *Name), &SHRouter::Authoritative(ESHConfig::SH, Channel, WithActor) == ActorCopy);
		WithActor.bHandoffBound = false;
	}
	TestTrue(TEXT("B0-B3 implemented"), SHRouter::IsImplemented(ESHConfig::B0) && SHRouter::IsImplemented(ESHConfig::B1)
		&& SHRouter::IsImplemented(ESHConfig::B2) && SHRouter::IsImplemented(ESHConfig::B3));
	TestTrue(TEXT("SH implemented"), SHRouter::IsImplemented(ESHConfig::SH));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
