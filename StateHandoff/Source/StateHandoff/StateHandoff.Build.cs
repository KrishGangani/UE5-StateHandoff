// P09 StateHandoff - primary game module: demo workload, baselines B0-B3 and benchmark harness only.
// The reusable system lives in the StateHandoffCore plugin; this module depends on it, never the reverse.

using UnrealBuildTool;

public class StateHandoff : ModuleRules
{
	public StateHandoff(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Public: the module's public headers derive from or include these modules' types
		// (fragments and tags: MassEntity; entity handles: MassCore; traits: MassSpawner; processors: MassLOD, MassRepresentation).
		// StateHandoffCore too: SHDemoState.h (a public header) derives from the plugin's FSHChannelFragment.
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine",
			"MassEntity", "MassCore", "MassSpawner", "MassLOD", "MassRepresentation", "StateHandoffCore" });

		// Private: only .cpp files use them (FMassActorFragment).
		PrivateDependencyModuleNames.AddRange(new string[] { "MassActors" });
	}
}
