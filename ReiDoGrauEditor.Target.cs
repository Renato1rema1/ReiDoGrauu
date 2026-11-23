using UnrealBuildTool;

public class ReiDoGrauEditorTarget : TargetRules
{
	public ReiDoGrauEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("ReiDoGrau");
	}
}
