// P09 StateHandoff (demo) - USHBenchmarkSubsystem.

#include "SHBenchmarkSubsystem.h"

#include "Camera/CameraActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Mass/EntityFragments.h"
#include "MassActorSubsystem.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassLODTrait.h"
#include "MassMovableVisualizationTrait.h"
#include "MassRepresentationFragments.h"
#include "MassSpawnerSubsystem.h"
#include "Misc/App.h"
#include "Math/RandomStream.h"
#include "Misc/CommandLine.h"
#include "Misc/Crc.h"
#include "Misc/Parse.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "SHAgentStateComponent.h"
#include "SHB2Mirror.h"
#include "SHDemoAgent.h"
#include "SHDemoStateTrait.h"
#include "SHDemoTypes.h"
#include "SHStateHandoff.h"
#include "SHStateRouter.h"
#include "SHWorkloadSubsystem.h"
#include "Stats/Stats.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SHBenchmarkSubsystem)

DEFINE_LOG_CATEGORY_STATIC(LogSHBench, Log, All);

// CSV category for the run's events (BenchStart / BenchFlip / BenchEnd).
CSV_DEFINE_CATEGORY(SHBench, true);

namespace SHBench
{
	/** Distance between grid cells (cm). 4 m keeps 4,000 agents inside a 250 m square. */
	constexpr double GridSpacing = 400.0;
	/** Random offset of each agent inside its cell (cm), so the layout isn't perfectly regular. */
	constexpr double GridJitter = 100.0;
	/** Height of agent centres (cm): the 1 m engine cylinder and the 0.6 m cube rest on the floor. */
	constexpr double AgentHeight = 50.0;

	/** Camera: an orbit of 15 m radius, 4 m up, around the current hot spot, one lap every 20 s, looking at its centre. */
	constexpr double CameraHeight = 400.0;
	constexpr double OrbitRadius = 1500.0;
	constexpr double OrbitSeconds = 20.0;
	/** The hot spots sit at +/- this fraction of the arena half-width on the X axis. */
	constexpr double HotSpotFraction = 0.5;
	/** Every BounceEvery-th flip is a bounce: the camera jumps back after BounceFrames frames (H1: quick returns). */
	constexpr int32 BounceEvery = 3;
	constexpr int32 BounceFrames = 10;

	/**
	 * Visualization LOD settings (cm and entity counts). S1 starting values; S3 calibrates them so a correctness batch
	 * reaches >= 20,000 transitions per config (01 section 6.4, SC1). The engine defaults allow only about 150 actors
	 * (MassRepresentationFragments.h:313; MassVisualizationTrait.cpp:46-49).
	 */
	constexpr float BaseLODDistance[] = { 0.f, 2000.f, 4000.f, 8000.f };
	constexpr float VisibleLODDistance[] = { 0.f, 3000.f, 6000.f, 12000.f };
	constexpr int32 LODMaxCountHigh = 200;
	constexpr int32 LODMaxCountMedium = 400;
	constexpr int32 LODMaxCountLow = 3000;

	/** The shortest warm-up: frame 1 spawns every entity and the first actors follow, which must stay out of the capture. */
	constexpr double MinWarmupSeconds = 1.0;
	/** The shortest flip period: a bounce (BounceFrames) must return well before the next scheduled flip. */
	constexpr double MinFlipEverySeconds = 1.0;

	/**
	 * Frames to wait after BenchEnd before quitting. Timed runs pass -csvStopOnEvent=BenchEnd (CsvProfiler.cpp:5090), so
	 * the capture ends with the BenchEnd frame and these frames are never measured; they give the profiler time to write.
	 */
	constexpr int32 ExitDelayFrames = 3;
}

bool USHBenchmarkSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

USHBenchmarkSubsystem::FRunSettings USHBenchmarkSubsystem::ReadRunSettings()
{
	const TCHAR* CommandLine = FCommandLine::Get();
	FRunSettings Read;
	FParse::Value(CommandLine, TEXT("SHAgents="), Read.NumAgents);
	FParse::Value(CommandLine, TEXT("SHSeed="), Read.Seed);
	FParse::Value(CommandLine, TEXT("SHWarmup="), Read.WarmupSeconds);
	FParse::Value(CommandLine, TEXT("SHCapture="), Read.CaptureSeconds);
	FParse::Value(CommandLine, TEXT("SHFlipEvery="), Read.FlipEverySeconds);
	FParse::Value(CommandLine, TEXT("SHRunTag="), Read.RunTag);
	Read.bExitWhenDone = FParse::Param(CommandLine, TEXT("SHExit"));
	Read.bLogCounts = FParse::Param(CommandLine, TEXT("SHLogCounts"));
	FString ConfigName;
	if (FParse::Value(CommandLine, TEXT("SHConfig="), ConfigName))
	{
		Read.bConfigValid = SHDemo::ParseConfig(ConfigName, Read.Config);
	}
	FParse::Value(CommandLine, TEXT("SHEventRate="), Read.EventRate);
	int32 Check = 1;
	FParse::Value(CommandLine, TEXT("SHCheck="), Check);
	Read.bCheck = Check != 0;
	Read.bParked = FParse::Param(CommandLine, TEXT("SHParked"));
	int32 Pooling = 0;
	FParse::Value(CommandLine, TEXT("SHPooling="), Pooling);
	Read.bPooling = Pooling != 0;
	int32 KeepLowRes = 1;
	FParse::Value(CommandLine, TEXT("SHKeepLowRes="), KeepLowRes);
	Read.bKeepLowRes = KeepLowRes != 0;
	if (Read.bParked)
	{
		Read.FlipEverySeconds = 0.0; // a parked camera never flips, whatever -SHFlipEvery says
	}
	return Read;
}

bool USHBenchmarkSubsystem::ValidateRunSettings() const
{
	bool bValid = true;
	if (Settings.WarmupSeconds < SHBench::MinWarmupSeconds || Settings.CaptureSeconds <= 0.0)
	{
		UE_LOG(LogSHBench, Error, TEXT("Invalid run timing: warm-up %.3f s, capture %.3f s (warm-up must be >= %.1f s, capture > 0)."),
			Settings.WarmupSeconds, Settings.CaptureSeconds, SHBench::MinWarmupSeconds);
		bValid = false;
	}
	if (Settings.FlipEverySeconds != 0.0
		&& (Settings.FlipEverySeconds < SHBench::MinFlipEverySeconds || Settings.FlipEverySeconds >= Settings.CaptureSeconds))
	{
		UE_LOG(LogSHBench, Error, TEXT("-SHFlipEvery=%.3f must be 0 (no flips) or between %.1f s and the %.3f s capture."),
			Settings.FlipEverySeconds, SHBench::MinFlipEverySeconds, Settings.CaptureSeconds);
		bValid = false;
	}
	if (!Settings.bConfigValid)
	{
		UE_LOG(LogSHBench, Error, TEXT("-SHConfig must be one of B0, B1, B2, B3, SH."));
		bValid = false;
	}
	else if (!SHRouter::IsImplemented(Settings.Config))
	{
		// Refused before anything is spawned, so a refused run leaves an empty world.
		UE_LOG(LogSHBench, Error, TEXT("-SHConfig=%s is not implemented yet."), SHDemo::GetConfigName(Settings.Config));
		bValid = false;
	}
	if (Settings.EventRate < 0.f || Settings.EventRate > 1.f)
	{
		UE_LOG(LogSHBench, Error, TEXT("-SHEventRate=%.4f must be a probability (0 to 1)."), Settings.EventRate);
		bValid = false;
	}
	if (!FApp::UseFixedTimeStep())
	{
		// Not an error: functional checks may run without it. Benchmarks always pass -deterministic (run_bench.py).
		UE_LOG(LogSHBench, Display, TEXT("No fixed time step: flips and events are repeatable frame by frame only with -deterministic -FPS=<n>."));
	}
	return bValid;
}

void USHBenchmarkSubsystem::BuildEntityConfig()
{
	// A transient config asset owned by this subsystem; its traits are created as its subobjects. The convenient
	// UMassEntityConfigAsset::AddTrait is editor-only (inside #if WITH_EDITOR, MassEntityConfigAsset.cpp:241-284), so the
	// traits are added through the runtime FMassEntityConfig::AddTrait (MassEntityConfigAsset.cpp:203-206).
	EntityConfig = NewObject<UMassEntityConfigAsset>(this, TEXT("SHDemoEntityConfig"), RF_Transient);
	FMassEntityConfig& Config = EntityConfig->GetMutableConfig();

	USHDemoStateTrait* DemoTrait = NewObject<USHDemoStateTrait>(EntityConfig);
	DemoTrait->bAddMirrorTag = Settings.Config == ESHConfig::B2; // B2's mirror processor matches only tagged entities
	Config.AddTrait(*DemoTrait);
	if (Settings.Config == ESHConfig::SH)
	{
		Config.AddTrait(*NewObject<USHStateHandoffTrait>(EntityConfig)); // the hand-off's fragment and tag
	}
	Config.AddTrait(*NewObject<UMassLODCollectorTrait>(EntityConfig));

	UMassMovableVisualizationTrait* Visualization = NewObject<UMassMovableVisualizationTrait>(EntityConfig);
	// One class for both resolutions: the same template index, so the engine keeps the actor (disabled) while the
	// entity shows as an instanced mesh: the kept-disabled path under test (01b #5).
	Visualization->HighResTemplateActor = ASHDemoAgent::StaticClass();
	Visualization->LowResTemplateActor = ASHDemoAgent::StaticClass();
	// -SHKeepLowRes=0 is the H2 variant: every Actor -> Mesh switch then releases the actor (01b #5).
	Visualization->Params.bKeepLowResActors = Settings.bKeepLowRes;
	if (Settings.Config == ESHConfig::B2)
	{
		// B2's push on spawn (05 E2). The engine allows replacing the class by default (MassRepresentationFragments.h:220).
		Visualization->Params.RepresentationActorManagementClass = USHB2ActorManagement::StaticClass();
	}
	if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
	{
		FMassStaticMeshInstanceVisualizationMeshDesc& MeshDesc = Visualization->StaticMeshInstanceDesc.Meshes.AddDefaulted_GetRef();
		MeshDesc.Mesh = Cube;
		MeshDesc.LocalTransform = FTransform(FQuat::Identity, FVector::ZeroVector, FVector(0.6));
	}
	else
	{
		UE_LOG(LogSHBench, Error, TEXT("Could not load /Engine/BasicShapes/Cube: the instanced-mesh representation will be missing."));
	}
	for (int32 LOD = 0; LOD < EMassLOD::Max; ++LOD)
	{
		Visualization->LODParams.BaseLODDistance[LOD] = SHBench::BaseLODDistance[LOD];
		Visualization->LODParams.VisibleLODDistance[LOD] = SHBench::VisibleLODDistance[LOD];
	}
	Visualization->LODParams.LODMaxCount[EMassLOD::High] = SHBench::LODMaxCountHigh;
	Visualization->LODParams.LODMaxCount[EMassLOD::Medium] = SHBench::LODMaxCountMedium;
	Visualization->LODParams.LODMaxCount[EMassLOD::Low] = SHBench::LODMaxCountLow;
	// Must equal USHVisualizationLODProcessor::FilterTag, or that processor skips these settings.
	Visualization->LODParams.FilterTag = FSHDemoAgentTag::StaticStruct();
	Config.AddTrait(*Visualization);
}

uint32 USHBenchmarkSubsystem::SpawnAgents(UWorld& World)
{
	UMassSpawnerSubsystem* Spawner = World.GetSubsystem<UMassSpawnerSubsystem>();
	UMassEntitySubsystem* EntitySubsystem = World.GetSubsystem<UMassEntitySubsystem>();
	if (!Spawner || !EntitySubsystem || !EntityConfig)
	{
		UE_LOG(LogSHBench, Error, TEXT("Mass spawner or entity subsystem missing: is the MassGameplay plugin enabled?"));
		return 0;
	}

	const FMassEntityTemplate& Template = EntityConfig->GetOrCreateEntityTemplate(World);
	// Keep the creation context alive while the fragments are filled in: the entities' creation observers run when it
	// is released, so they see the real transforms, not the defaults.
	TSharedPtr<FMassEntityManager::FEntityCreationContext> Creation = Spawner->SpawnEntities(Template, static_cast<uint32>(Settings.NumAgents), Agents);

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	FRandomStream Random(Settings.Seed);
	const int32 Side = FMath::CeilToInt32(FMath::Sqrt(static_cast<double>(Settings.NumAgents)));
	ArenaHalfExtent = 0.5 * SHBench::GridSpacing * (Side - 1);

	uint32 PositionHash = 0;
	for (int32 Index = 0; Index < Agents.Num(); ++Index)
	{
		const FVector Position(
			(Index % Side) * SHBench::GridSpacing - ArenaHalfExtent + Random.FRandRange(-SHBench::GridJitter, SHBench::GridJitter),
			(Index / Side) * SHBench::GridSpacing - ArenaHalfExtent + Random.FRandRange(-SHBench::GridJitter, SHBench::GridJitter),
			SHBench::AgentHeight);
		EntityManager.GetFragmentDataChecked<FTransformFragment>(Agents[Index]).SetTransform(FTransform(Position));
		EntityManager.GetFragmentDataChecked<FSHAgentIdFragment>(Agents[Index]).AgentIndex = Index;

		// Running CRC: chaining equals one CRC over all positions, without buffering them.
		PositionHash = FCrc::TypeCrc32(FVector3f(Position), PositionHash);
	}
	Creation.Reset();
	return PositionHash;
}

void USHBenchmarkSubsystem::UpdateCamera(UWorld& World, double SecondsFromBenchStart)
{
	if (!Camera)
	{
		return;
	}

	const double HotSpotX = (HotSpot == 0 ? -1.0 : 1.0) * SHBench::HotSpotFraction * ArenaHalfExtent;
	const FVector Center(HotSpotX, 0.0, SHBench::AgentHeight);
	// Parked: angle 0 for the whole run, so the LOD settles in the warm-up and nothing changes representation after it.
	const double Angle = Settings.bParked ? 0.0 : 2.0 * UE_DOUBLE_PI * SecondsFromBenchStart / SHBench::OrbitSeconds;
	const FVector Location(HotSpotX + SHBench::OrbitRadius * FMath::Cos(Angle), SHBench::OrbitRadius * FMath::Sin(Angle), SHBench::CameraHeight);
	Camera->SetActorLocationAndRotation(Location, (Center - Location).Rotation());

	// Re-applied every frame: possessing a pawn later would otherwise reset the view target. Mass's LOD subsystem reads
	// the viewer from the player controller's view point, so this camera is the LOD viewer (MassLODSubsystem.cpp:376).
	APlayerController* PlayerController = World.GetFirstPlayerController();
	if (PlayerController && PlayerController->GetViewTarget() != Camera)
	{
		PlayerController->SetViewTarget(Camera);
	}
}

void USHBenchmarkSubsystem::Teleport()
{
	HotSpot = 1 - HotSpot;
	++Teleports;
	CSV_EVENT(SHBench, TEXT("BenchFlip"));
}

void USHBenchmarkSubsystem::LogRepresentationCounts(const TCHAR* When) const
{
	const UMassEntitySubsystem* EntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem)
	{
		return;
	}
	const FMassEntityManager& EntityManager = EntitySubsystem->GetEntityManager();

	int32 Counts[static_cast<int32>(EMassRepresentationType::None) + 1] = {};
	int32 WithActor = 0;
	for (const FMassEntityHandle Agent : Agents)
	{
		if (const FMassRepresentationFragment* Representation = EntityManager.GetFragmentDataPtr<FMassRepresentationFragment>(Agent))
		{
			++Counts[static_cast<int32>(Representation->CurrentRepresentation)];
		}
		if (const FMassActorFragment* ActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Agent))
		{
			WithActor += ActorFragment->IsValid() ? 1 : 0;
		}
	}
	UE_LOG(LogSHBench, Display, TEXT("Representation at %s: high-res actor %d, low-res actor %d, skinned mesh %d, static mesh %d, none %d; entities holding an actor %d"),
		When,
		Counts[static_cast<int32>(EMassRepresentationType::HighResSpawnedActor)],
		Counts[static_cast<int32>(EMassRepresentationType::LowResSpawnedActor)],
		Counts[static_cast<int32>(EMassRepresentationType::SkinnedMeshInstance)],
		Counts[static_cast<int32>(EMassRepresentationType::StaticMeshInstance)],
		Counts[static_cast<int32>(EMassRepresentationType::None)],
		WithActor);
}

void USHBenchmarkSubsystem::StartRun(UWorld& World, const double Now)
{
	Settings = ReadRunSettings();
	if (Settings.NumAgents <= 0)
	{
		Phase = ERunPhase::Idle; // No workload requested: the world behaves like a normal empty map.
		return;
	}
	if (!ValidateRunSettings())
	{
		// Never produce a normal-looking run from bad settings: no workload, and a failing exit code with -SHExit.
		Phase = ERunPhase::Idle;
		if (Settings.bExitWhenDone)
		{
			// Forced: on Windows a normal exit doesn't keep the code (WindowsPlatformMisc.cpp:1493-1521; AGENT_BRIEF rule 12).
			FPlatformMisc::RequestExitWithStatus(true, 3, TEXT("USHBenchmarkSubsystem: invalid run settings"));
		}
		return;
	}

	if (Settings.Config == ESHConfig::SH)
	{
		// The four demo channels, by property name on the actor-side component (02 section 4.1).
		USHStateHandoffSubsystem* Handoff = World.GetSubsystem<USHStateHandoffSubsystem>();
		if (!Handoff || !Handoff->RegisterChannels(USHAgentStateComponent::StaticClass(), { TEXT("Health"), TEXT("Cooldown"), TEXT("Memory"), TEXT("Reservation") }))
		{
			Phase = ERunPhase::Idle;
			if (Settings.bExitWhenDone)
			{
				FPlatformMisc::RequestExitWithStatus(true, 3, TEXT("USHBenchmarkSubsystem: StateHandoff channels could not be registered"));
			}
			return;
		}
	}

	const double SpawnStart = FPlatformTime::Seconds();
	BuildEntityConfig();
	const uint32 PositionHash = SpawnAgents(World);
	UE_LOG(LogSHBench, Display, TEXT("Spawned %d Mass entities (seed %d) in %.1f ms; start-position hash 0x%08X"),
		Agents.Num(), Settings.Seed, (FPlatformTime::Seconds() - SpawnStart) * 1000.0, PositionHash);

	// The workload and the checker start with the entities. A configuration that isn't usable yet stops the run.
	USHWorkloadSubsystem::FRunConfig WorkloadRun;
	WorkloadRun.Config = Settings.Config;
	WorkloadRun.Seed = static_cast<uint32>(Settings.Seed);
	WorkloadRun.EventRate = Settings.EventRate;
	WorkloadRun.bCheck = Settings.bCheck;
	WorkloadRun.bPooling = Settings.bPooling;
	WorkloadRun.bKeepLowRes = Settings.bKeepLowRes;
	WorkloadRun.RunTag = Settings.RunTag;
	Workload = World.GetSubsystem<USHWorkloadSubsystem>();
	if (!Workload || !Workload->BeginRun(WorkloadRun, Agents.Num()))
	{
		Phase = ERunPhase::Idle;
		if (Settings.bExitWhenDone)
		{
			FPlatformMisc::RequestExitWithStatus(true, 3, TEXT("USHBenchmarkSubsystem: the workload could not start"));
		}
		return;
	}

	Camera = World.SpawnActor<ACameraActor>();
	FirstTickTime = Now;
	Phase = ERunPhase::WarmUp;

	// Run description inside the CSV itself, so every capture is self-describing.
	CSV_METADATA(TEXT("SHRunTag"), *Settings.RunTag);
	CSV_METADATA(TEXT("SHAgents"), *FString::FromInt(Settings.NumAgents));
	CSV_METADATA(TEXT("SHSeed"), *FString::FromInt(Settings.Seed));
	CSV_METADATA(TEXT("SHFlipEvery"), *FString::SanitizeFloat(Settings.FlipEverySeconds));
	CSV_METADATA(TEXT("SHWarmup"), *FString::SanitizeFloat(Settings.WarmupSeconds));
	CSV_METADATA(TEXT("SHCapture"), *FString::SanitizeFloat(Settings.CaptureSeconds));
	CSV_METADATA(TEXT("SHConfig"), SHDemo::GetConfigName(Settings.Config));
	CSV_METADATA(TEXT("SHEventRate"), *FString::SanitizeFloat(Settings.EventRate));
	CSV_METADATA(TEXT("SHCheck"), Settings.bCheck ? TEXT("1") : TEXT("0"));
	CSV_METADATA(TEXT("SHParked"), Settings.bParked ? TEXT("1") : TEXT("0"));
	CSV_METADATA(TEXT("SHPooling"), Settings.bPooling ? TEXT("1") : TEXT("0"));
	CSV_METADATA(TEXT("SHKeepLowRes"), Settings.bKeepLowRes ? TEXT("1") : TEXT("0"));
}

void USHBenchmarkSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UWorld& World = *GetWorld();
	const double Now = World.GetTimeSeconds();

	if (Phase == ERunPhase::Unread)
	{
		// First tick, after every subsystem's OnWorldBeginPlay: Mass's own subsystems are certainly ready now.
		StartRun(World, Now);
	}
	if (Phase == ERunPhase::Idle)
	{
		return;
	}

	const double FromBenchStart = (Now - FirstTickTime) - Settings.WarmupSeconds;
	switch (Phase)
	{
	case ERunPhase::WarmUp:
		if (FromBenchStart >= 0.0)
		{
			Phase = ERunPhase::Capture;
			CSV_EVENT(SHBench, TEXT("BenchStart"));
			Workload->RequestCapturing(true); // from the next frame, the first one the CSV holds
			UE_LOG(LogSHBench, Display, TEXT("BenchStart at %.3f s (warm-up %.1f s)"), Now, Settings.WarmupSeconds);
		}
		else if (FromBenchStart + DeltaTime >= 0.0)
		{
			// The last warm-up frame: never measured, so the start-of-capture counts cost nothing that is captured.
			LogRepresentationCounts(TEXT("end of warm-up"));
		}
		break;

	case ERunPhase::Capture:
	{
		if (FromBenchStart >= Settings.CaptureSeconds)
		{
			// Checked first: a flip due on this frame would have no measured window left.
			CSV_EVENT(SHBench, TEXT("BenchEnd"));
			Workload->RequestCapturing(false); // this frame still counts: the switch takes effect at the next frame's start
			Phase = ERunPhase::Done;
			FramesSinceEnd = 0;
			UE_LOG(LogSHBench, Display, TEXT("BenchEnd after %.3f s of capture; %d scheduled flips, %d camera jumps"), FromBenchStart, ScheduledFlips, Teleports);
			break;
		}
		// Flips only inside the capture, at FlipEvery, 2 x FlipEvery, ...: with a fixed time step, the same frames in
		// every run (run_bench.py always passes -deterministic -FPS=60).
		const int32 FlipIndex = Settings.FlipEverySeconds > 0.0 ? FMath::FloorToInt32(FromBenchStart / Settings.FlipEverySeconds) : 0;
		if (BounceFramesLeft > 0 && --BounceFramesLeft == 0)
		{
			Teleport(); // the bounce returns to the hot spot it just left
		}
		if (FlipIndex > ScheduledFlips)
		{
			++ScheduledFlips;
			if (Settings.bLogCounts)
			{
				// Functional checks only (-SHLogCounts): this walks every agent on a captured frame.
				LogRepresentationCounts(*FString::Printf(TEXT("flip %d (%.2f s)"), ScheduledFlips, FromBenchStart));
			}
			Teleport();
			if (ScheduledFlips % SHBench::BounceEvery == 0)
			{
				BounceFramesLeft = SHBench::BounceFrames;
			}
		}
		break;
	}

	case ERunPhase::Done:
		++FramesSinceEnd;
		if (FramesSinceEnd == 1)
		{
			// The frame after BenchEnd: outside the capture (-csvStopOnEvent=BenchEnd ends it with the BenchEnd frame).
			LogRepresentationCounts(TEXT("BenchEnd"));
			// Counting stopped at the start of this frame (RequestCapturing at BenchEnd), so every count is final.
			Workload->WriteReport();
		}
		if (Settings.bExitWhenDone && FramesSinceEnd >= SHBench::ExitDelayFrames)
		{
			Settings.bExitWhenDone = false; // Request once.
			FPlatformMisc::RequestExit(false, TEXT("USHBenchmarkSubsystem"));
		}
		break;

	default:
		break;
	}

	UpdateCamera(World, FromBenchStart);
}

TStatId USHBenchmarkSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USHBenchmarkSubsystem, STATGROUP_Tickables);
}
