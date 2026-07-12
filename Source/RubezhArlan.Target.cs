using UnrealBuildTool;
using System.Collections.Generic;

public class RubezhArlanTarget : TargetRules
{
	public RubezhArlanTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		// Latest — не привязываемся к минорной версии движка (5.8+)
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("RubezhArlan");
	}
}
