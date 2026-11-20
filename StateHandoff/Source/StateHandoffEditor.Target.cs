// P09 StateHandoff - editor target.

using UnrealBuildTool;

public class StateHandoffEditorTarget : TargetRules
{
	public StateHandoffEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("StateHandoff");
	}
}
