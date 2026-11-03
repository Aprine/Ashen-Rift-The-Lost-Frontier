using UnrealBuildTool;
using System.Collections.Generic;
public class AshenRiftEditorTarget : TargetRules
{
    public AshenRiftEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V2;
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            WindowsPlatform.Compiler = WindowsCompiler.VisualStudio2019;
            WindowsPlatform.WindowsSdkVersion = "10.0.19041.0";
        }
        ExtraModuleNames.Add("AshenRift");
    }
}
