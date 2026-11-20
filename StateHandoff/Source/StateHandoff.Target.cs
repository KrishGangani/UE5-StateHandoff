// P09 StateHandoff - game target (standalone/packaged builds).

using UnrealBuildTool;

public class StateHandoffTarget : TargetRules
{
	public StateHandoffTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("StateHandoff");
	}
}
