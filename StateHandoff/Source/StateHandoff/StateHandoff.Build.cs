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
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine",
			"MassEntity", "MassCore", "MassSpawner", "MassLOD", "MassRepresentation" });

		// Private: only .cpp files use them (FMassActorFragment). StateHandoffCore is added in S2 with the checker.
		PrivateDependencyModuleNames.AddRange(new string[] { "MassActors" });
	}
}
