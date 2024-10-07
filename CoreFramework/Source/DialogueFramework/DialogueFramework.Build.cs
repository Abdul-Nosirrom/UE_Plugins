using System.IO;
using UnrealBuildTool;

public class DialogueFramework : ModuleRules
{
    public DialogueFramework(ReadOnlyTargetRules Target) : base(Target)
    {
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        CppStandard = CppStandardVersion.Latest;
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core", "CoreFramework", "ActionFramework", 
                "UMG", "Projects", "GameplayTags", "LevelSequence", "InteractionFramework", "Flow"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "DeveloperSettings",
                "MovieScene",
                "MovieSceneTracks",
            }
        );
        
        // Public include directories.
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
		
        // Private includes directories.
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));
    }
}