// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.
using System;
using System.IO;
using UnrealBuildTool;

public class ActionFramework : ModuleRules
{
	public ActionFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Latest;

		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivatePCHHeaderFile = "Private/AF_PCH.h";

		PublicDependencyModuleNames.AddRange(new[]
			{
				"Core", "CoreUObject", "Engine", "InputCore", "PhysicsCore", "AIModule", "EnhancedInput", "GameplayTags",
				"Projects", "UMG"
			}
		);
		
		PrivateDependencyModuleNames.AddRange(new []
		{
			"CoreFramework", "DeveloperSettings", "Niagara"
		}
		);

		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"UnrealEd",
				"ComponentVisualizers",
				"DataValidation"
			});
		}
		
		// Public include directories.
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public/ActionTemplates"));
		
		// Private includes directories.
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));
	}
}