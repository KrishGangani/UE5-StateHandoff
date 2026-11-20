// StateHandoffCore - the reusable runtime module (ownership contract, commit-time hand-off, invariant checker).
// It must not depend on any game module.

using UnrealBuildTool;

public class StateHandoffCore : ModuleRules
{
	public StateHandoffCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Phase 4: only the engine basics. The Mass modules (MassEntity, MassCore, MassActors, MassRepresentation,
		// MassSpawner) are added in S2/S5, together with the first code that includes their headers.
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
	}
}
