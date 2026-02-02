#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "McpAutomationBridgeSettings.h"

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// Save current LOCTEXT_NAMESPACE if defined, then set our own
#pragma push_macro("LOCTEXT_NAMESPACE")
#undef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "FMcpAutomationBridgeModule"

DEFINE_LOG_CATEGORY_STATIC(LogMcpAutomationBridge, Log, All);

class FMcpAutomationBridgeModule final : public IModuleInterface
{
public:
    /**
     * @brief Initializes the MCP Automation Bridge module.
     *
     * Performs module startup tasks required by the plugin. In editor builds, it records that
     * UMcpAutomationBridgeSettings are exposed via the Project Settings UI.
     */
    virtual void StartupModule() override
    {
        UE_LOG(LogMcpAutomationBridge, Log, TEXT("MCP Automation Bridge module initialized."));

#if WITH_EDITOR
        // UDeveloperSettings (UMcpAutomationBridgeSettings) are auto-registered with the
        // Project Settings UI. Do not manually register them via ISettingsModule as this
        // produces duplicate entries in Project Settings. The settings class saves
        // automatically in PostEditChangeProperty.
        UE_LOG(LogMcpAutomationBridge, Verbose, TEXT("UMcpAutomationBridgeSettings are exposed via Project Settings (auto-registered)."));
#endif
    }

    /**
     * @brief Shuts down the MCP Automation Bridge module.
     *
     * Logs a shutdown message. In editor builds the function does not attempt to unregister project
     * settings because UDeveloperSettings instances are managed by the engine.
     */
    virtual void ShutdownModule() override
    {
        UE_LOG(LogMcpAutomationBridge, Log, TEXT("MCP Automation Bridge module shut down."));

#if WITH_EDITOR
        // No explicit unregister needed because we did not register the settings
        // manually. UDeveloperSettings instances are managed by the engine.
#endif
    }

    /**
     * @brief Persists UMcpAutomationBridgeSettings to DefaultGame.ini when project settings are modified.
     *
     * Saves the mutable default UMcpAutomationBridgeSettings to disk and logs the save action if the settings object is available.
     *
     * @return `true` if the settings object was found and saved, `false` otherwise.
     */
    bool HandleSettingsModified()
    {
        if (UMcpAutomationBridgeSettings* Settings = GetMutableDefault<UMcpAutomationBridgeSettings>())
        {
            Settings->SaveConfig();
            UE_LOG(LogMcpAutomationBridge, Log, TEXT("MCP Automation Bridge settings saved to DefaultGame.ini"));
            return true;
        }
        return false;
    }

private:
    // Hold the registered settings section so we can unbind and unregister it cleanly
    TSharedPtr<class ISettingsSection> SettingsSection;
};

// Restore the previous LOCTEXT_NAMESPACE
#pragma pop_macro("LOCTEXT_NAMESPACE")

IMPLEMENT_MODULE(FMcpAutomationBridgeModule, McpAutomationBridge)