using UnrealBuildTool;

public class CombatFramework : ModuleRules
{
    public CombatFramework(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core", "Niagara", "CoreFramework", "GameplayTags"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "CoreFramework",
                "ActionFramework",
                "DeveloperSettings"
            }
        );

        if (Target.Type == TargetType.Editor)
        {
            PublicDependencyModuleNames.AddRange(new string[]
            {
                "GameplayTagsEditor"
            });
        }
    }
}