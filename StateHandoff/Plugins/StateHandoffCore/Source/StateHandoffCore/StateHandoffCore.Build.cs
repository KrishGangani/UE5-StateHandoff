// StateHandoffCore - the reusable runtime module (ownership contract, commit-time hand-off, invariant checker).
// It must not depend on any game module.

using UnrealBuildTool;

public class StateHandoffCore : ModuleRules
{
	public StateHandoffCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Public: SHContractTypes.h (a public header) derives from MassCore's FMassFragment. The other Mass modules
		// (MassEntity, MassSpawner, MassActors, MassRepresentation) are added in S4-S6, with the first code that uses them.
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "MassCore" });
	}
}
