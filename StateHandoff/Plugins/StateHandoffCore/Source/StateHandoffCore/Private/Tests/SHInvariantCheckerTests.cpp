// StateHandoffCore - automation tests for the checker's classification and counting (no world needed).
// Run: Tools\run_tests.bat (editor, headless), filter "StateHandoff.Checker".

#include "Misc/AutomationTest.h"
#include "SHInvariantChecker.h"
#include "Tests/SHTestTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace SHCheckerTests
{
	/** Editor and game contexts; one filter (ProductFilter), as the macro requires (AutomationTest.h:4126-4133). */
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter;

	/** One write, the way the demo channels are written: change the value, bump the version. */
	void Write(FSHTestChannel& Channel, const float Delta)
	{
		Channel.Value += Delta;
		++Channel.Version;
	}

	/** The classification as an integer: TestEqual has no overload for an enum class. */
	int32 Classify(const FSHTestChannel& Expected, const FSHTestChannel& Observed)
	{
		return static_cast<int32>(FSHInvariantChecker::Classify(*FSHTestChannel::StaticStruct(), Expected, Observed));
	}

	int32 AsInt(const ESHDivergence Result)
	{
		return static_cast<int32>(Result);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHCheckerIdenticalTest, "StateHandoff.Checker.Identical", SHCheckerTests::Flags)
bool FSHCheckerIdenticalTest::RunTest(const FString& Parameters)
{
	using namespace SHCheckerTests;
	FSHTestChannel Expected;
	FSHTestChannel Observed;
	TestEqual(TEXT("two defaults"), Classify(Expected, Observed), AsInt(ESHDivergence::None));

	// The same writes in the same order give bit-identical floats: the property comparison must say None.
	for (int32 Step = 0; Step < 10; ++Step)
	{
		Write(Expected, -7.3f);
		Write(Observed, -7.3f);
	}
	TestEqual(TEXT("same ten writes"), Classify(Expected, Observed), AsInt(ESHDivergence::None));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHCheckerLostTest, "StateHandoff.Checker.Lost", SHCheckerTests::Flags)
bool FSHCheckerLostTest::RunTest(const FString& Parameters)
{
	using namespace SHCheckerTests;
	FSHTestChannel Expected;
	Write(Expected, -10.f);
	Write(Expected, -10.f);

	// A copy that missed the second write, and a copy reset to defaults (a new actor that was never restored).
	FSHTestChannel MissedOne;
	Write(MissedOne, -10.f);
	const FSHTestChannel Defaults{};
	TestEqual(TEXT("one write missing"), Classify(Expected, MissedOne), AsInt(ESHDivergence::Lost));
	TestEqual(TEXT("reset to defaults"), Classify(Expected, Defaults), AsInt(ESHDivergence::Lost));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHCheckerDuplicatedTest, "StateHandoff.Checker.Duplicated", SHCheckerTests::Flags)
bool FSHCheckerDuplicatedTest::RunTest(const FString& Parameters)
{
	using namespace SHCheckerTests;
	FSHTestChannel Expected;
	Write(Expected, -10.f);
	FSHTestChannel Twice = Expected;
	Write(Twice, -10.f); // the same event applied a second time, e.g. to both copies
	TestEqual(TEXT("one write too many"), Classify(Expected, Twice), AsInt(ESHDivergence::Duplicated));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHCheckerCorruptedTest, "StateHandoff.Checker.Corrupted", SHCheckerTests::Flags)
bool FSHCheckerCorruptedTest::RunTest(const FString& Parameters)
{
	using namespace SHCheckerTests;
	FSHTestChannel Expected;
	Write(Expected, -10.f);

	// Same number of writes, different content: another entity's state (a reused pooled actor), or a stale value
	// written over the right one with its version.
	FSHTestChannel Foreign;
	Write(Foreign, -25.f);
	FSHTestChannel OtherSlot = Expected;
	OtherSlot.Slot = 3;
	TestEqual(TEXT("different float"), Classify(Expected, Foreign), AsInt(ESHDivergence::Corrupted));
	TestEqual(TEXT("different integer"), Classify(Expected, OtherSlot), AsInt(ESHDivergence::Corrupted));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSHCheckerCountsTest, "StateHandoff.Checker.Counts", SHCheckerTests::Flags)
bool FSHCheckerCountsTest::RunTest(const FString& Parameters)
{
	FSHInvariantChecker Checker;
	Checker.Record(ESHDivergence::None);
	Checker.Record(ESHDivergence::None);
	Checker.Record(ESHDivergence::Lost);
	Checker.Record(ESHDivergence::Duplicated);
	Checker.Record(ESHDivergence::Corrupted);
	Checker.Record(ESHDivergence::Lost);

	TestEqual(TEXT("None"), Checker.GetCount(ESHDivergence::None), int64(2));
	TestEqual(TEXT("Lost"), Checker.GetCount(ESHDivergence::Lost), int64(2));
	TestEqual(TEXT("incidents exclude None"), Checker.GetIncidents(), int64(4));

	Checker.Reset();
	TestEqual(TEXT("after Reset"), Checker.GetIncidents() + Checker.GetCount(ESHDivergence::None), int64(0));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
