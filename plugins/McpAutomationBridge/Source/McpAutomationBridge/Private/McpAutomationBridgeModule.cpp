#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "McpAutomationBridgeSettings.h"

#define LOCTEXT_NAMESPACE "FMcpAutomationBridgeModule"
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpAutomationBridge, Log, All);

class FMcpAutomationBridgeModule final : public IModuleInterface
{
public:
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

    virtual void ShutdownModule() override
    {
        UE_LOG(LogMcpAutomationBridge, Log, TEXT("MCP Automation Bridge module shut down."));

#if WITH_EDITOR
        // No explicit unregister needed because we did not register the settings
        // manually. UDeveloperSettings instances are managed by the engine.
#endif
    }

    // Called when project settings are modified via Project Settings UI
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

IMPLEMENT_MODULE(FMcpAutomationBridgeModule, McpAutomationBridge)
