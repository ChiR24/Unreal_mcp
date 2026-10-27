// Copyright (c) 2024 EngineWire Contributors

using UnrealBuildTool;

/**
 * lint_blueprint's rules as a Data Validation validator, so they also run in the editor's Validate Assets, on save and
 * in validation commandlets, not only over MCP.
 *
 * A module of its own: a validator is a UCLASS deriving from UEditorValidatorBase, so it needs the DataValidation
 * module at compile time (UHT cannot gate a UCLASS on the optional-module define the core module uses), and the core
 * module keeps DataValidation delay-loaded. A project may turn that plugin off, so the .uplugin never loads this
 * module by itself (LoadingPhase None): EngineWire's StartupModule loads it where DataValidation's module exists.
 */
public class EngineWireValidation : ModuleRules
{
    public EngineWireValidation(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new string[] {
            "Core", "CoreUObject", "Engine", "DataValidation", "EngineWire"
        });
    }
}
