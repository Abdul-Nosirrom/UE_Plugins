// Copyright 2023 Abdulrahmen Almodaimegh. All Rights Reserved.
using System;
using System.IO;
using UnrealBuildTool;

public class ActionFramework : ModuleRules
{
	public ActionFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp17;

		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivatePCHHeaderFile = "Private/AF_PCH.h";

		PublicDependencyModuleNames.AddRange(new[]
			{
				"Core", "CoreUObject", "Engine", "InputCore", "PhysicsCore", "AIModule", "EnhancedInput", "GameplayTags",
				"Projects"
			}
		);
		
		PrivateDependencyModuleNames.AddRange(new []
		{
			"CoreFramework", "SMSystem", "CameraFramework"
		}
		);
		
		// Public include directories.
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public/StateMachineTemplates"));
		
		// Private includes directories.
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));
	}
}