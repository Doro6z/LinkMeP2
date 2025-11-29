// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class LinkMeProject : ModuleRules
{
        public LinkMeProject(ReadOnlyTargetRules Target) : base(Target)
        {
                PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

                PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "PhysicsCore", "ProceduralMeshComponent" });

                // Expose the module root so subfolders like Rdm can include headers without extra relative paths.
                PublicIncludePaths.AddRange(new string[] { ModuleDirectory });
                PrivateIncludePaths.AddRange(new string[] { Path.Combine(ModuleDirectory, "Rdm") });

                PrivateDependencyModuleNames.AddRange(new string[] {  });

                // Uncomment if you are using Slate UI
                // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

                // Uncomment if you are using online features
                // PrivateDependencyModuleNames.Add("OnlineSubsystem");

                // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
        }
}
