// Copyright (c) 2024 EngineWire Contributors

using UnrealBuildTool;

/**
 * Game feature plugins for the system_control game feature records, behind IMcpGameFeaturesProvider (EngineWire's
 * Public/McpGameFeaturesProvider.h). The Game Features subsystem has no reflected functions to reach, so its C++ is
 * linked here, in a module of its own, as EngineWireFab links Fab. A project may leave the GameFeatures plugin off, so
 * the .uplugin never loads this module by itself (LoadingPhase None) and names GameFeatures as an optional dependency
 * that it does not turn on: EngineWire's StartupModule loads this module only where the GameFeatures module exists.
 */
public class EngineWireGameFeatures : ModuleRules
{
    public EngineWireGameFeatures(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "Json", "Projects", "AssetTools", "GameFeatures", "EngineWire"
        });
    }
}
