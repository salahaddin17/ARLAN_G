using UnrealBuildTool;

public class RubezhArlan : ModuleRules
{
	public RubezhArlan(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"NavigationSystem",
			"AIModule",
			"GameplayTasks",
		});

		// Позволяет включать заголовки от корня модуля: #include "Core/RTSGameMode.h"
		PublicIncludePaths.Add(ModuleDirectory);
	}
}
