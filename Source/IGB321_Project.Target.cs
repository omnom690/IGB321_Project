using UnrealBuildTool;

public class IGB321_ProjectTarget : TargetRules
{
    public IGB321_ProjectTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("IGB321_Project");
    }
}
