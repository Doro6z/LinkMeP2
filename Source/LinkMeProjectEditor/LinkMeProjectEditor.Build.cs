// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LinkMeProjectEditor : ModuleRules
{
	public LinkMeProjectEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Slate",
			"SlateCore",
			"UnrealEd",
			"EditorFramework",
			"ToolMenus",
			"LinkMeProject" // Dependency on runtime module
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AssetRegistry",
			"EditorStyle",
			"EngineSettings",
			"LevelEditor",
			"Projects"
		});
	}
}
