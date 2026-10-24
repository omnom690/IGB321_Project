using UnrealBuildTool;

public class IGB321_ProjectEditorTarget : TargetRules
{
    public IGB321_ProjectEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("IGB321_Project");
    }
}
