#if WITH_DEV_AUTOMATION_TESTS

#include "UnrealAgentPanelAutomation.h"

#include "UnrealAgentAutomationWidgetHelpers.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/FileManager.h"
#include "LevelEditor.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UI/SUnrealAgentPanel.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

namespace UnrealAgent::AutomationTests
{
    FPanelTestFixture::FPanelTestFixture()
        : ChatHistoryPath(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentAutomation"), TEXT("PanelOpensChatHistory.json")))
    {
        IFileManager::Get().Delete(*ChatHistoryPath);
        SUnrealAgentPanel::SetChatHistoryStoragePathOverrideForAutomation(ChatHistoryPath);
    }

    FPanelTestFixture::~FPanelTestFixture()
    {
        Cleanup();
    }

    bool FPanelTestFixture::Open(FAutomationTestBase& Test)
    {
        FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
        TSharedPtr<FTabManager> LevelEditorTabManager = LevelEditorModule.GetLevelEditorTabManager();
        if (!Test.TestTrue(TEXT("Level Editor tab manager is available"), LevelEditorTabManager.IsValid()) || !LevelEditorTabManager.IsValid())
        {
            return false;
        }

        Tab = LevelEditorTabManager->TryInvokeTab(FName(TEXT("UnrealAgent")));
        if (!Test.TestTrue(TEXT("Unreal Agent ACP panel opens"), Tab.IsValid()) || !Tab.IsValid())
        {
            return false;
        }

        PanelContent = Tab->GetContent();
        Panel = StaticCastSharedPtr<SUnrealAgentPanel>(PanelContent);
        Panel->ResetChatHistoryForAutomation();
        return true;
    }

    void FPanelTestFixture::Cleanup()
    {
        if (Panel.IsValid())
        {
            Panel->ResetChatHistoryForAutomation();
        }
        Panel.Reset();
        PanelContent.Reset();
        Tab.Reset();
        SUnrealAgentPanel::ClearChatHistoryStoragePathOverrideForAutomation();
        IFileManager::Get().Delete(*ChatHistoryPath);
        IFileManager::Get().DeleteDirectory(*FPaths::GetPath(ChatHistoryPath), false, true);
    }

    TSharedRef<SWidget> FPanelTestFixture::Content() const
    {
        return PanelContent.ToSharedRef();
    }

    TSharedRef<SUnrealAgentPanel> FPanelTestFixture::PanelRef() const
    {
        return Panel.ToSharedRef();
    }

    TSharedPtr<SWidget> FindHistoryContainerByTitle(const TSharedRef<SWidget>& RootWidget, const FString& ExpectedTitle)
    {
        TArray<TSharedPtr<SWidget>> HistoryRowContainers;
        FindWidgetsByTag(RootWidget, FName(TEXT("UnrealAgent.Sidebar.History.RowContainer")), HistoryRowContainers);
        for (const TSharedPtr<SWidget>& HistoryRowContainer : HistoryRowContainers)
        {
            if (!HistoryRowContainer.IsValid())
            {
                continue;
            }

            TSharedPtr<SWidget> TitleWidget = FindWidgetByTag(HistoryRowContainer.ToSharedRef(), FName(TEXT("UnrealAgent.Sidebar.History.ActiveTitle")));
            if (!TitleWidget.IsValid())
            {
                TitleWidget = FindWidgetByTag(HistoryRowContainer.ToSharedRef(), FName(TEXT("UnrealAgent.Sidebar.History.Title")));
            }

            const TSharedPtr<STextBlock> TitleText = TitleWidget.IsValid()
                ? StaticCastSharedPtr<STextBlock>(TitleWidget)
                : nullptr;
            if (TitleText.IsValid() && TitleText->GetText().ToString() == ExpectedTitle)
            {
                return HistoryRowContainer;
            }
        }

        return nullptr;
    }

    TSharedPtr<SButton> FindHistoryButtonByTitle(const TSharedRef<SWidget>& RootWidget, const FString& ExpectedTitle)
    {
        const TSharedPtr<SWidget> HistoryRowContainer = FindHistoryContainerByTitle(RootWidget, ExpectedTitle);
        if (!HistoryRowContainer.IsValid())
        {
            return nullptr;
        }

        const TSharedPtr<SWidget> HistoryButton = FindWidgetByTag(HistoryRowContainer.ToSharedRef(), FName(TEXT("UnrealAgent.Sidebar.History.Row")));
        return HistoryButton.IsValid() ? StaticCastSharedPtr<SButton>(HistoryButton) : nullptr;
    }
}

#endif
