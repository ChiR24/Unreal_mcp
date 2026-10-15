#include "EngineWireSettings.h"

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "ToolMenus.h"
#include "UI/SMcpStatusBarWidget.h"

// Save current LOCTEXT_NAMESPACE if defined, then set our own
#pragma push_macro("LOCTEXT_NAMESPACE")
#undef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "FEngineWireModule"

DEFINE_LOG_CATEGORY_STATIC(LogEngineWire, Log, All);

class FEngineWireModule final : public IModuleInterface
{
public:
    /**
     * @brief Initializes the EngineWire module.
     *
     * Performs module startup tasks required by the plugin. In editor builds, it records that
     * UEngineWireSettings are exposed via the Project Settings UI.
     */
    virtual void StartupModule() override
    {
        UE_LOG(LogEngineWire, Log, TEXT("EngineWire module initialized."));

        // UDeveloperSettings (UEngineWireSettings) are auto-registered with the
        // Project Settings UI. Do not manually register them via ISettingsModule as this
        // produces duplicate entries in Project Settings. The settings class saves
        // automatically in PostEditChangeProperty.
        UE_LOG(LogEngineWire, Verbose, TEXT("UEngineWireSettings are exposed via Project Settings (auto-registered)."));

        UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(
            this, &FEngineWireModule::RegisterStatusBarWidget));
    }

    /**
     * @brief Shuts down the EngineWire module.
     *
     * Logs a shutdown message. In editor builds the function does not attempt to unregister project
     * settings because UDeveloperSettings instances are managed by the engine.
     */
    virtual void ShutdownModule() override
    {
        UE_LOG(LogEngineWire, Log, TEXT("EngineWire module shut down."));

        UToolMenus::UnRegisterStartupCallback(this);
        UToolMenus::UnregisterOwner(this);
    }

private:
    void RegisterStatusBarWidget()
    {
        FToolMenuOwnerScoped OwnerScoped(this);
        UToolMenu* StatusBar = UToolMenus::Get()->ExtendMenu("LevelEditor.StatusBar.ToolBar");
        if (!StatusBar) return;

        FToolMenuSection& Section = StatusBar->AddSection(
            "McpStatus", FText::GetEmpty(),
            FToolMenuInsert("SourceControl", EToolMenuInsertType::Before));

        Section.AddEntry(FToolMenuEntry::InitWidget(
            "McpStatusWidget",
            SNew(SMcpStatusBarWidget),
            FText::GetEmpty(),
            true, false));
    }

};

// Restore the previous LOCTEXT_NAMESPACE
#pragma pop_macro("LOCTEXT_NAMESPACE")

IMPLEMENT_MODULE(FEngineWireModule, EngineWire)
