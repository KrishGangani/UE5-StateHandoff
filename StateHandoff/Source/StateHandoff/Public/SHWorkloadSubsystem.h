// P09 StateHandoff (demo) - the workload and the checker: events, the oracle, the sweep, the counts.

#pragma once

#include "SHDemoState.h"
#include "SHDemoTypes.h"
#include "SHInvariantChecker.h"
#include "Subsystems/WorldSubsystem.h"
#include "SHWorkloadSubsystem.generated.h"

struct FSHAgentView;

/**
 * Owns everything the instrument needs, in every configuration (02 section 4.2):
 *  - the oracle: each agent's expected state, written with the same decided writes as the live copy;
 *  - the slot registry (world-level gameplay state);
 *  - the event points: the processors call ApplyEvent once per agent per point;
 *  - the sweep: CheckAgent compares the copy the configuration treats as real with the oracle, classifies any
 *    difference, records it, then re-syncs the oracle so the same root cause is counted once;
 *  - the reservation invariants (01 section 6.1: a slot has at most one holder, and the holder agrees it holds it),
 *    the transition counts, one CSV publish per frame, and the end-of-run report.
 * A world subsystem (not a processor) because both processors and the director need it, and its lifetime is the
 * world's. Plain arrays indexed by agent: the agent index is the entity's FSHAgentIdFragment.
 */
UCLASS()
class USHWorkloadSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** What the director passes in when the entities exist. */
	struct FRunConfig
	{
		ESHConfig Config = ESHConfig::B0;
		uint32 Seed = 1;
		/** Probability that an agent gets an event at one event point in one frame. */
		float EventRate = 0.02f;
		/** Run the sweep (correctness runs) or not (timing runs). Events and transition counts run either way. */
		bool bCheck = true;
		/** Whether demo actors may go into Mass's pool (ASHDemoAgent::CanBePooled). */
		bool bPooling = false;
		/** bKeepLowResActors, for the report only (the director sets it on the visualization trait). */
		bool bKeepLowRes = true;
		FString RunTag;
	};

	/** Starts the workload for NumAgents agents. Returns false (and logs why) if the configuration isn't usable. */
	bool BeginRun(const FRunConfig& InRun, int32 NumAgents);

	/**
	 * Counts only inside the capture window. The director asks on the BenchStart frame (on) and the BenchEnd frame (off);
	 * the switch takes effect at the next BeginFrame, the start of the next frame's simulation phase, so every counter
	 * covers whole frames: from the frame after BenchStart to the BenchEnd frame, the frames the CSV capture holds
	 * (-csvStartOnEvent starts the capture after the event's frame, 09 S2b-7; -csvStopOnEvent ends it with its frame).
	 */
	void RequestCapturing(bool bInCapturing) { bPendingCapturing = bInCapturing; }

	/** A baseline copied one agent's state (B1 hydrate or dehydrate, B2 push on spawn): counted per frame (CSV) and over
	 *  the capture (report), so every run shows that its baseline's path actually ran. */
	void NoteBaselineCopy()
	{
		++FrameCounts.BaselineCopies;
		BaselineCopies += bCapturing ? 1 : 0;
	}

	bool IsRunning() const { return bRunning; }
	ESHConfig GetConfig() const { return Run.Config; }
	bool IsCheckEnabled() const { return Run.bCheck; }
	bool IsPoolingEnabled() const { return bRunning && Run.bPooling; }

	/** Starts a new run frame. Called once per frame, first, by the simulation-event processor. */
	void BeginFrame()
	{
		++Frame;
		bCapturing = bPendingCapturing;
	}

	/** Makes this agent's event for this point and frame, if any, decides it on the oracle and applies the write to
	 *  both the oracle and the copy the configuration treats as real. */
	void ApplyEvent(ESHEventPoint Point, int32 Agent, const FSHAgentView& View, double Now);

	/** Every agent, every frame, every config: counts a representation change (during capture) and remembers it for
	 *  incident reports, and counts the agents that have an actor. */
	void ObserveAgent(EMassRepresentationType Current, bool bHasActor, FSHObservedFragment& Observed);

	/** Compares the agent's four channels with the oracle (checker on only). */
	void CheckAgent(int32 Agent, const FSHAgentView& View, const FSHObservedFragment& Observed);

	/** End of the frame: reservation invariants, then one CSV publish of the frame's counts. */
	void EndFrame();

	/** Writes Saved/SHRuns/<RunTag>_incidents.csv (summary header + one line per incident) and logs the summary. */
	void WriteReport() const;

protected:
	//~ Begin UWorldSubsystem
	/** Game and PIE worlds only. */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End UWorldSubsystem

private:
	/** One divergence, as written to the report. Channel == Count for the two reservation invariants. */
	struct FIncident
	{
		uint32 Frame = 0;
		int32 Agent = INDEX_NONE;
		ESHChannel Channel = ESHChannel::Count;
		/** ESHDivergence for a channel; for a reservation invariant 0 = slot leaked, 1 = slot double-held. */
		uint8 Class = 0;
		ESHTransitionKind LastKind = ESHTransitionKind::Count;
		/** INDEX_NONE when the agent never changed representation. */
		int32 FramesSinceTransition = INDEX_NONE;
	};

	/** Counts per frame, published once and reset (UE5-UpdateBudget measured about 50 ns per CSV stat call). */
	struct FFrameCounts
	{
		int32 EventsSimulation = 0;
		int32 EventsActor = 0;
		int32 Transitions = 0;
		int32 Incidents = 0;
		int32 AgentsWithActor = 0;
		int32 BaselineCopies = 0;
	};

	/** Records one incident (capture only), with the agent's latest transition from LastObserved. */
	void AddIncident(int32 Agent, ESHChannel Channel, uint8 Class);

	FRunConfig Run;
	bool bRunning = false;
	bool bCapturing = false;
	bool bPendingCapturing = false;
	uint32 Frame = 0;

	TArray<FSHAgentState> Expected;
	FSHSlotRegistry Registry;

	/** This frame's slot as each agent's configuration sees it, and its representation record (both filled by
	 *  CheckAgent, used by EndFrame and AddIncident). */
	TArray<int32> LiveSlots;
	TArray<FSHObservedFragment> LastObserved;
	/** Whether each slot / agent was already in violation last frame: an invariant is counted when it starts. */
	TArray<bool> SlotLeaked;
	TArray<bool> AgentDoubleHeld;

	FSHInvariantChecker Checkers[static_cast<int32>(ESHChannel::Count)];
	int64 TransitionCounts[static_cast<int32>(ESHTransitionKind::Count)] = {};
	int64 LeakedSlots = 0;
	int64 DoubleHeldSlots = 0;
	int64 EventsApplied = 0;
	int64 BaselineCopies = 0;
	uint32 CapturedFrames = 0;

	FFrameCounts FrameCounts;
	TArray<FIncident> Incidents;
};
