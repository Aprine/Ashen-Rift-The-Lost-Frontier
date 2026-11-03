using UnrealBuildTool;
public class AshenRift : ModuleRules
{
    public AshenRift(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "NavigationSystem", "UMG", "Slate", "SlateCore", "AudioMixer" });
        if (Target.bBuildEditor) PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "AnimGraph", "BlueprintGraph", "KismetCompiler", "AssetRegistry", "AssetTools" });
    }
}
