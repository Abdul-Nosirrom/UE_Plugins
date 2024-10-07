using System.IO;
using UnrealBuildTool;

public class InteractionFramework : ModuleRules
{
    public InteractionFramework(ReadOnlyTargetRules Target) : base(Target)
    {
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        CppStandard = CppStandardVersion.Latest;
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core", "Engine"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "CoreFramework", "ActionFramework", 
                "UMG", "DeveloperSettings", "EnhancedInput", "Niagara",
                "CombatFramework", "GameplayTags", "Projects", "AIModule", "LevelSequence", "MovieScene"
            }
        );
        
        // Public include directories.
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
		
        // Private includes directories.
        PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));

        if (Target.Type == TargetType.Editor)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "DataValidation"
            });
        }
    }
}