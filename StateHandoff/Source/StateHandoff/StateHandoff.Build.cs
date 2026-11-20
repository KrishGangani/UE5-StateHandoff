// P09 StateHandoff - primary game module: demo workload, baselines B0-B3 and benchmark harness only.
// The reusable system lives in the StateHandoffCore plugin; this module depends on it, never the reverse.

using UnrealBuildTool;

public class StateHandoff : ModuleRules
{
	public StateHandoff(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Phase 4: only the engine basics. Mass and StateHandoffCore are added in S1, when the first code uses them.
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
	}
}
