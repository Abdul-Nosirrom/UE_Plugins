using UnrealBuildTool;

public class EditorUtilities : ModuleRules
{
    public EditorUtilities(ReadOnlyTargetRules Target) : base(Target)
    {
        DefaultBuildSettings = BuildSettingsVersion.V2;
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core", "CoreFramework", "ActionFramework", "Engine"
            }
        );

        if (Target.Type == TargetType.Editor)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd"
            });
        }

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreFramework",
                "Engine",
                "CoreUObject",
                "Projects",
                //"UnrealEd",
                "InputCore",
                "SlateCore",
                "Slate",
                "EditorStyle",
                "EditorWidgets",
                "ToolMenus",
                "ToolWidgets",
                "AssetTools",
                "GameplayTags",

                "WorkspaceMenuStructure",
                "DetailCustomizations",
                "PropertyEditor",

                "BlueprintGraph",
                "Kismet",
                "KismetCompiler",
                "KismetWidgets",

                "GraphEditor",
                "ContentBrowser",

                "ApplicationCore",
                "AppFramework",
                "MainFrame"
            }
        );
    }
}