using UnrealBuildTool;

public class ProjectRiftSmokeTests : ModuleRules
{
	public ProjectRiftSmokeTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "Gauntlet", "ProjectA" });
	}
}
