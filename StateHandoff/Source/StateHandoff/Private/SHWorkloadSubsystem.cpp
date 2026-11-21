// P09 StateHandoff (demo) - USHWorkloadSubsystem.

#include "SHWorkloadSubsystem.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "SHStateRouter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SHWorkloadSubsystem)

DEFINE_LOG_CATEGORY_STATIC(LogSHWorkload, Log, All);

// Per-frame counts of the workload and the checker.
CSV_DEFINE_CATEGORY(SHWorkload, true);

namespace SHWorkload
{
	/** Shared slots per agent: N / 4 gives enough contention that some claims fail (02 section 7). */
	constexpr int32 AgentsPerSlot = 4;

	const TCHAR* GetIncidentName(const ESHChannel Channel, const uint8 Class)
	{
		if (Channel == ESHChannel::Count)
		{
			return Class == 0 ? TEXT("SlotLeaked") : TEXT("SlotDoubleHeld");
		}
		switch (static_cast<ESHDivergence>(Class))
		{
		case ESHDivergence::Lost:       return TEXT("Lost");
		case ESHDivergence::Duplicated: return TEXT("Duplicated");
		case ESHDivergence::Corrupted:  return TEXT("Corrupted");
		default:                        return TEXT("None");
		}
	}
}

bool USHWorkloadSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool USHWorkloadSubsystem::BeginRun(const FRunConfig& InRun, const int32 NumAgents)
{
	if (!SHRouter::IsImplemented(InRun.Config))
	{
		UE_LOG(LogSHWorkload, Error, TEXT("Configuration %s is not implemented yet; the workload does not start."), SHDemo::GetConfigName(InRun.Config));
		return false;
	}
	Run = InRun;
	Expected.SetNum(NumAgents);
	Registry.Reset(FMath::Max(1, NumAgents / SHWorkload::AgentsPerSlot));
	LiveSlots.Init(INDEX_NONE, NumAgents);
	LastObserved.Init(FSHObservedFragment(), NumAgents);
	SlotLeaked.Init(false, Registry.Num());
	AgentDoubleHeld.Init(false, NumAgents);
	bRunning = true;
	UE_LOG(LogSHWorkload, Display, TEXT("Workload: config %s, %d agents, %d slots, event rate %.3f per agent per point, checker %s"),
		SHDemo::GetConfigName(Run.Config), NumAgents, Registry.Num(), Run.EventRate, Run.bCheck ? TEXT("on") : TEXT("off"));
	return true;
}

void USHWorkloadSubsystem::ApplyEvent(const ESHEventPoint Point, const int32 Agent, const FSHAgentView& View, const double Now)
{
	FSHEvent Event;
	if (!Expected.IsValidIndex(Agent) || !SHDemo::MakeEvent(Run.Seed, Frame, Point, Agent, Run.EventRate, Event))
	{
		return;
	}
	// Decide once, on the oracle; then the same write goes to the oracle and to the configuration's copy.
	const FSHWrite Write = SHDemo::Decide(Expected[Agent], Event, Agent, Now, Registry);
	if (Write.Channel == ESHChannel::Count)
	{
		return;
	}
	SHDemo::ApplyWrite(Expected[Agent].Get(Write.Channel), Write);
	SHDemo::ApplyWrite(SHRouter::Authoritative(Run.Config, Write.Channel, View), Write);

	if (bCapturing)
	{
		++EventsApplied;
	}
	++(Point == ESHEventPoint::Simulation ? FrameCounts.EventsSimulation : FrameCounts.EventsActor);
}

void USHWorkloadSubsystem::ObserveAgent(const EMassRepresentationType Current, const bool bHasActor, FSHObservedFragment& Observed)
{
	FrameCounts.AgentsWithActor += bHasActor ? 1 : 0;
	if (Current == Observed.LastRepresentation)
	{
		return;
	}
	const ESHTransitionKind Kind = SHDemo::ClassifyTransition(Observed.LastRepresentation, Current);
	Observed.LastRepresentation = Current;
	if (Kind == ESHTransitionKind::Count)
	{
		return;
	}
	Observed.LastKind = Kind;
	Observed.LastTransitionFrame = Frame;
	if (bCapturing)
	{
		++TransitionCounts[static_cast<int32>(Kind)];
		++FrameCounts.Transitions;
	}
}

void USHWorkloadSubsystem::CheckAgent(const int32 Agent, const FSHAgentView& View, const FSHObservedFragment& Observed)
{
	if (!Expected.IsValidIndex(Agent))
	{
		return;
	}
	LastObserved[Agent] = Observed;
	for (int32 Index = 0; Index < static_cast<int32>(ESHChannel::Count); ++Index)
	{
		const ESHChannel Channel = static_cast<ESHChannel>(Index);
		const UScriptStruct& Type = SHDemo::GetChannelStruct(Channel);
		FSHChannelFragment& ExpectedCopy = Expected[Agent].Get(Channel);
		const FSHChannelFragment& ObservedCopy = SHRouter::Authoritative(Run.Config, Channel, View);

		const ESHDivergence Result = FSHInvariantChecker::Classify(Type, ExpectedCopy, ObservedCopy);
		if (bCapturing)
		{
			Checkers[Index].Record(Result);
		}
		if (Result != ESHDivergence::None)
		{
			AddIncident(Agent, Channel, static_cast<uint8>(Result));
			// Re-sync: the oracle now expects what the game sees, so this root cause is counted once and later
			// writes are judged on their own.
			Type.CopyScriptStruct(&ExpectedCopy, &ObservedCopy);
		}
	}
	LiveSlots[Agent] = static_cast<const FSHReservationState&>(SHRouter::Authoritative(Run.Config, ESHChannel::Reservation, View)).Slot;
}

void USHWorkloadSubsystem::AddIncident(const int32 Agent, const ESHChannel Channel, const uint8 Class)
{
	if (!bCapturing)
	{
		return;
	}
	const FSHObservedFragment& Observed = LastObserved[Agent];
	FIncident& Incident = Incidents.AddDefaulted_GetRef();
	Incident.Frame = Frame;
	Incident.Agent = Agent;
	Incident.Channel = Channel;
	Incident.Class = Class;
	Incident.LastKind = Observed.LastKind;
	if (Observed.LastKind != ESHTransitionKind::Count)
	{
		Incident.FramesSinceTransition = static_cast<int32>(Frame - Observed.LastTransitionFrame);
	}
	++FrameCounts.Incidents;
}

void USHWorkloadSubsystem::EndFrame()
{
	if (!bRunning)
	{
		return;
	}
	if (Run.bCheck)
	{
		// The registry is the world's truth about slots; each agent's slot is what its configuration's game sees.
		// Leaked: the registry gives a slot to an agent that doesn't know it holds it (nobody will ever release it).
		// Double-held: an agent believes it holds a slot the registry gives to someone else, or to nobody (so the
		// registry has given it, or will give it, to a second agent). Each is counted once, when it starts.
		for (int32 Slot = 0; Slot < Registry.Num(); ++Slot)
		{
			const int32 Holder = Registry.GetHolder(Slot);
			const bool bLeaked = Holder != INDEX_NONE && LiveSlots.IsValidIndex(Holder) && LiveSlots[Holder] != Slot;
			if (bLeaked && !SlotLeaked[Slot] && bCapturing)
			{
				++LeakedSlots;
				AddIncident(Holder, ESHChannel::Count, 0);
			}
			SlotLeaked[Slot] = bLeaked;
		}
		for (int32 Agent = 0; Agent < LiveSlots.Num(); ++Agent)
		{
			const int32 Slot = LiveSlots[Agent];
			const bool bDoubleHeld = Slot != INDEX_NONE && (Slot < 0 || Slot >= Registry.Num() || Registry.GetHolder(Slot) != Agent);
			if (bDoubleHeld && !AgentDoubleHeld[Agent] && bCapturing)
			{
				++DoubleHeldSlots;
				AddIncident(Agent, ESHChannel::Count, 1);
			}
			AgentDoubleHeld[Agent] = bDoubleHeld;
		}
	}

	CapturedFrames += bCapturing ? 1 : 0;
	CSV_CUSTOM_STAT(SHWorkload, EventsSimulation, FrameCounts.EventsSimulation, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(SHWorkload, EventsActor, FrameCounts.EventsActor, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(SHWorkload, Transitions, FrameCounts.Transitions, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(SHWorkload, Incidents, FrameCounts.Incidents, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(SHWorkload, AgentsWithActor, FrameCounts.AgentsWithActor, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(SHWorkload, BaselineCopies, FrameCounts.BaselineCopies, ECsvCustomStatOp::Set);
	FrameCounts = FFrameCounts();
}

void USHWorkloadSubsystem::WriteReport() const
{
	int64 Transitions = 0;
	for (const int64 Count : TransitionCounts)
	{
		Transitions += Count;
	}
	int64 ChannelIncidents = 0;
	for (const FSHInvariantChecker& Checker : Checkers)
	{
		ChannelIncidents += Checker.GetIncidents();
	}
	const int64 AllIncidents = ChannelIncidents + LeakedSlots + DoubleHeldSlots;
	const double PerThousand = Transitions > 0 ? 1000.0 * static_cast<double>(AllIncidents) / static_cast<double>(Transitions) : 0.0;

	// Every count below covers the capture window only (SetCapturing): the frames the CSV capture holds.
	FString Report = FString::Printf(TEXT("# run=%s config=%s seed=%u eventrate=%.4f check=%d pooling=%d keeplowres=%d frames=%u events=%lld\n"),
		*Run.RunTag, SHDemo::GetConfigName(Run.Config), Run.Seed, Run.EventRate, Run.bCheck ? 1 : 0, Run.bPooling ? 1 : 0,
		Run.bKeepLowRes ? 1 : 0, CapturedFrames, EventsApplied);
	Report += FString::Printf(TEXT("# transitions=%lld"), Transitions);
	for (int32 Kind = 0; Kind < static_cast<int32>(ESHTransitionKind::Count); ++Kind)
	{
		Report += FString::Printf(TEXT(" %s=%lld"), SHDemo::GetTransitionName(static_cast<ESHTransitionKind>(Kind)), TransitionCounts[Kind]);
	}
	Report += FString::Printf(TEXT("\n# incidents=%lld per1000=%.4f slotleaked=%lld slotdoubleheld=%lld baselinecopies=%lld"), AllIncidents, PerThousand, LeakedSlots, DoubleHeldSlots, BaselineCopies);
	for (int32 Index = 0; Index < static_cast<int32>(ESHChannel::Count); ++Index)
	{
		const FSHInvariantChecker& Checker = Checkers[Index];
		Report += FString::Printf(TEXT(" %s:lost=%lld,dup=%lld,corrupt=%lld,checks=%lld"), SHDemo::GetChannelName(static_cast<ESHChannel>(Index)),
			Checker.GetCount(ESHDivergence::Lost), Checker.GetCount(ESHDivergence::Duplicated), Checker.GetCount(ESHDivergence::Corrupted),
			Checker.GetCount(ESHDivergence::None) + Checker.GetIncidents());
	}
	Report += TEXT("\nframe,agent,channel,class,last_transition,frames_since_transition\n");
	for (const FIncident& Incident : Incidents)
	{
		// An agent that never changed representation has no "frames since": the field is left empty.
		const FString Since = Incident.FramesSinceTransition >= 0 ? FString::FromInt(Incident.FramesSinceTransition) : FString();
		Report += FString::Printf(TEXT("%u,%d,%s,%s,%s,%s\n"), Incident.Frame, Incident.Agent, SHDemo::GetChannelName(Incident.Channel),
			SHWorkload::GetIncidentName(Incident.Channel, Incident.Class), SHDemo::GetTransitionName(Incident.LastKind), *Since);
	}

	UE_LOG(LogSHWorkload, Display, TEXT("Checker: %lld transitions, %lld incidents (%.4f per 1,000 transitions); slots leaked %lld, double-held %lld"),
		Transitions, AllIncidents, PerThousand, LeakedSlots, DoubleHeldSlots);

	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SHRuns"), Run.RunTag + TEXT("_incidents.csv"));
	if (FFileHelper::SaveStringToFile(Report, *Path))
	{
		UE_LOG(LogSHWorkload, Display, TEXT("Run report written: %s"), *FPaths::ConvertRelativePathToFull(Path));
	}
	else
	{
		UE_LOG(LogSHWorkload, Error, TEXT("Could not write the run report: %s"), *Path);
	}
}
