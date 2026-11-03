using UnrealBuildTool;
using System.Collections.Generic;
public class AshenRiftTarget : TargetRules
{
    public AshenRiftTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V2;
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            WindowsPlatform.Compiler = WindowsCompiler.VisualStudio2019;
            WindowsPlatform.WindowsSdkVersion = "10.0.19041.0";
        }
        ExtraModuleNames.Add("AshenRift");
    }
}
