// StateHandoffCore - the reusable runtime module (ownership contract, commit-time hand-off, invariant checker).
// It must not depend on any game module.

using UnrealBuildTool;

public class StateHandoffCore : ModuleRules
{
	public StateHandoffCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Public: the public headers derive from MassCore's FMassFragment, MassEntity's UMassProcessor and MassSpawner's
		// UMassEntityTraitBase (SHContractTypes.h, SHStateHandoff.h).
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "MassCore", "MassEntity", "MassSpawner" });

		// Private: only SHStateHandoff.cpp uses them (FMassActorFragment; FMassRepresentationFragment and the group names).
		PrivateDependencyModuleNames.AddRange(new string[] { "MassActors", "MassRepresentation" });
	}
}
