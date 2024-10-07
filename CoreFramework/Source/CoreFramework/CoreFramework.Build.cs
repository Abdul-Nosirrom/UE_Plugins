// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.
using System;
using System.IO;
using UnrealBuildTool;

public class CoreFramework : ModuleRules
{
	public CoreFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		CppStandard = CppStandardVersion.Latest;
	    PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

	    PrivatePCHHeaderFile = "Private/CFW_PCH.h";

	    PublicDependencyModuleNames.AddRange(new[]
		    {
			    "Core", "CoreUObject", "Engine", "InputCore", "PhysicsCore", "AIModule", "EnhancedInput", "GameplayTags", "ImGui", "ImGuiWindow"
		    }
	    );

	    PrivateDependencyModuleNames.AddRange(new []
	    {
		    "DeveloperSettings"
	    });

	    // Public include directories.
	    PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
	    PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public/Actors"));
	    PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public/Components"));
	    PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public/DataAssets"));
	    PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public/DataStructures"));
	    PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public/Subsystems"));
	    
	    // Private includes directories.
	    PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));
	    PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Actors"));
	    PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Components"));
	    PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/DataAssets"));
	    PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/DataStructures"));
	    PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Debug"));
	    PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private/Subsystems"));
	    
	    // Specify Defs (For Logging)
	    PrivateDefinitions.Add("CFW_MOVEMENT_LOG");
	    PrivateDefinitions.Add("CFW_BUFFER_LOG");
	}
}
