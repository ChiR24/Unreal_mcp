#include "UI/SUnrealAgentPanel.h"

#include "CoreMinimal.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Docking/TabManager.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FUnrealAgentModule"

namespace
{
    const FName UnrealAgentTabName(TEXT("UnrealAgent"));
}

class FUnrealAgentModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
        LevelEditorModule.OnRegisterTabs().AddRaw(this, &FUnrealAgentModule::RegisterLevelEditorTabSpawner);
        RegisterLevelEditorTabSpawner(LevelEditorModule.GetLevelEditorTabManager());

        UToolMenus::RegisterStartupCallback(
            FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FUnrealAgentModule::RegisterMenus));
    }

    virtual void ShutdownModule() override
    {
        if (UObjectInitialized())
        {
            UToolMenus::UnRegisterStartupCallback(this);
            UToolMenus::UnregisterOwner(this);
        }

        if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor")))
        {
            LevelEditorModule->OnRegisterTabs().RemoveAll(this);

            if (TSharedPtr<FTabManager> TabManager = LevelEditorModule->GetLevelEditorTabManager())
            {
                TabManager->UnregisterTabSpawner(UnrealAgentTabName);
            }
        }
    }

private:
    void RegisterLevelEditorTabSpawner(TSharedPtr<FTabManager> InTabManager) const
    {
        if (!InTabManager.IsValid() || InTabManager->HasTabSpawner(UnrealAgentTabName))
        {
            return;
        }

        InTabManager->RegisterTabSpawner(
            UnrealAgentTabName,
            FOnSpawnTab::CreateRaw(this, &FUnrealAgentModule::SpawnUnrealAgentTab))
            .SetDisplayName(LOCTEXT("TabTitle", "Unreal Agent"))
            .SetTooltipText(LOCTEXT("TabTooltip", "Open the Unreal Agent panel"))
            .SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.StatsViewer"))
            .SetMenuType(ETabSpawnerMenuType::Hidden);
    }

    TSharedRef<SDockTab> SpawnUnrealAgentTab(const FSpawnTabArgs&) const
    {
        return SNew(SDockTab)
            .TabRole(ETabRole::PanelTab)
            .Label(LOCTEXT("TabTitle", "Unreal Agent"))
            [
                SNew(SUnrealAgentPanel)
            ];
    }

    void RegisterMenus()
    {
        FToolMenuOwnerScoped OwnerScoped(this);

        if (UToolMenu* WindowMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window"))
        {
            FToolMenuSection& Section = WindowMenu->FindOrAddSection("WindowLayout");
            Section.AddEntry(FToolMenuEntry::InitMenuEntry(
                "UnrealAgentOpenWindow",
                LOCTEXT("OpenUnrealAgentTab", "Unreal Agent"),
                LOCTEXT("OpenUnrealAgentTabTooltip", "Open the Unreal Agent panel"),
                FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.StatsViewer"),
                FUIAction(FExecuteAction::CreateRaw(this, &FUnrealAgentModule::OpenUnrealAgentTab))));
        }
    }

    void OpenUnrealAgentTab()
    {
        if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor")))
        {
            RegisterLevelEditorTabSpawner(LevelEditorModule->GetLevelEditorTabManager());
            if (TSharedPtr<FTabManager> TabManager = LevelEditorModule->GetLevelEditorTabManager())
            {
                TabManager->TryInvokeTab(UnrealAgentTabName);
            }
        }
    }
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealAgentModule, UnrealAgent)
