using UnrealBuildTool;

public class ReiDoGrau : ModuleRules
{
	public ReiDoGrau(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "AudioMixer"
		});
	}
}
