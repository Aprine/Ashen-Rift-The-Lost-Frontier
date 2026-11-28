using UnrealBuildTool;
public class AshenRift : ModuleRules
{
    public AshenRift(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // UE4's unity batches can merge file-local gameplay and automation symbols.
        bUseUnity = false;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "NavigationSystem", "UMG", "Slate", "SlateCore", "AudioMixer" });
        if (Target.bBuildEditor) PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "AnimGraph", "BlueprintGraph", "KismetCompiler", "AssetRegistry", "AssetTools", "RenderCore" });
    }
}
