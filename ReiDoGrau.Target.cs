using UnrealBuildTool;

public class ReiDoGrauTarget : TargetRules
{
	public ReiDoGrauTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("ReiDoGrau");
	}
}
