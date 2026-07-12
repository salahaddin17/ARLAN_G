using UnrealBuildTool;
using System.Collections.Generic;

public class RubezhArlanEditorTarget : TargetRules
{
	public RubezhArlanEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		// Latest — не привязываемся к минорной версии движка (5.8+)
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("RubezhArlan");
	}
}
