// P09 StateHandoff (demo) - benchmark director: builds the Mass workload in code and runs one timed, repeatable run.

#pragma once

#include "Mass/EntityHandle.h"
#include "SHDemoTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "SHBenchmarkSubsystem.generated.h"

class ACameraActor;
class USHWorkloadSubsystem;
class UMassEntityConfigAsset;

/**
 * Runs the benchmark for one game world. A world subsystem, not a placed actor: the project has no hand-authored map,
 * so nothing would place a director, and a world subsystem has exactly the world's lifetime (as in UE5-UpdateBudget).
 * Tickable, because it drives the camera and the run timeline every frame.
 *
 * Timeline (world time since the first tick):
 *   frame 1                 build the entity config in code and spawn N Mass entities from the seed
 *   [0, Warmup)             warm-up: the camera orbits (no flips yet), nothing is measured; at least 1 s
 *   Warmup                  CSV_EVENT "BenchStart"  (captures start with -csvStartOnEvent=BenchStart)
 *   every FlipEvery s       the camera jumps to the other hot spot: CSV_EVENT "BenchFlip". Every third flip is a
 *                           "bounce": the camera jumps back BounceFrames frames later (a second "BenchFlip")
 *   Warmup + Capture        CSV_EVENT "BenchEnd" (timed runs stop the capture with -csvStopOnEvent=BenchEnd)
 *   next frame              representation counts are logged (outside the capture)
 *   a few frames later      with -SHExit the game quits, after the CSV profiler has written its file
 * Flips and the orbit are timed from the planned BenchStart, so the captured path never depends on the warm-up length.
 * With a fixed time step (-deterministic -FPS=60, as every benchmark run uses) the flips fall on the same frames in
 * every run. Invalid settings start no workload, and with -SHExit the game exits with code 3.
 *
 * Command line (all optional):
 *   -SHAgents=N      Mass entities to spawn (default 0: no workload, normal empty map)
 *   -SHSeed=S        placement seed (default 1)
 *   -SHWarmup=SEC    warm-up seconds (default 10, at least 1)
 *   -SHCapture=SEC   measured seconds (default 60)
 *   -SHFlipEvery=SEC seconds between camera flips (default 4; 0 = no flips, the camera only orbits; else at least 1)
 *   -SHRunTag=NAME   CSV metadata tag (default "run")
 *   -SHExit          quit when the run is finished
 *   -SHLogCounts     also log representation counts at every flip (functional checks; costs time on captured frames)
 *   -SHConfig=ID     B0 | B1 | B2 | B3 | SH (default B0; SH is refused until S5)
 *   -SHEventRate=P   probability per agent, per event point, per frame (default 0.02)
 *   -SHCheck=0|1     run the checker's sweep (default 1; timing runs pass 0)
 *   -SHParked        the camera holds its first position: no orbit and no flips (steady state: no transitions)
 *   -SHPooling=0|1   demo actors may go into Mass's actor pool (default 0)
 *   -SHKeepLowRes=0|1 the engine keeps a disabled actor while the entity is an instanced mesh (default 1, the engine's)
 */
UCLASS()
class USHBenchmarkSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableGameObject

protected:
	//~ Begin UWorldSubsystem
	/** Game and PIE worlds only: the director must never run in an editor world. */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End UWorldSubsystem

private:
	/** Values read once from the command line. */
	struct FRunSettings
	{
		int32 NumAgents = 0;
		int32 Seed = 1;
		double WarmupSeconds = 10.0;
		double CaptureSeconds = 60.0;
		double FlipEverySeconds = 4.0;
		FString RunTag = TEXT("run");
		bool bExitWhenDone = false;
		bool bLogCounts = false;
		ESHConfig Config = ESHConfig::B0;
		bool bConfigValid = true;
		float EventRate = 0.02f;
		bool bCheck = true;
		bool bParked = false;
		bool bPooling = false;
		bool bKeepLowRes = true;
	};

	enum class ERunPhase : uint8
	{
		Unread,   // settings not read yet (before the first tick)
		Idle,     // no workload requested
		WarmUp,
		Capture,
		Done
	};

	static FRunSettings ReadRunSettings();

	/** First tick: reads the settings and, if they are valid, builds the config, spawns the entities and the camera. */
	void StartRun(UWorld& World, double Now);

	/** Logs every setting that would produce a misleading run; false if there is any. */
	bool ValidateRunSettings() const;

	/** Builds the entity config in code (01b section 4): demo state, LOD collector and movable visualization traits. */
	void BuildEntityConfig();

	/** Spawns the N entities on a jittered grid (same seed, same placement). Returns a hash of the start positions. */
	uint32 SpawnAgents(UWorld& World);

	/** Moves the camera: an orbit around the current hot spot, which changes at each flip. */
	void UpdateCamera(UWorld& World, double SecondsFromBenchStart);

	/** Jumps the camera to the other hot spot and marks it in the CSV ("BenchFlip"). */
	void Teleport();

	/** Logs how many entities are in each representation, and how many hold an actor (a cheap S1 sanity check). */
	void LogRepresentationCounts(const TCHAR* When) const;

	FRunSettings Settings;
	ERunPhase Phase = ERunPhase::Unread;
	double FirstTickTime = 0.0;
	double ArenaHalfExtent = 0.0;
	int32 FramesSinceEnd = 0;

	/** Scheduled flips done so far, all camera jumps so far (bounce returns included), frames until a bounce returns. */
	int32 ScheduledFlips = 0;
	int32 Teleports = 0;
	int32 BounceFramesLeft = 0;
	/** Which hot spot the camera orbits: 0 = west, 1 = east. */
	int32 HotSpot = 0;

	/** The code-built config; kept alive here, because its traits are UObjects the entity template refers to. */
	UPROPERTY(Transient)
	TObjectPtr<UMassEntityConfigAsset> EntityConfig;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> Camera;

	/** The workload and checker of this world; set when the run starts (a world subsystem lives as long as the world). */
	UPROPERTY(Transient)
	TObjectPtr<USHWorkloadSubsystem> Workload;

	TArray<FMassEntityHandle> Agents;
};
