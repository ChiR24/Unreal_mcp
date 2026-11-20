using System.IO;
using UnrealBuildTool;

// A DeveloperTool module: it builds into editor, Development and Test targets and never into Shipping. It links no
// editor module and nothing of EngineWire, so a game build carries it alone. What differs between engines is read
// from the engine's own headers, never from a version number (the same markers EngineWire's FeatureDetection.json uses).
public class EngineWireRuntime : ModuleRules
{
    public EngineWireRuntime(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "InputCore", "ApplicationCore", "Json", "Sockets", "Networking", "RenderCore", "RHI"
        });
        string Engine = Path.GetFullPath(Target.RelativeEnginePath);
        PrivateDefinitions.Add("MCP_RT_INPUT_ARGS_SIMULATED=" + (Has(Engine, "Source/Runtime/Engine/Public/InputKeyEventArgs.h", "CreateSimulated") ? "1" : "0"));
        PrivateDefinitions.Add("MCP_RT_INPUT_DEVICE_MAPPER=" + (Has(Engine, "Source/Runtime/ApplicationCore/Public/GenericPlatform/GenericPlatformInputDeviceMapper.h", null) ? "1" : "0"));
    }

    private static bool Has(string Engine, string Relative, string Marker)
    {
        string File = Path.Combine(Engine, Relative);
        return System.IO.File.Exists(File) && (Marker == null || System.IO.File.ReadAllText(File).Contains(Marker));
    }
}
