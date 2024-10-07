using UnrealBuildTool;

public class EditorUtilities : ModuleRules
{
    public EditorUtilities(ReadOnlyTargetRules Target) : base(Target)
    {
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        CppStandard = CppStandardVersion.Latest;
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core", "CoreFramework", "ActionFramework", "DialogueFramework", "CombatFramework", "InteractionFramework", "Engine"
            }
        );

        if (Target.Type == TargetType.Editor)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd",
                "ComponentVisualizers"
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
                "GameplayTagsEditor",

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
                "MainFrame", "DataTableEditor"
            }
        );
    }
}