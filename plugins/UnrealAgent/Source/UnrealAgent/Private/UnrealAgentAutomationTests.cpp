#if WITH_DEV_AUTOMATION_TESTS

#include "Acp/UnrealAgentEditorContext.h"
#include "Acp/UnrealAgentEvidenceLedger.h"
#include "Acp/UnrealAgentStudioKit.h"
#include "Acp/UnrealAgentValidationRunner.h"
#include "Acp/McpOpenCodeAcpClient.h"
#include "CoreMinimal.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Layout/Children.h"
#include "LevelEditor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Templates/Function.h"
#include "UI/SUnrealAgentPanel.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Text/STextBlock.h"

#if PLATFORM_LINUX
#include <signal.h>
#endif

namespace
{
    constexpr const TCHAR* TestAutomationBridgeSettingsSection = TEXT("/Script/McpAutomationBridge.McpAutomationBridgeSettings");

    class FScopedIgnoredSigPwrForTest
    {
    public:
        FScopedIgnoredSigPwrForTest()
        {
#if PLATFORM_LINUX && defined(SIGPWR)
            if (sigaction(SIGPWR, nullptr, &PreviousSigPwrAction) == 0)
            {
                struct sigaction IgnoreAction;
                FMemory::Memzero(&IgnoreAction, sizeof(IgnoreAction));
                IgnoreAction.sa_handler = SIG_IGN;
                sigemptyset(&IgnoreAction.sa_mask);
                bRestoreSigPwr = sigaction(SIGPWR, &IgnoreAction, nullptr) == 0;
            }
#endif
        }

        ~FScopedIgnoredSigPwrForTest()
        {
#if PLATFORM_LINUX && defined(SIGPWR)
            if (bRestoreSigPwr)
            {
                sigaction(SIGPWR, &PreviousSigPwrAction, nullptr);
            }
#endif
        }

    private:
#if PLATFORM_LINUX && defined(SIGPWR)
        struct sigaction PreviousSigPwrAction;
        bool bRestoreSigPwr = false;
#endif
    };

    TSharedPtr<SWidget> FindWidgetByTag(const TSharedRef<SWidget>& RootWidget, const FName& Tag)
    {
        if (RootWidget->GetTag() == Tag)
        {
            return RootWidget;
        }

        FChildren* Children = RootWidget->GetAllChildren();
        if (Children == nullptr)
        {
            return nullptr;
        }

        for (int32 ChildIndex = 0; ChildIndex < Children->Num(); ++ChildIndex)
        {
            TSharedPtr<SWidget> Match = FindWidgetByTag(Children->GetChildAt(ChildIndex), Tag);
            if (Match.IsValid())
            {
                return Match;
            }
        }

        return nullptr;
    }

    void FindWidgetsByTag(const TSharedRef<SWidget>& RootWidget, const FName& Tag, TArray<TSharedPtr<SWidget>>& OutWidgets)
    {
        if (RootWidget->GetTag() == Tag)
        {
            OutWidgets.Add(RootWidget);
        }

        FChildren* Children = RootWidget->GetAllChildren();
        if (Children == nullptr)
        {
            return;
        }

        for (int32 ChildIndex = 0; ChildIndex < Children->Num(); ++ChildIndex)
        {
            FindWidgetsByTag(Children->GetChildAt(ChildIndex), Tag, OutWidgets);
        }
    }

    bool FindWidgetTraversalIndexByTag(const TSharedRef<SWidget>& RootWidget, const FName& Tag, int32& OutIndex, int32& Cursor)
    {
        const int32 CurrentIndex = Cursor++;
        if (RootWidget->GetTag() == Tag)
        {
            OutIndex = CurrentIndex;
            return true;
        }

        FChildren* Children = RootWidget->GetAllChildren();
        if (Children == nullptr)
        {
            return false;
        }

        for (int32 ChildIndex = 0; ChildIndex < Children->Num(); ++ChildIndex)
        {
            if (FindWidgetTraversalIndexByTag(Children->GetChildAt(ChildIndex), Tag, OutIndex, Cursor))
            {
                return true;
            }
        }

        return false;
    }

    bool FindWidgetTraversalIndexByTag(const TSharedRef<SWidget>& RootWidget, const FName& Tag, int32& OutIndex)
    {
        int32 Cursor = 0;
        OutIndex = INDEX_NONE;
        return FindWidgetTraversalIndexByTag(RootWidget, Tag, OutIndex, Cursor);
    }

    void ClickSlateButton(const TSharedPtr<SButton>& Button)
    {
        if (!Button.IsValid())
        {
            return;
        }

        TSet<FKey> PressedButtons;
        PressedButtons.Add(EKeys::LeftMouseButton);
        const FPointerEvent PointerEvent(
            0,
            FVector2D::ZeroVector,
            FVector2D::ZeroVector,
            PressedButtons,
            EKeys::LeftMouseButton,
            0.0f,
            FModifierKeysState());

        Button->SetClickMethod(EButtonClickMethod::MouseDown);
        Button->OnMouseButtonDown(FGeometry(), PointerEvent);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealAgentAcpPanelSmokeTest,
    "UnrealAgent.Acp.PanelOpens",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealAgentAcpPanelSmokeTest::RunTest(const FString& Parameters)
{
    FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
    TSharedPtr<FTabManager> LevelEditorTabManager = LevelEditorModule.GetLevelEditorTabManager();
    if (!TestTrue(TEXT("Level Editor tab manager is available"), LevelEditorTabManager.IsValid()) || !LevelEditorTabManager.IsValid())
    {
        return false;
    }

    const FString ChatHistoryPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentAutomation"), TEXT("PanelOpensChatHistory.json"));
    IFileManager::Get().Delete(*ChatHistoryPath);
    SUnrealAgentPanel::SetChatHistoryStoragePathOverrideForAutomation(ChatHistoryPath);
    auto CleanupPanelTestHistory = [&ChatHistoryPath]()
    {
        SUnrealAgentPanel::ClearChatHistoryStoragePathOverrideForAutomation();
        IFileManager::Get().Delete(*ChatHistoryPath);
        IFileManager::Get().DeleteDirectory(*FPaths::GetPath(ChatHistoryPath), false, true);
    };

    const FName UnrealAgentTabId(TEXT("UnrealAgent"));
    TSharedPtr<SDockTab> Tab = LevelEditorTabManager->TryInvokeTab(UnrealAgentTabId);
    const bool bOpened = Tab.IsValid();
    if (!TestTrue(TEXT("Unreal Agent ACP panel opens"), bOpened) || !Tab.IsValid())
    {
        CleanupPanelTestHistory();
        return false;
    }

    TSharedRef<SWidget> PanelContent = Tab->GetContent();
    TSharedRef<SUnrealAgentPanel> Panel = StaticCastSharedRef<SUnrealAgentPanel>(PanelContent);
    Panel->ResetChatHistoryForAutomation();

    TSharedPtr<SWidget> Layout = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Layout")));
    TSharedPtr<SWidget> MainColumn = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.MainColumn")));
    TSharedPtr<SWidget> Sidebar = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Sidebar")));
    TSharedPtr<SWidget> SidebarExpanded = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Sidebar.Expanded")));
    TSharedPtr<SWidget> SidebarCollapsed = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Sidebar.Collapsed")));
    TSharedPtr<SWidget> SidebarToggle = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Sidebar.ToggleButton")));
    TSharedPtr<SWidget> SidebarNewChat = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Sidebar.NewChatButton")));
    TSharedPtr<SWidget> SidebarHistoryList = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Sidebar.History.List")));
    TSharedPtr<SWidget> SidebarHistoryEmpty = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Sidebar.History.Empty")));
    TSharedPtr<SWidget> Header = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Header")));
    TSharedPtr<SWidget> HeaderTitle = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Header.Title")));
    TSharedPtr<SWidget> HeaderConnectionIndicator = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Header.ConnectionIndicator")));
    TSharedPtr<SWidget> HeaderConnectButton = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Header.ConnectButton")));
    TSharedPtr<SWidget> Cockpit = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Cockpit")));
    TSharedPtr<SWidget> CockpitContextToggle = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Cockpit.ContextToggle")));
    TSharedPtr<SWidget> CockpitContextPreview = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Cockpit.ContextPreview")));
    TSharedPtr<SWidget> CockpitInspectContextButton = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Cockpit.InspectContextButton")));
    TSharedPtr<SWidget> CockpitValidateButton = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Cockpit.ValidateButton")));
    TSharedPtr<SWidget> CockpitEvidenceStatus = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Cockpit.EvidenceStatus")));
    TSharedPtr<SWidget> CockpitStudioKitStatus = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Cockpit.StudioKitStatus")));
    TSharedPtr<SWidget> CenterComposer = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Composer.Center")));
    TSharedPtr<SWidget> ComposerFooter = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Composer.Footer")));
    TSharedPtr<SWidget> ComposerModelControls = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Composer.ModelControls")));
    TSharedPtr<SWidget> ComposerContextWindow = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Composer.ContextWindow")));
    TSharedPtr<SWidget> ComposerContextIndicator = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Composer.ContextWindow.Indicator")));
    TSharedPtr<SWidget> ComposerContextStatus = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Composer.ContextWindow.Status")));
    TSharedPtr<SWidget> ComposerActionRow = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Composer.ActionRow")));
    TSharedPtr<SWidget> EmptyState = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.EmptyState")));
    TSharedPtr<SWidget> QuickPromptGrid = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.EmptyState.QuickPromptGrid")));
    TSharedPtr<SWidget> ArchitectureReviewPrompt = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.EmptyState.QuickPrompt.ArchitectureReview")));
    TSharedPtr<SWidget> GameplayPlanPrompt = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.EmptyState.QuickPrompt.GameplayPlan")));
    TSharedPtr<SWidget> QARiskPassPrompt = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.EmptyState.QuickPrompt.QARiskPass")));
    TSharedPtr<SWidget> EditorToolingPrompt = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.EmptyState.QuickPrompt.EditorTooling")));
    TSharedPtr<SWidget> PermissionBar = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.PermissionBar")));
    TSharedPtr<SWidget> PermissionAllowOnceButton = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Permission.AllowOnceButton")));
    TSharedPtr<SWidget> PermissionAllowAlwaysButton = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Permission.AllowAlwaysButton")));
    TSharedPtr<SWidget> PermissionRejectButton = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Permission.RejectButton")));
    TSharedPtr<SWidget> ComposerInputFrame = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Composer.InputFrame")));
    TSharedPtr<SWidget> PromptInput = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Composer.Input")));
    TSharedPtr<SWidget> ComposerHelperRow = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Composer.HelperRow")));
    TSharedPtr<SWidget> ComposerHelper = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Composer.Helper")));
    TSharedPtr<SWidget> SendButton = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Composer.SendButton")));
    TSharedPtr<SWidget> ModelCombo = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Model.Combo")));
    TSharedPtr<SWidget> ThinkingCombo = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Thinking.Combo")));
    TSharedPtr<SWidget> AgentCombo = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Agent.Combo")));
    TSharedPtr<SWidget> TranscriptScroll = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Transcript.Scroll")));

    const bool bHasLayout = TestTrue(TEXT("Unreal Agent layout is tagged"), Layout.IsValid());
    const bool bHasMainColumn = TestTrue(TEXT("Main chat column is tagged"), MainColumn.IsValid());
    const bool bHasSidebar = TestTrue(TEXT("Sidebar is tagged"), Sidebar.IsValid());
    const bool bHasHeader = TestTrue(TEXT("Unreal Agent header is tagged"), Header.IsValid());
    const bool bHasHeaderTitle = TestTrue(TEXT("Header title is tagged"), HeaderTitle.IsValid());
    const bool bHasHeaderIndicator = TestTrue(TEXT("Connection indicator is tagged"), HeaderConnectionIndicator.IsValid());
    const bool bHasHeaderConnect = TestTrue(TEXT("Connect button is tagged in the header"), HeaderConnectButton.IsValid());
    const bool bHasCenterComposer = TestTrue(TEXT("Initial composer is tagged"), CenterComposer.IsValid());
    const bool bHasComposerFooter = TestTrue(TEXT("Composer footer is tagged"), ComposerFooter.IsValid());
    bool bHasRequiredControls = true;
    bHasRequiredControls &= TestTrue(TEXT("Empty state is tagged"), EmptyState.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Quick prompt grid is tagged"), QuickPromptGrid.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Architecture review quick prompt is tagged"), ArchitectureReviewPrompt.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Gameplay plan quick prompt is tagged"), GameplayPlanPrompt.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("QA risk pass quick prompt is tagged"), QARiskPassPrompt.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Editor tooling quick prompt is tagged"), EditorToolingPrompt.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Permission bar is tagged"), PermissionBar.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Allow once permission button is tagged"), PermissionAllowOnceButton.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Always allow permission button is tagged"), PermissionAllowAlwaysButton.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Reject permission button is tagged"), PermissionRejectButton.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Composer input frame is tagged"), ComposerInputFrame.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Prompt input is tagged"), PromptInput.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Composer helper row is tagged"), ComposerHelperRow.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Composer helper is tagged"), ComposerHelper.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Send button is tagged"), SendButton.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Model combo is tagged"), ModelCombo.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Thinking combo is tagged"), ThinkingCombo.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Agent combo is tagged"), AgentCombo.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Sidebar expanded state is tagged"), SidebarExpanded.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Sidebar collapsed state is tagged"), SidebarCollapsed.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Sidebar toggle button is tagged"), SidebarToggle.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Sidebar New Chat button is tagged"), SidebarNewChat.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Sidebar history list is tagged"), SidebarHistoryList.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Sidebar history starts with an empty state"), SidebarHistoryEmpty.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Agent cockpit is tagged"), Cockpit.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Cockpit context toggle is tagged"), CockpitContextToggle.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Cockpit context preview is tagged"), CockpitContextPreview.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Cockpit inspect context button is tagged"), CockpitInspectContextButton.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Cockpit validate button is tagged"), CockpitValidateButton.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Cockpit evidence status is tagged"), CockpitEvidenceStatus.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Cockpit Studio Kit status is tagged"), CockpitStudioKitStatus.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Composer context window status is tagged"), ComposerContextWindow.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Composer context status indicator is tagged"), ComposerContextIndicator.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Composer context status text is tagged"), ComposerContextStatus.IsValid());
    bHasRequiredControls &= TestTrue(TEXT("Transcript scrollbox is tagged"), TranscriptScroll.IsValid());
    if (!bHasLayout || !bHasMainColumn || !bHasSidebar || !bHasHeader || !bHasHeaderTitle || !bHasHeaderIndicator || !bHasHeaderConnect || !bHasCenterComposer || !bHasComposerFooter || !bHasRequiredControls)
    {
        CleanupPanelTestHistory();
        return false;
    }

    const bool bConnectUnderHeader = FindWidgetByTag(Header.ToSharedRef(), FName(TEXT("UnrealAgent.Header.ConnectButton"))).IsValid();
    const bool bConnectUnderFooter = FindWidgetByTag(ComposerFooter.ToSharedRef(), FName(TEXT("UnrealAgent.Header.ConnectButton"))).IsValid();

    bool bPassed = true;
    bPassed &= TestTrue(TEXT("Sidebar is inside the main layout"), Layout.IsValid() && FindWidgetByTag(Layout.ToSharedRef(), FName(TEXT("UnrealAgent.Sidebar"))).IsValid());
    bPassed &= TestTrue(TEXT("Main chat column is inside the main layout"), Layout.IsValid() && FindWidgetByTag(Layout.ToSharedRef(), FName(TEXT("UnrealAgent.MainColumn"))).IsValid());
    bPassed &= TestTrue(TEXT("Expanded sidebar owns the New Chat button"), SidebarExpanded.IsValid() && FindWidgetByTag(SidebarExpanded.ToSharedRef(), FName(TEXT("UnrealAgent.Sidebar.NewChatButton"))).IsValid());
    bPassed &= TestTrue(TEXT("Expanded sidebar owns the history list"), SidebarExpanded.IsValid() && FindWidgetByTag(SidebarExpanded.ToSharedRef(), FName(TEXT("UnrealAgent.Sidebar.History.List"))).IsValid());
    bPassed &= TestFalse(TEXT("Expanded sidebar does not own context window status"), SidebarExpanded.IsValid() && FindWidgetByTag(SidebarExpanded.ToSharedRef(), FName(TEXT("UnrealAgent.Sidebar.ContextWindow"))).IsValid());
    bPassed &= TestTrue(TEXT("Composer footer owns context window status"), ComposerFooter.IsValid() && FindWidgetByTag(ComposerFooter.ToSharedRef(), FName(TEXT("UnrealAgent.Composer.ContextWindow"))).IsValid());
    bPassed &= TestTrue(TEXT("Composer footer owns context status indicator"), ComposerFooter.IsValid() && FindWidgetByTag(ComposerFooter.ToSharedRef(), FName(TEXT("UnrealAgent.Composer.ContextWindow.Indicator"))).IsValid());
    bPassed &= TestFalse(TEXT("Context status is hidden before ACP model load"), Panel->IsContextWindowVisibleForAutomation());
    bPassed &= TestTrue(TEXT("Transcript scrollbox stays in the main chat column"), MainColumn.IsValid() && FindWidgetByTag(MainColumn.ToSharedRef(), FName(TEXT("UnrealAgent.Transcript.Scroll"))).IsValid());
    bPassed &= TestTrue(TEXT("Connect button is inside the header row"), bConnectUnderHeader);
    bPassed &= TestFalse(TEXT("Connect button is not inside the composer footer"), bConnectUnderFooter);
    bPassed &= TestTrue(TEXT("Cockpit is inside the main chat column"), MainColumn.IsValid() && FindWidgetByTag(MainColumn.ToSharedRef(), FName(TEXT("UnrealAgent.Cockpit"))).IsValid());
    bPassed &= TestTrue(TEXT("Quick prompt buttons are inside the quick prompt grid"), QuickPromptGrid.IsValid() && FindWidgetByTag(QuickPromptGrid.ToSharedRef(), FName(TEXT("UnrealAgent.EmptyState.QuickPrompt.ArchitectureReview"))).IsValid() && FindWidgetByTag(QuickPromptGrid.ToSharedRef(), FName(TEXT("UnrealAgent.EmptyState.QuickPrompt.GameplayPlan"))).IsValid() && FindWidgetByTag(QuickPromptGrid.ToSharedRef(), FName(TEXT("UnrealAgent.EmptyState.QuickPrompt.QARiskPass"))).IsValid() && FindWidgetByTag(QuickPromptGrid.ToSharedRef(), FName(TEXT("UnrealAgent.EmptyState.QuickPrompt.EditorTooling"))).IsValid());
    bPassed &= TestTrue(TEXT("Model controls row is inside the composer footer"), ComposerModelControls.IsValid() && FindWidgetByTag(ComposerFooter.ToSharedRef(), FName(TEXT("UnrealAgent.Composer.ModelControls"))).IsValid());
    bPassed &= TestTrue(TEXT("Action row is inside the composer footer"), ComposerActionRow.IsValid() && FindWidgetByTag(ComposerFooter.ToSharedRef(), FName(TEXT("UnrealAgent.Composer.ActionRow"))).IsValid());
    bPassed &= TestTrue(TEXT("Model combo is inside the model row"), ComposerModelControls.IsValid() && FindWidgetByTag(ComposerModelControls.ToSharedRef(), FName(TEXT("UnrealAgent.Model.Combo"))).IsValid());
    bPassed &= TestTrue(TEXT("Thinking combo is inside the model row"), ComposerModelControls.IsValid() && FindWidgetByTag(ComposerModelControls.ToSharedRef(), FName(TEXT("UnrealAgent.Thinking.Combo"))).IsValid());
    bPassed &= TestTrue(TEXT("Agent combo is inside the model row"), ComposerModelControls.IsValid() && FindWidgetByTag(ComposerModelControls.ToSharedRef(), FName(TEXT("UnrealAgent.Agent.Combo"))).IsValid());
    int32 ModelComboIndex = INDEX_NONE;
    int32 ThinkingComboIndex = INDEX_NONE;
    int32 AgentComboIndex = INDEX_NONE;
    const bool bFoundModelComboIndex = ComposerModelControls.IsValid() && FindWidgetTraversalIndexByTag(ComposerModelControls.ToSharedRef(), FName(TEXT("UnrealAgent.Model.Combo")), ModelComboIndex);
    const bool bFoundThinkingComboIndex = ComposerModelControls.IsValid() && FindWidgetTraversalIndexByTag(ComposerModelControls.ToSharedRef(), FName(TEXT("UnrealAgent.Thinking.Combo")), ThinkingComboIndex);
    const bool bFoundAgentComboIndex = ComposerModelControls.IsValid() && FindWidgetTraversalIndexByTag(ComposerModelControls.ToSharedRef(), FName(TEXT("UnrealAgent.Agent.Combo")), AgentComboIndex);
    bPassed &= TestTrue(TEXT("Agent combo appears before the model combo"), bFoundAgentComboIndex && bFoundModelComboIndex && AgentComboIndex < ModelComboIndex);
    bPassed &= TestTrue(TEXT("Thinking combo appears after the model combo"), bFoundModelComboIndex && bFoundThinkingComboIndex && ModelComboIndex < ThinkingComboIndex);
    bPassed &= TestTrue(TEXT("Context window status is inside the right side of the action row"), ComposerActionRow.IsValid() && FindWidgetByTag(ComposerActionRow.ToSharedRef(), FName(TEXT("UnrealAgent.Composer.ContextWindow"))).IsValid());
    bPassed &= TestFalse(TEXT("Context window status is not inside the model controls"), ComposerModelControls.IsValid() && FindWidgetByTag(ComposerModelControls.ToSharedRef(), FName(TEXT("UnrealAgent.Composer.ContextWindow"))).IsValid());
    bPassed &= TestTrue(TEXT("Composer helper row is inside the composer footer"), ComposerHelperRow.IsValid() && FindWidgetByTag(ComposerFooter.ToSharedRef(), FName(TEXT("UnrealAgent.Composer.HelperRow"))).IsValid());
    bPassed &= TestFalse(TEXT("Composer helper is below the text box, not inside the action row"), ComposerActionRow.IsValid() && FindWidgetByTag(ComposerActionRow.ToSharedRef(), FName(TEXT("UnrealAgent.Composer.HelperRow"))).IsValid());
    bPassed &= TestTrue(TEXT("Composer helper is in the bottom centered helper row"), ComposerHelperRow.IsValid() && FindWidgetByTag(ComposerHelperRow.ToSharedRef(), FName(TEXT("UnrealAgent.Composer.Helper"))).IsValid());
    bPassed &= TestFalse(TEXT("Composer helper is separated from the action row"), ComposerActionRow.IsValid() && FindWidgetByTag(ComposerActionRow.ToSharedRef(), FName(TEXT("UnrealAgent.Composer.Helper"))).IsValid());
    bPassed &= TestTrue(TEXT("Send button is inside the composer input frame"), ComposerInputFrame.IsValid() && FindWidgetByTag(ComposerInputFrame.ToSharedRef(), FName(TEXT("UnrealAgent.Composer.SendButton"))).IsValid());
    bPassed &= TestFalse(TEXT("Send button is not inside the action row"), ComposerActionRow.IsValid() && FindWidgetByTag(ComposerActionRow.ToSharedRef(), FName(TEXT("UnrealAgent.Composer.SendButton"))).IsValid());
    bPassed &= TestTrue(TEXT("Permission actions are inside the permission bar"), PermissionBar.IsValid() && FindWidgetByTag(PermissionBar.ToSharedRef(), FName(TEXT("UnrealAgent.Permission.AllowOnceButton"))).IsValid() && FindWidgetByTag(PermissionBar.ToSharedRef(), FName(TEXT("UnrealAgent.Permission.AllowAlwaysButton"))).IsValid() && FindWidgetByTag(PermissionBar.ToSharedRef(), FName(TEXT("UnrealAgent.Permission.RejectButton"))).IsValid());
    bPassed &= TestEqual(TEXT("Unreal Agent opens as a dockable Level Editor panel tab"), Tab->GetTabRole(), ETabRole::PanelTab);

    Panel->AddTranscriptEntryForAutomation(TEXT("User"), TEXT("hi"));
    Panel->AddTranscriptEntryForAutomation(TEXT("Thought"), TEXT("thinking about the answer"));
    Panel->AddTranscriptEntryForAutomation(TEXT("Tool"), TEXT("Started Read /Game/SourceA.cpp"));
    Panel->AddTranscriptEntryForAutomation(TEXT("Tool"), TEXT("Read /Game/SourceA.cpp in_progress"));
    Panel->AddTranscriptEntryForAutomation(TEXT("Tool"), TEXT("Started Read /Game/SourceB.cpp"));
    Panel->AddTranscriptEntryForAutomation(TEXT("Tool"), TEXT("Read /Game/SourceB.cpp completed"));
    Panel->AddTranscriptEntryForAutomation(TEXT("OpenCode"), TEXT("assistant answer\n\n| Aspect | State |\n| --- | --- |\n| Engine | UE 5.x |\n| Multiplayer | Ready |"));
    Panel->AddTranscriptEntryForAutomation(TEXT("Tool"), TEXT("Started Read /Game/SourceC.cpp"));
    Panel->AddTranscriptEntryForAutomation(TEXT("OpenCode"), TEXT("next answer"));
    Panel->AddTranscriptEntryForAutomation(TEXT("OpenCode"), TEXT(" streamed tail"));
    Panel->SetActiveContextWindowUsageForAutomation(6400, 128000);

    const TSharedPtr<SWidget> UserBubble = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Transcript.UserBubble")));
    const TSharedPtr<SWidget> AssistantText = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Transcript.AssistantText")));
    const TSharedPtr<SWidget> WorkingWidget = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Transcript.Working")));
    const TSharedPtr<SWidget> WorkingHeaderWidget = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Transcript.Working.Header")));
    const TSharedPtr<SWidget> ReasoningBubble = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Transcript.ReasoningBubble")));
    const TSharedPtr<SWidget> ActivityText = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Transcript.ActivityText")));
    const TSharedPtr<SWidget> WorkingTextWidget = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Transcript.Working.Text")));
    const TSharedPtr<SWidget> ToolGroupWidget = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Transcript.ToolGroup")));
    const TSharedPtr<SWidget> ToolGroupHeaderWidget = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Transcript.ToolGroup.Header")));
    const TSharedPtr<SWidget> ToolGroupBodyWidget = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Transcript.ToolGroup.Body")));
    const TSharedPtr<SWidget> ChatHistoryRowContainer = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Sidebar.History.RowContainer")));
    const TSharedPtr<SWidget> ChatHistoryRow = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Sidebar.History.Row")));
    const TSharedPtr<SWidget> ChatHistoryActiveTitle = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Sidebar.History.ActiveTitle")));
    const TSharedPtr<SWidget> ChatHistoryPreview = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Sidebar.History.Preview")));
    const TSharedPtr<SWidget> ChatHistoryCount = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Sidebar.History.Count")));
    const TSharedPtr<SWidget> ChatHistoryRenameButton = ChatHistoryRowContainer.IsValid()
        ? FindWidgetByTag(ChatHistoryRowContainer.ToSharedRef(), FName(TEXT("UnrealAgent.Sidebar.History.RenameButton")))
        : nullptr;
    const TSharedPtr<SWidget> ChatHistoryRenameIcon = ChatHistoryRowContainer.IsValid()
        ? FindWidgetByTag(ChatHistoryRowContainer.ToSharedRef(), FName(TEXT("UnrealAgent.Sidebar.History.RenameIcon")))
        : nullptr;
    const TSharedPtr<SWidget> ChatHistoryDeleteButton = ChatHistoryRowContainer.IsValid()
        ? FindWidgetByTag(ChatHistoryRowContainer.ToSharedRef(), FName(TEXT("UnrealAgent.Sidebar.History.DeleteButton")))
        : nullptr;
    const TSharedPtr<SWidget> ChatHistoryDeleteIcon = ChatHistoryRowContainer.IsValid()
        ? FindWidgetByTag(ChatHistoryRowContainer.ToSharedRef(), FName(TEXT("UnrealAgent.Sidebar.History.DeleteIcon")))
        : nullptr;
    const TSharedPtr<SWidget> UpdatedContextStatus = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Composer.ContextWindow.Status")));
    const TSharedPtr<SExpandableArea> WorkingArea = WorkingWidget.IsValid()
        ? StaticCastSharedPtr<SExpandableArea>(WorkingWidget)
        : nullptr;
    const TSharedPtr<STextBlock> WorkingHeaderText = WorkingHeaderWidget.IsValid()
        ? StaticCastSharedPtr<STextBlock>(WorkingHeaderWidget)
        : nullptr;
    const TSharedPtr<SExpandableArea> ToolGroupArea = ToolGroupWidget.IsValid()
        ? StaticCastSharedPtr<SExpandableArea>(ToolGroupWidget)
        : nullptr;
    const TSharedPtr<STextBlock> ToolGroupHeaderText = ToolGroupHeaderWidget.IsValid()
        ? StaticCastSharedPtr<STextBlock>(ToolGroupHeaderWidget)
        : nullptr;
    const TSharedPtr<STextBlock> ToolGroupBodyText = ToolGroupBodyWidget.IsValid()
        ? StaticCastSharedPtr<STextBlock>(ToolGroupBodyWidget)
        : nullptr;
    const TSharedPtr<STextBlock> WorkingTextBlock = WorkingTextWidget.IsValid()
        ? StaticCastSharedPtr<STextBlock>(WorkingTextWidget)
        : nullptr;
    const TSharedPtr<STextBlock> AssistantTextBlock = AssistantText.IsValid()
        ? StaticCastSharedPtr<STextBlock>(AssistantText)
        : nullptr;
    const TSharedPtr<STextBlock> ChatHistoryTitleBlock = ChatHistoryActiveTitle.IsValid()
        ? StaticCastSharedPtr<STextBlock>(ChatHistoryActiveTitle)
        : nullptr;
    const TSharedPtr<STextBlock> UpdatedContextStatusBlock = UpdatedContextStatus.IsValid()
        ? StaticCastSharedPtr<STextBlock>(UpdatedContextStatus)
        : nullptr;
    FChildren* ContextIndicatorChildren = ComposerContextIndicator.IsValid()
        ? ComposerContextIndicator->GetAllChildren()
        : nullptr;
    const TSharedPtr<STextBlock> ComposerContextIndicatorTextBlock = ContextIndicatorChildren != nullptr && ContextIndicatorChildren->Num() > 0
        ? StaticCastSharedRef<STextBlock>(ContextIndicatorChildren->GetChildAt(0)).ToSharedPtr()
        : nullptr;
    bPassed &= TestTrue(TEXT("User chat keeps a boxed bubble"), UserBubble.IsValid());
    bPassed &= TestTrue(TEXT("Assistant chat renders as plain text"), AssistantText.IsValid());
    bPassed &= TestTrue(TEXT("Sidebar history row container is tagged"), ChatHistoryRowContainer.IsValid());
    bPassed &= TestTrue(TEXT("Sidebar history adds a row for the active chat"), ChatHistoryRow.IsValid());
    bPassed &= TestTrue(TEXT("Sidebar history row exposes rename option"), ChatHistoryRenameButton.IsValid());
    bPassed &= TestTrue(TEXT("Sidebar history row uses a rename icon"), ChatHistoryRenameIcon.IsValid());
    bPassed &= TestTrue(TEXT("Sidebar history row exposes delete option"), ChatHistoryDeleteButton.IsValid());
    bPassed &= TestTrue(TEXT("Sidebar history row uses a delete icon"), ChatHistoryDeleteIcon.IsValid());
    bPassed &= TestTrue(TEXT("Sidebar history uses the user prompt as the active chat title"), ChatHistoryTitleBlock.IsValid() && ChatHistoryTitleBlock->GetText().ToString() == TEXT("hi"));
    bPassed &= TestFalse(TEXT("Sidebar history row is title-only without assistant preview"), ChatHistoryPreview.IsValid());
    bPassed &= TestFalse(TEXT("Sidebar history row is title-only without entry count"), ChatHistoryCount.IsValid());
    bPassed &= TestEqual(TEXT("Sidebar history stores one active chat entry"), Panel->GetChatHistoryCountForAutomation(), 1);
    const FString UpdatedContextStatusText = UpdatedContextStatusBlock.IsValid() ? UpdatedContextStatusBlock->GetText().ToString() : FString();
    const FString ContextIndicatorText = ComposerContextIndicatorTextBlock.IsValid() ? ComposerContextIndicatorTextBlock->GetText().ToString() : FString();
    bPassed &= TestEqual(TEXT("Context status indicator renders a hollow circle glyph"), ContextIndicatorText, FString(TEXT("○")));
    bPassed &= TestTrue(TEXT("Context window status shows only used percentage"), UpdatedContextStatusText.Contains(TEXT("used")) && !UpdatedContextStatusText.Contains(TEXT("free")) && !UpdatedContextStatusText.Contains(TEXT("remaining")) && !UpdatedContextStatusText.Contains(TEXT("/")) && !UpdatedContextStatusText.Contains(TEXT("Context window")));
    bPassed &= TestEqual(TEXT("Context window status follows the active chat usage"), Panel->GetContextWindowStatusTextForAutomation(), FString(TEXT("5% used")));
    bPassed &= TestTrue(TEXT("Assistant Markdown tables render as readable lines"), AssistantTextBlock.IsValid() && AssistantTextBlock->GetText().ToString().Contains(TEXT("Engine: UE 5.x")) && AssistantTextBlock->GetText().ToString().Contains(TEXT("Multiplayer: Ready")));
    bPassed &= TestFalse(TEXT("Assistant Markdown table pipes are not shown raw"), AssistantTextBlock.IsValid() && AssistantTextBlock->GetText().ToString().Contains(TEXT("|")));
    bPassed &= TestTrue(TEXT("Tool and reasoning activity collapse into working row"), WorkingArea.IsValid());
    bPassed &= TestTrue(TEXT("Reasoning activity text remains visible"), WorkingTextBlock.IsValid() && WorkingTextBlock->GetText().ToString().Contains(TEXT("thinking about the answer")));
    bPassed &= TestFalse(TEXT("Reasoning activity text does not show a redundant label"), WorkingTextBlock.IsValid() && WorkingTextBlock->GetText().ToString().Contains(TEXT("Reasoning")));
    bPassed &= TestTrue(TEXT("Working header stays clean"), WorkingHeaderText.IsValid() && WorkingHeaderText->GetText().ToString().StartsWith(TEXT("Working")) && !WorkingHeaderText->GetText().ToString().Contains(TEXT("Reasoned")) && !WorkingHeaderText->GetText().ToString().Contains(TEXT("updates")) && !WorkingHeaderText->GetText().ToString().Contains(TEXT("...")));
    bPassed &= TestTrue(TEXT("Repeated same-tool activity is grouped"), ToolGroupArea.IsValid());
    bPassed &= TestTrue(TEXT("Tool group has a readable header"), ToolGroupHeaderText.IsValid() && ToolGroupHeaderText->GetText().ToString().Contains(TEXT("Read")) && ToolGroupHeaderText->GetText().ToString().Contains(TEXT("2")));
    bPassed &= TestTrue(TEXT("Tool group body lists useful tool details"), ToolGroupBodyText.IsValid() && ToolGroupBodyText->GetText().ToString().Contains(TEXT("/Game/SourceA.cpp")) && ToolGroupBodyText->GetText().ToString().Contains(TEXT("/Game/SourceB.cpp")));
    bPassed &= TestFalse(TEXT("Tool group body suppresses raw start/status logs"), ToolGroupBodyText.IsValid() && (ToolGroupBodyText->GetText().ToString().Contains(TEXT("Started")) || ToolGroupBodyText->GetText().ToString().Contains(TEXT("in_progress")) || ToolGroupBodyText->GetText().ToString().Contains(TEXT("completed"))));
    bPassed &= TestFalse(TEXT("Reasoning no longer uses a boxed bubble"), ReasoningBubble.IsValid());
    bPassed &= TestFalse(TEXT("Tool activity is not shown as loose transcript text"), ActivityText.IsValid());

    TArray<TSharedPtr<SWidget>> WorkingHeaderWidgets;
    FindWidgetsByTag(PanelContent, FName(TEXT("UnrealAgent.Transcript.Working.Header")), WorkingHeaderWidgets);
    bPassed &= TestTrue(TEXT("Each activity burst gets its own working row"), WorkingHeaderWidgets.Num() >= 2);
    for (const TSharedPtr<SWidget>& HeaderWidget : WorkingHeaderWidgets)
    {
        const TSharedPtr<STextBlock> HeaderTextBlock = HeaderWidget.IsValid()
            ? StaticCastSharedPtr<STextBlock>(HeaderWidget)
            : nullptr;
        const FString HeaderText = HeaderTextBlock.IsValid() ? HeaderTextBlock->GetText().ToString() : FString();
        bPassed &= TestTrue(TEXT("Completed working rows keep elapsed text"), HeaderText.StartsWith(TEXT("Working")) && HeaderText.Contains(TEXT("sec")));
        bPassed &= TestFalse(TEXT("Completed working rows stop animating"), HeaderText.Contains(TEXT("...")));
    }

    if (WorkingArea.IsValid())
    {
        bPassed &= TestFalse(TEXT("Working activity starts collapsed"), WorkingArea->IsExpanded());
    }
    if (ToolGroupArea.IsValid())
    {
        bPassed &= TestFalse(TEXT("Same-tool group starts collapsed"), ToolGroupArea->IsExpanded());
    }

    const TSharedPtr<SButton> NewChatButton = SidebarNewChat.IsValid()
        ? StaticCastSharedPtr<SButton>(SidebarNewChat)
        : nullptr;
    if (NewChatButton.IsValid())
    {
        ClickSlateButton(NewChatButton);
    }
    bPassed &= TestTrue(TEXT("New Chat clears the visible transcript"), !FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Transcript.UserBubble"))).IsValid() && !FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Transcript.AssistantText"))).IsValid());
    bPassed &= TestEqual(TEXT("New Chat resets context usage to an empty chat"), Panel->GetContextWindowStatusTextForAutomation(), FString(TEXT("0% used")));
    const TSharedPtr<SWidget> SavedHistoryRow = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Sidebar.History.Row")));
    bPassed &= TestTrue(TEXT("New Chat keeps the previous chat in sidebar history"), SavedHistoryRow.IsValid());

    Panel->AddTranscriptEntryForAutomation(TEXT("User"), TEXT("second chat"));
    Panel->AddTranscriptEntryForAutomation(TEXT("OpenCode"), TEXT("second answer"));
    Panel->SetActiveContextWindowUsageForAutomation(32000, 64000);
    bPassed &= TestEqual(TEXT("Second chat creates another sidebar history entry"), Panel->GetChatHistoryCountForAutomation(), 2);
    bPassed &= TestEqual(TEXT("Second chat shows its own context usage"), Panel->GetContextWindowStatusTextForAutomation(), FString(TEXT("50% used")));

    auto FindHistoryContainerByTitleInRoot = [](const TSharedRef<SWidget>& RootWidget, const FString& ExpectedTitle) -> TSharedPtr<SWidget>
    {
        TArray<TSharedPtr<SWidget>> HistoryRowContainers;
        FindWidgetsByTag(RootWidget, FName(TEXT("UnrealAgent.Sidebar.History.RowContainer")), HistoryRowContainers);
        for (const TSharedPtr<SWidget>& HistoryRowContainerWidget : HistoryRowContainers)
        {
            if (!HistoryRowContainerWidget.IsValid())
            {
                continue;
            }

            TSharedPtr<SWidget> TitleWidget = FindWidgetByTag(HistoryRowContainerWidget.ToSharedRef(), FName(TEXT("UnrealAgent.Sidebar.History.ActiveTitle")));
            if (!TitleWidget.IsValid())
            {
                TitleWidget = FindWidgetByTag(HistoryRowContainerWidget.ToSharedRef(), FName(TEXT("UnrealAgent.Sidebar.History.Title")));
            }

            const TSharedPtr<STextBlock> TitleTextBlock = TitleWidget.IsValid()
                ? StaticCastSharedPtr<STextBlock>(TitleWidget)
                : nullptr;
            if (TitleTextBlock.IsValid() && TitleTextBlock->GetText().ToString() == ExpectedTitle)
            {
                return HistoryRowContainerWidget;
            }
        }

        return nullptr;
    };

    auto FindHistoryContainerByTitle = [&PanelContent, &FindHistoryContainerByTitleInRoot](const FString& ExpectedTitle) -> TSharedPtr<SWidget>
    {
        return FindHistoryContainerByTitleInRoot(PanelContent, ExpectedTitle);
    };

    auto FindHistoryButtonByTitle = [&FindHistoryContainerByTitle](const FString& ExpectedTitle) -> TSharedPtr<SButton>
    {
        const TSharedPtr<SWidget> HistoryRowContainerWidget = FindHistoryContainerByTitle(ExpectedTitle);
        if (!HistoryRowContainerWidget.IsValid())
        {
            return nullptr;
        }

        const TSharedPtr<SWidget> HistoryButtonWidget = FindWidgetByTag(HistoryRowContainerWidget.ToSharedRef(), FName(TEXT("UnrealAgent.Sidebar.History.Row")));
        return HistoryButtonWidget.IsValid() ? StaticCastSharedPtr<SButton>(HistoryButtonWidget) : nullptr;
    };

    const TSharedPtr<SButton> SavedHistoryButton = FindHistoryButtonByTitle(TEXT("hi"));
    bPassed &= TestTrue(TEXT("Sidebar history row is clickable"), SavedHistoryButton.IsValid());
    if (SavedHistoryButton.IsValid())
    {
        ClickSlateButton(SavedHistoryButton);
    }

    const TSharedPtr<SWidget> RestoredUserBubble = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Transcript.UserBubble")));
    const TSharedPtr<SWidget> RestoredUserText = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Transcript.UserText")));
    const TSharedPtr<STextBlock> RestoredUserTextBlock = RestoredUserText.IsValid()
        ? StaticCastSharedPtr<STextBlock>(RestoredUserText)
        : nullptr;
    TArray<TSharedPtr<SWidget>> RestoredAssistantTexts;
    FindWidgetsByTag(PanelContent, FName(TEXT("UnrealAgent.Transcript.AssistantText")), RestoredAssistantTexts);
    const TSharedPtr<SWidget> RestoredWorkingWidget = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Transcript.Working")));
    const TSharedPtr<SWidget> RestoredWorkingTextWidget = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Transcript.Working.Text")));
    const TSharedPtr<SWidget> RestoredToolGroupBodyWidget = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Transcript.ToolGroup.Body")));
    const TSharedPtr<STextBlock> RestoredWorkingTextBlock = RestoredWorkingTextWidget.IsValid()
        ? StaticCastSharedPtr<STextBlock>(RestoredWorkingTextWidget)
        : nullptr;
    const TSharedPtr<STextBlock> RestoredToolGroupBodyTextBlock = RestoredToolGroupBodyWidget.IsValid()
        ? StaticCastSharedPtr<STextBlock>(RestoredToolGroupBodyWidget)
        : nullptr;
    TArray<TSharedPtr<SWidget>> RestoredWorkingHeaderWidgets;
    FindWidgetsByTag(PanelContent, FName(TEXT("UnrealAgent.Transcript.Working.Header")), RestoredWorkingHeaderWidgets);
    const bool bRestoredLatestAssistantText = RestoredAssistantTexts.ContainsByPredicate([](const TSharedPtr<SWidget>& AssistantWidget)
    {
        const TSharedPtr<STextBlock> AssistantTextBlock = AssistantWidget.IsValid()
            ? StaticCastSharedPtr<STextBlock>(AssistantWidget)
            : nullptr;
        return AssistantTextBlock.IsValid() && AssistantTextBlock->GetText().ToString().Contains(TEXT("next answer streamed tail"));
    });
    bPassed &= TestTrue(TEXT("Clicking a history row restores the user transcript"), RestoredUserBubble.IsValid());
    bPassed &= TestTrue(TEXT("Clicking a history row restores the user text"), RestoredUserTextBlock.IsValid() && RestoredUserTextBlock->GetText().ToString() == TEXT("hi"));
    bPassed &= TestEqual(TEXT("Clicking a history row restores assistant turns as separate chat rows"), RestoredAssistantTexts.Num(), 2);
    bPassed &= TestTrue(TEXT("Clicking a history row restores the assistant transcript"), bRestoredLatestAssistantText);
    bPassed &= TestTrue(TEXT("Clicking a history row restores working activity rows"), RestoredWorkingWidget.IsValid() && RestoredWorkingHeaderWidgets.Num() >= 2);
    bPassed &= TestTrue(TEXT("Clicking a history row restores reasoning activity text"), RestoredWorkingTextBlock.IsValid() && RestoredWorkingTextBlock->GetText().ToString().Contains(TEXT("thinking about the answer")));
    bPassed &= TestTrue(TEXT("Clicking a history row restores tool activity groups"), RestoredToolGroupBodyTextBlock.IsValid() && RestoredToolGroupBodyTextBlock->GetText().ToString().Contains(TEXT("/Game/SourceA.cpp")));
    bPassed &= TestTrue(TEXT("Clicking a history row marks it active again"), FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Sidebar.History.ActiveTitle"))).IsValid());
    bPassed &= TestEqual(TEXT("Clicking a history row restores that chat's context usage"), Panel->GetContextWindowStatusTextForAutomation(), FString(TEXT("5% used")));

    const TSharedPtr<SWidget> RestoredHistoryContainer = FindHistoryContainerByTitle(TEXT("hi"));
    const TSharedPtr<SWidget> RestoredRenameButtonWidget = RestoredHistoryContainer.IsValid()
        ? FindWidgetByTag(RestoredHistoryContainer.ToSharedRef(), FName(TEXT("UnrealAgent.Sidebar.History.RenameButton")))
        : nullptr;
    const TSharedPtr<SButton> RestoredRenameButton = RestoredRenameButtonWidget.IsValid()
        ? StaticCastSharedPtr<SButton>(RestoredRenameButtonWidget)
        : nullptr;
    bPassed &= TestTrue(TEXT("Restored history row exposes rename action"), RestoredRenameButton.IsValid());
    if (RestoredRenameButton.IsValid())
    {
        ClickSlateButton(RestoredRenameButton);
    }

    const TSharedPtr<SWidget> CancelRenameInputWidget = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Sidebar.History.RenameInput")));
    const TSharedPtr<SWidget> CancelRenameButtonWidget = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Sidebar.History.RenameCancelButton")));
    const TSharedPtr<SButton> CancelRenameButton = CancelRenameButtonWidget.IsValid()
        ? StaticCastSharedPtr<SButton>(CancelRenameButtonWidget)
        : nullptr;
    bPassed &= TestTrue(TEXT("Rename mode shows an editable title input"), CancelRenameInputWidget.IsValid());
    bPassed &= TestTrue(TEXT("Rename mode shows a cancel option"), CancelRenameButton.IsValid());
    if (CancelRenameButton.IsValid())
    {
        ClickSlateButton(CancelRenameButton);
    }
    bPassed &= TestTrue(TEXT("Cancelling rename keeps the original title"), FindHistoryContainerByTitle(TEXT("hi")).IsValid());
    bPassed &= TestFalse(TEXT("Cancelling rename hides the edit input"), FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Sidebar.History.RenameInput"))).IsValid());

    const TSharedPtr<SWidget> RenameHistoryContainer = FindHistoryContainerByTitle(TEXT("hi"));
    const TSharedPtr<SWidget> RenameButtonWidget = RenameHistoryContainer.IsValid()
        ? FindWidgetByTag(RenameHistoryContainer.ToSharedRef(), FName(TEXT("UnrealAgent.Sidebar.History.RenameButton")))
        : nullptr;
    const TSharedPtr<SButton> RenameButton = RenameButtonWidget.IsValid()
        ? StaticCastSharedPtr<SButton>(RenameButtonWidget)
        : nullptr;
    if (RenameButton.IsValid())
    {
        ClickSlateButton(RenameButton);
    }

    const TSharedPtr<SWidget> RenameInputWidget = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Sidebar.History.RenameInput")));
    const TSharedPtr<SWidget> RenameSaveButtonWidget = FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Sidebar.History.RenameSaveButton")));
    const TSharedPtr<SEditableTextBox> RenameInput = RenameInputWidget.IsValid()
        ? StaticCastSharedPtr<SEditableTextBox>(RenameInputWidget)
        : nullptr;
    const TSharedPtr<SButton> RenameSaveButton = RenameSaveButtonWidget.IsValid()
        ? StaticCastSharedPtr<SButton>(RenameSaveButtonWidget)
        : nullptr;
    bPassed &= TestTrue(TEXT("Rename mode shows a save option"), RenameSaveButton.IsValid());
    if (RenameInput.IsValid())
    {
        RenameInput->SetText(FText::FromString(TEXT("renamed chat")));
    }
    if (RenameSaveButton.IsValid())
    {
        ClickSlateButton(RenameSaveButton);
    }
    bPassed &= TestTrue(TEXT("Saving rename updates the visible chat title"), FindHistoryContainerByTitle(TEXT("renamed chat")).IsValid());
    bPassed &= TestFalse(TEXT("Saving rename removes the previous chat title"), FindHistoryContainerByTitle(TEXT("hi")).IsValid());

    Panel->AddTranscriptEntryForAutomation(TEXT("User"), TEXT("prompt after rename"));
    Panel->AddTranscriptEntryForAutomation(TEXT("OpenCode"), TEXT("answer after rename"));
    bPassed &= TestTrue(TEXT("Manual chat rename survives continuing the active chat"), FindHistoryContainerByTitle(TEXT("renamed chat")).IsValid());
    bPassed &= TestFalse(TEXT("Manual chat rename is not replaced by later prompts"), FindHistoryContainerByTitle(TEXT("prompt after rename")).IsValid());

    FString SavedChatHistoryJson;
    bPassed &= TestTrue(TEXT("Sidebar history is written to isolated test storage"), FPaths::FileExists(ChatHistoryPath) && FFileHelper::LoadFileToString(SavedChatHistoryJson, *ChatHistoryPath));
    bPassed &= TestTrue(TEXT("Saved sidebar history includes the renamed active chat title"), SavedChatHistoryJson.Contains(TEXT("renamed chat")));
    bPassed &= TestTrue(TEXT("Saved sidebar history keeps transcript entries added after rename"), SavedChatHistoryJson.Contains(TEXT("prompt after rename")) && SavedChatHistoryJson.Contains(TEXT("answer after rename")));
    bPassed &= TestTrue(TEXT("Saved sidebar history includes restorable transcript entries"), SavedChatHistoryJson.Contains(TEXT("\"Transcript\"")) && SavedChatHistoryJson.Contains(TEXT("next answer streamed tail")));
    bPassed &= TestTrue(TEXT("Saved sidebar history includes visible activity entries"), SavedChatHistoryJson.Contains(TEXT("thinking about the answer")) && SavedChatHistoryJson.Contains(TEXT("/Game/SourceA.cpp")));
    const TSharedRef<SUnrealAgentPanel> ReloadedPanel = SNew(SUnrealAgentPanel);
    const TSharedRef<SWidget> ReloadedPanelWidget = StaticCastSharedRef<SWidget>(ReloadedPanel);
    bPassed &= TestEqual(TEXT("Saved sidebar history reloads in a new panel instance"), ReloadedPanel->GetChatHistoryCountForAutomation(), 2);
    bPassed &= TestTrue(TEXT("Saved sidebar history reloads the renamed previous chat"), FindHistoryContainerByTitleInRoot(ReloadedPanelWidget, TEXT("renamed chat")).IsValid());
    bPassed &= TestTrue(TEXT("Saved sidebar history reloads the second saved chat"), FindHistoryContainerByTitleInRoot(ReloadedPanelWidget, TEXT("second chat")).IsValid());

    const TSharedPtr<SWidget> RenamedHistoryContainer = FindHistoryContainerByTitle(TEXT("renamed chat"));
    const TSharedPtr<SWidget> DeleteButtonWidget = RenamedHistoryContainer.IsValid()
        ? FindWidgetByTag(RenamedHistoryContainer.ToSharedRef(), FName(TEXT("UnrealAgent.Sidebar.History.DeleteButton")))
        : nullptr;
    const TSharedPtr<SButton> DeleteButton = DeleteButtonWidget.IsValid()
        ? StaticCastSharedPtr<SButton>(DeleteButtonWidget)
        : nullptr;
    bPassed &= TestTrue(TEXT("Renamed history row exposes delete action"), DeleteButton.IsValid());
    if (DeleteButton.IsValid())
    {
        ClickSlateButton(DeleteButton);
    }
    bPassed &= TestEqual(TEXT("Deleting a chat removes it from sidebar history"), Panel->GetChatHistoryCountForAutomation(), 1);
    bPassed &= TestTrue(TEXT("Deleting the active chat clears the visible transcript"), !FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Transcript.UserBubble"))).IsValid() && !FindWidgetByTag(PanelContent, FName(TEXT("UnrealAgent.Transcript.AssistantText"))).IsValid());
    FString DeletedChatHistoryJson;
    bPassed &= TestTrue(TEXT("Deleting a chat persists the history file"), FPaths::FileExists(ChatHistoryPath) && FFileHelper::LoadFileToString(DeletedChatHistoryJson, *ChatHistoryPath));
    bPassed &= TestFalse(TEXT("Deleted chat title is removed from persisted history"), DeletedChatHistoryJson.Contains(TEXT("renamed chat")));

    const bool bSidebarCollapsedBeforeToggle = Panel->IsSidebarCollapsedForAutomation();
    Panel->ToggleSidebarForAutomation();
    bPassed &= TestEqual(TEXT("Sidebar toggle flips the sidebar collapsed state"), Panel->IsSidebarCollapsedForAutomation(), !bSidebarCollapsedBeforeToggle);
    Panel->ToggleSidebarForAutomation();
    bPassed &= TestEqual(TEXT("Sidebar toggle restores the original sidebar state"), Panel->IsSidebarCollapsedForAutomation(), bSidebarCollapsedBeforeToggle);
    Panel->ResetChatHistoryForAutomation();
    CleanupPanelTestHistory();
    return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealAgentStudioKitAndContextTest,
    "UnrealAgent.Acp.StudioKitAndContext",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealAgentStudioKitAndContextTest::RunTest(const FString& Parameters)
{
    const FString TestDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentStudioKitTest")));
    IFileManager::Get().DeleteDirectory(*TestDirectory, false, true);
    if (!IFileManager::Get().MakeDirectory(*TestDirectory, true))
    {
        AddError(FString::Printf(TEXT("Failed to create Studio Kit test directory: %s"), *TestDirectory));
        return false;
    }

    bool bPassed = true;
    const FUnrealAgentStudioKitResult KitResult = FUnrealAgentStudioKit::EnsureForProject(TestDirectory);
    bPassed &= TestTrue(TEXT("Studio Kit generation succeeds"), KitResult.WasSuccessful());
    bPassed &= TestTrue(TEXT("Studio Kit writes multiple OpenCode files"), KitResult.FilesWritten >= 10);

    FString PrimaryAgent;
    const FString PrimaryAgentPath = FPaths::Combine(TestDirectory, TEXT(".opencode/agents/unreal-agent.md"));
    bPassed &= TestTrue(TEXT("Primary Unreal Agent file exists"), FPaths::FileExists(PrimaryAgentPath) && FFileHelper::LoadFileToString(PrimaryAgent, *PrimaryAgentPath));
    bPassed &= TestTrue(TEXT("Primary agent has prompt marker"), PrimaryAgent.Contains(FUnrealAgentStudioKit::GetPromptVersionMarker()));
    bPassed &= TestTrue(TEXT("Primary agent has Studio Kit marker"), PrimaryAgent.Contains(FUnrealAgentStudioKit::GetStudioKitVersionMarker()));
    bPassed &= TestTrue(TEXT("Primary agent includes specialist roles"), PrimaryAgent.Contains(TEXT("unreal-technical-director")) && PrimaryAgent.Contains(TEXT("unreal-qa-release")));
    bPassed &= TestTrue(TEXT("Primary agent includes MCP tool playbook"), PrimaryAgent.Contains(TEXT("MCP tool playbook")));
    bPassed &= TestTrue(TEXT("Primary agent includes release workflow"), PrimaryAgent.Contains(TEXT("Shipping: confirm packaging readiness")));

    const FString SkillPath = FPaths::Combine(TestDirectory, TEXT(".opencode/skills/unreal-validation-loop/SKILL.md"));
    const FString PluginPath = FPaths::Combine(TestDirectory, TEXT(".opencode/plugins/unreal-agent-guardrails.ts"));
    const FString CommandPath = FPaths::Combine(TestDirectory, TEXT(".opencode/commands/unreal-ship-check.md"));
    const FString ConfigPath = FPaths::Combine(TestDirectory, TEXT(".opencode/opencode.json"));
    bPassed &= TestTrue(TEXT("Validation skill is generated"), FPaths::FileExists(SkillPath));
    bPassed &= TestTrue(TEXT("Guardrails plugin is generated"), FPaths::FileExists(PluginPath));
    bPassed &= TestTrue(TEXT("Ship check command is generated"), FPaths::FileExists(CommandPath));
    bPassed &= TestTrue(TEXT("OpenCode config is generated"), FPaths::FileExists(ConfigPath));
    FString OpenCodeConfigText;
    bPassed &= TestTrue(TEXT("OpenCode config is readable"), FFileHelper::LoadFileToString(OpenCodeConfigText, *ConfigPath));
    bPassed &= TestTrue(TEXT("OpenCode config has Studio Kit comment marker"), OpenCodeConfigText.Contains(FUnrealAgentStudioKit::GetStudioKitVersionMarker()));
    bPassed &= TestFalse(TEXT("OpenCode config does not contain unknown Studio Kit keys"), OpenCodeConfigText.Contains(TEXT("\"unreal_agent_studio_kit_version\"")));

    const FString LegacyConfigDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentStudioKitLegacyConfigTest")));
    IFileManager::Get().DeleteDirectory(*LegacyConfigDirectory, false, true);
    const FString LegacyConfigPath = FPaths::Combine(LegacyConfigDirectory, TEXT(".opencode/opencode.json"));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(LegacyConfigPath), true);
    const FString LegacyConfigText = FString()
        + TEXT("{\n")
        + TEXT("  \"$schema\": \"https://opencode.ai/config.json\",\n")
        + TEXT("  \"permission\": {\n")
        + TEXT("    \"read\": \"allow\",\n")
        + TEXT("    \"glob\": \"allow\",\n")
        + TEXT("    \"grep\": \"allow\",\n")
        + TEXT("    \"list\": \"allow\",\n")
        + TEXT("    \"edit\": \"ask\",\n")
        + TEXT("    \"bash\": \"ask\",\n")
        + TEXT("    \"skill\": {\n")
        + TEXT("      \"unreal-*\": \"allow\"\n")
        + TEXT("    }\n")
        + TEXT("  }\n")
        + TEXT("}\n");
    bPassed &= TestTrue(TEXT("Legacy generated OpenCode config is seeded"), FFileHelper::SaveStringToFile(LegacyConfigText, *LegacyConfigPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
    FUnrealAgentStudioKit::EnsureForProject(LegacyConfigDirectory);
    FString UpgradedLegacyConfigText;
    bPassed &= TestTrue(TEXT("Legacy generated OpenCode config remains readable"), FFileHelper::LoadFileToString(UpgradedLegacyConfigText, *LegacyConfigPath));
    bPassed &= TestTrue(TEXT("Legacy generated OpenCode config is upgraded with marker"), UpgradedLegacyConfigText.Contains(FUnrealAgentStudioKit::GetStudioKitVersionMarker()));

    const FString Redacted = FUnrealAgentStudioKit::RedactSensitiveText(TEXT("Authorization: Bearer abc123\nX-MCP-Capability-Token: fake-capability-token\nsafe: value"));
    bPassed &= TestFalse(TEXT("Redaction removes bearer token"), Redacted.Contains(TEXT("abc123")));
    bPassed &= TestFalse(TEXT("Redaction removes capability token"), Redacted.Contains(TEXT("fake-capability-token")));
    bPassed &= TestTrue(TEXT("Redaction keeps safe lines"), Redacted.Contains(TEXT("safe: value")));

    const FUnrealAgentEditorContextSnapshot Context = FUnrealAgentEditorContext::Capture(TestDirectory);
    bPassed &= TestTrue(TEXT("Editor context envelope is produced"), Context.Envelope.Contains(TEXT("<unreal_editor_context")));
    bPassed &= TestTrue(TEXT("Editor context has privacy guidance"), Context.Envelope.Contains(TEXT("Sensitive credential values")));
    bPassed &= TestFalse(TEXT("Editor context does not expose fake secrets"), Context.Envelope.Contains(TEXT("fake-capability-token")));
    bPassed &= TestFalse(TEXT("Editor context does not expose absolute project directory"), Context.Envelope.Contains(TestDirectory));
    bPassed &= TestTrue(TEXT("Editor context redacts project directory"), Context.Envelope.Contains(TEXT("projectDir: [redacted project root]")));

    const FUnrealAgentValidationResult ValidationResult = FUnrealAgentValidationRunner::RunFastValidation(TestDirectory);
    bPassed &= TestTrue(TEXT("Fast validation passes after Studio Kit generation"), ValidationResult.bPassed);
    bPassed &= TestTrue(TEXT("Validation records evidence"), FPaths::FileExists(ValidationResult.EvidencePath));

    FString FirstEvidencePath;
    FString SecondEvidencePath;
    bPassed &= TestTrue(TEXT("First same-type evidence event records"), FUnrealAgentEvidenceLedger::RecordEvent(TestDirectory, TEXT("collision"), TEXT("passed"), TEXT("same summary"), TEXT("details"), &FirstEvidencePath));
    bPassed &= TestTrue(TEXT("Second same-type evidence event records"), FUnrealAgentEvidenceLedger::RecordEvent(TestDirectory, TEXT("collision"), TEXT("passed"), TEXT("same summary"), TEXT("details"), &SecondEvidencePath));
    bPassed &= TestTrue(TEXT("Same-second evidence events use distinct paths"), !FirstEvidencePath.IsEmpty() && !SecondEvidencePath.IsEmpty() && FirstEvidencePath != SecondEvidencePath && FPaths::FileExists(FirstEvidencePath) && FPaths::FileExists(SecondEvidencePath));

    const FString MissingDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentMissingValidationTest")));
    IFileManager::Get().DeleteDirectory(*MissingDirectory, false, true);
    const FUnrealAgentValidationResult MissingValidationResult = FUnrealAgentValidationRunner::RunFastValidation(MissingDirectory);
    bPassed &= TestFalse(TEXT("Validation fails for missing project directory"), MissingValidationResult.bPassed);
    bPassed &= TestTrue(TEXT("Validation reports missing project directory"), !MissingValidationResult.Errors.IsEmpty() && MissingValidationResult.Errors[0].Contains(TEXT("Project directory does not exist")));
    bPassed &= TestTrue(TEXT("Missing project validation does not write evidence"), MissingValidationResult.EvidencePath.IsEmpty() && !FPaths::DirectoryExists(FPaths::Combine(MissingDirectory, TEXT("Saved/UnrealAgent"))));

    const FString BrokenDecisionsDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentBrokenDecisionsTest")));
    IFileManager::Get().DeleteDirectory(*BrokenDecisionsDirectory, false, true);
    FUnrealAgentEvidenceLedger::EnsureLedger(BrokenDecisionsDirectory);
    const FString BrokenDecisionsPath = FPaths::Combine(BrokenDecisionsDirectory, TEXT("Saved/UnrealAgent/decisions.md"));
    IFileManager::Get().Delete(*BrokenDecisionsPath);
    IFileManager::Get().MakeDirectory(*BrokenDecisionsPath, true);
    AddExpectedErrorPlain(TEXT("UnrealAgentBrokenDecisionsTest"), EAutomationExpectedErrorFlags::Contains, 2);
    FString BrokenDecisionsEvidencePath = TEXT("stale-evidence-path");
    bPassed &= TestFalse(TEXT("Evidence recording fails when decisions ledger cannot be appended"), FUnrealAgentEvidenceLedger::RecordEvent(BrokenDecisionsDirectory, TEXT("broken-decisions"), TEXT("failed"), TEXT("summary"), TEXT("details"), &BrokenDecisionsEvidencePath));
    bPassed &= TestTrue(TEXT("Decision write failure clears stale success path"), BrokenDecisionsEvidencePath.IsEmpty());

    const FString BrokenStateDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentBrokenStateTest")));
    IFileManager::Get().DeleteDirectory(*BrokenStateDirectory, false, true);
    FUnrealAgentEvidenceLedger::EnsureLedger(BrokenStateDirectory);
    const FString BrokenStatePath = FPaths::Combine(BrokenStateDirectory, TEXT("Saved/UnrealAgent/state.json"));
    IFileManager::Get().Delete(*BrokenStatePath);
    IFileManager::Get().MakeDirectory(*BrokenStatePath, true);
    AddExpectedErrorPlain(TEXT("UnrealAgentBrokenStateTest"), EAutomationExpectedErrorFlags::Contains, 2);
    FString BrokenStateEvidencePath = TEXT("stale-evidence-path");
    bPassed &= TestFalse(TEXT("Evidence recording fails when state ledger cannot be written"), FUnrealAgentEvidenceLedger::RecordEvent(BrokenStateDirectory, TEXT("broken-state"), TEXT("failed"), TEXT("summary"), TEXT("details"), &BrokenStateEvidencePath));
    bPassed &= TestTrue(TEXT("State write failure clears stale success path"), BrokenStateEvidencePath.IsEmpty());

    const FString CustomDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentStudioKitCustomTest")));
    IFileManager::Get().DeleteDirectory(*CustomDirectory, false, true);
    const FString CustomAgentPath = FPaths::Combine(CustomDirectory, TEXT(".opencode/agents/unreal-agent.md"));
    const FString CustomConfigPath = FPaths::Combine(CustomDirectory, TEXT(".opencode/opencode.json"));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(CustomAgentPath), true);
    const FString CustomAgentText = TEXT("custom user-owned Unreal Agent prompt");
    const FString CustomConfigText = TEXT("{\n  \"$schema\": \"https://opencode.ai/config.json\",\n  \"permission\": {\n    \"read\": \"deny\"\n  }\n}\n");
    FFileHelper::SaveStringToFile(CustomAgentText, *CustomAgentPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    FFileHelper::SaveStringToFile(CustomConfigText, *CustomConfigPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    FUnrealAgentStudioKit::EnsureForProject(CustomDirectory);
    FString PreservedAgentText;
    FString PreservedConfigText;
    bPassed &= TestTrue(TEXT("Custom prompt remains readable"), FFileHelper::LoadFileToString(PreservedAgentText, *CustomAgentPath));
    bPassed &= TestEqual(TEXT("Custom unmarked prompt is preserved"), PreservedAgentText, CustomAgentText);
    bPassed &= TestTrue(TEXT("Custom OpenCode config remains readable"), FFileHelper::LoadFileToString(PreservedConfigText, *CustomConfigPath));
    bPassed &= TestEqual(TEXT("Custom unmarked OpenCode config is preserved"), PreservedConfigText, CustomConfigText);

    IFileManager::Get().DeleteDirectory(*TestDirectory, false, true);
    IFileManager::Get().DeleteDirectory(*LegacyConfigDirectory, false, true);
    IFileManager::Get().DeleteDirectory(*CustomDirectory, false, true);
    IFileManager::Get().DeleteDirectory(*BrokenDecisionsDirectory, false, true);
    IFileManager::Get().DeleteDirectory(*BrokenStateDirectory, false, true);
    return bPassed;
}

namespace
{
    FString MakeFakeAcpScript()
    {
        return FString()
            + TEXT("#!/usr/bin/python3\n")
            + TEXT("import json, signal, sys\n")
            + TEXT("if hasattr(signal, 'SIGPWR') and signal.getsignal(signal.SIGPWR) == signal.SIG_IGN:\n")
            + TEXT("    print('fake ACP inherited ignored SIGPWR', file=sys.stderr, flush=True)\n")
            + TEXT("    sys.exit(42)\n")
            + TEXT("session_id = 'fake-session'\n")
            + TEXT("prompt_id = None\n")
            + TEXT("permission_id = 1000\n")
            + TEXT("cancelled_permission = False\n")
            + TEXT("current_model = 'model-a'\n")
            + TEXT("current_thinking = 'medium'\n")
            + TEXT("current_agent = 'build'\n")
            + TEXT("model_options = [{'id': 'model-a', 'label': 'Model A', 'provider': {'name': 'NVIDIA'}, 'contextLength': 128000}, {'optionId': 'model-b', 'name': 'Model B', 'vendor': 'Modal', 'maxInputTokens': 64000}, {'modelId': 'openai/gpt-5.5', 'name': 'openai/gpt-5.5'}]\n")
            + TEXT("thinking_options = [{'value': 'low', 'name': 'Low', 'description': 'Short reasoning'}, {'value': 'medium', 'name': 'Medium', 'description': 'Balanced reasoning'}, {'value': 'high', 'name': 'High', 'description': 'Deep reasoning'}]\n")
            + TEXT("agent_options = [{'value': 'build', 'name': 'Build', 'description': 'Default OpenCode agent'}, {'value': 'unreal-agent', 'name': 'Unreal Agent', 'description': 'Live Unreal editor specialist'}]\n")
            + TEXT("expected_mcp_url = 'http://127.0.0.1:43123/mcp'\n")
            + TEXT("expected_mcp_token = 'fake-capability-token'\n")
            + TEXT("print('fake ACP stdout banner', flush=True)\n")
            + TEXT("def send(obj):\n")
            + TEXT("    print(json.dumps(obj, separators=(',', ':')), flush=True)\n")
            + TEXT("def config_result(model=None, agent=None, thinking=None):\n")
            + TEXT("    global current_model, current_agent, current_thinking\n")
            + TEXT("    if model is not None:\n")
            + TEXT("        current_model = model\n")
            + TEXT("    if agent is not None:\n")
            + TEXT("        current_agent = agent\n")
            + TEXT("    if thinking is not None:\n")
            + TEXT("        current_thinking = thinking\n")
            + TEXT("    return {'configOptions': [{'configOptionId': 'preferred_model', 'name': 'Current Model', 'category': 'current_model', 'optionValue': current_model, 'options': model_options}, {'id': 'effort', 'name': 'Thinking', 'category': 'thought_level', 'currentValue': current_thinking, 'options': thinking_options}, {'id': 'mode', 'name': 'Mode', 'currentValue': current_agent, 'options': agent_options}]}\n")
            + TEXT("def validate_mcp_servers(params):\n")
            + TEXT("    servers = params.get('mcpServers', [])\n")
            + TEXT("    if not isinstance(servers, list) or len(servers) != 1:\n")
            + TEXT("        return 'expected one mcpServers entry'\n")
            + TEXT("    server = servers[0]\n")
            + TEXT("    if server.get('type') != 'http' or server.get('name') != 'unreal-engine' or server.get('url') != expected_mcp_url:\n")
            + TEXT("        return 'unexpected mcp server shape: ' + json.dumps(server, sort_keys=True)\n")
            + TEXT("    headers = server.get('headers', [])\n")
            + TEXT("    if {'name': 'X-MCP-Capability-Token', 'value': expected_mcp_token} not in headers:\n")
            + TEXT("        return 'missing capability token header'\n")
            + TEXT("    return None\n")
            + TEXT("def prompt_text(msg):\n")
            + TEXT("    prompt = msg.get('params', {}).get('prompt', [])\n")
            + TEXT("    return ''.join([part.get('text', '') for part in prompt if isinstance(part, dict)])\n")
            + TEXT("def request_permission(text):\n")
            + TEXT("    global permission_id\n")
            + TEXT("    permission_id += 1\n")
            + TEXT("    if text == 'no options path':\n")
            + TEXT("        send({'jsonrpc': '2.0', 'id': permission_id, 'method': 'session/request_permission', 'params': {'sessionId': session_id, 'toolCall': {'toolCallId': 'fake-tool', 'status': 'pending', 'title': 'fake permission', 'kind': 'execute', 'rawInput': {'prompt': text}}, 'options': []}})\n")
            + TEXT("        return\n")
            + TEXT("    if text == 'mismatched always path':\n")
            + TEXT("        send({'jsonrpc': '2.0', 'id': permission_id, 'method': 'session/request_permission', 'params': {'sessionId': session_id, 'toolCall': {'toolCallId': 'fake-tool', 'status': 'pending', 'title': 'fake permission', 'kind': 'execute', 'rawInput': {'prompt': text}}, 'options': [{'optionId': 'always', 'kind': 'reject_once', 'name': 'Reject'}]}})\n")
            + TEXT("        return\n")
            + TEXT("    send({'jsonrpc': '2.0', 'id': permission_id, 'method': 'session/request_permission', 'params': {'sessionId': session_id, 'toolCall': {'toolCallId': 'fake-tool', 'status': 'pending', 'title': 'fake permission', 'kind': 'execute', 'rawInput': {'prompt': text}}, 'options': [{'optionId': 'once', 'kind': 'allow_once', 'name': 'Allow once'}, {'optionId': 'always', 'kind': 'allow_always', 'name': 'Always allow'}, {'optionId': 'reject', 'kind': 'reject_once', 'name': 'Reject'}]}})\n")
            + TEXT("for line in sys.stdin:\n")
            + TEXT("    if not line.strip():\n")
            + TEXT("        continue\n")
            + TEXT("    msg = json.loads(line)\n")
            + TEXT("    method = msg.get('method')\n")
            + TEXT("    msg_id = msg.get('id')\n")
            + TEXT("    if method == 'initialize':\n")
            + TEXT("        send({'jsonrpc': '2.0', 'id': msg_id, 'result': {'protocolVersion': 1}})\n")
            + TEXT("    elif method == 'session/new':\n")
            + TEXT("        mcp_error = validate_mcp_servers(msg.get('params', {}))\n")
            + TEXT("        if mcp_error is not None:\n")
            + TEXT("            send({'jsonrpc': '2.0', 'id': msg_id, 'error': {'code': -32602, 'message': mcp_error}})\n")
            + TEXT("            continue\n")
            + TEXT("        result = {'sessionId': session_id}\n")
            + TEXT("        result.update(config_result())\n")
            + TEXT("        send({'jsonrpc': '2.0', 'id': msg_id, 'result': result})\n")
            + TEXT("    elif method == 'session/set_config_option':\n")
            + TEXT("        params = msg.get('params', {})\n")
            + TEXT("        config_id = params.get('configId', params.get('id', ''))\n")
            + TEXT("        value = params.get('value', '')\n")
            + TEXT("        if config_id == 'mode':\n")
            + TEXT("            send({'jsonrpc': '2.0', 'id': msg_id, 'result': config_result(agent=value)})\n")
            + TEXT("        elif config_id == 'effort':\n")
            + TEXT("            send({'jsonrpc': '2.0', 'id': msg_id, 'result': config_result(thinking=value)})\n")
            + TEXT("        elif config_id in ('model', 'preferred_model'):\n")
            + TEXT("            result = config_result(model=value)\n")
            + TEXT("            update = dict(result)\n")
            + TEXT("            update['sessionUpdate'] = 'config_option_update'\n")
            + TEXT("            send({'jsonrpc': '2.0', 'method': 'session/update', 'params': {'sessionId': session_id, 'update': update}})\n")
            + TEXT("            send({'jsonrpc': '2.0', 'id': msg_id, 'result': result})\n")
            + TEXT("        else:\n")
            + TEXT("            send({'jsonrpc': '2.0', 'id': msg_id, 'error': {'code': -32602, 'message': 'unknown config option'}})\n")
            + TEXT("    elif method == 'session/prompt':\n")
            + TEXT("        prompt_id = msg_id\n")
            + TEXT("        text = prompt_text(msg)\n")
            + TEXT("        send({'jsonrpc': '2.0', 'method': 'session/update', 'params': {'sessionId': session_id, 'update': {'sessionUpdate': 'agent_message_chunk', 'content': {'type': 'text', 'text': 'fake response: ' + text}}}})\n")
            + TEXT("        send({'jsonrpc': '2.0', 'method': 'session/update', 'params': {'sessionId': session_id, 'update': {'sessionUpdate': 'agent_thought_chunk', 'content': {'type': 'text', 'text': 'thinking about ' + text}}}})\n")
            + TEXT("        send({'jsonrpc': '2.0', 'method': 'session/update', 'params': {'sessionId': session_id, 'update': {'sessionUpdate': 'tool_call', 'title': 'fake tool call'}}})\n")
            + TEXT("        send({'jsonrpc': '2.0', 'method': 'session/update', 'params': {'sessionId': session_id, 'update': {'sessionUpdate': 'tool_call_update', 'title': 'fake tool call', 'status': 'completed'}}})\n")
            + TEXT("        send({'jsonrpc': '2.0', 'method': 'session/update', 'params': {'sessionId': session_id, 'update': {'sessionUpdate': 'usage_update', 'used': 32000, 'size': 64000}}})\n")
            + TEXT("        send({'jsonrpc': '2.0', 'method': 'session/update', 'params': {'sessionId': session_id, 'update': {'sessionUpdate': 'tool_call', 'toolCallId': 'read-a', 'title': 'read', 'kind': 'read', 'rawInput': {'filePath': '/Game/SourceA.cpp'}, 'locations': [{'path': '/Game/SourceA.cpp'}]}}})\n")
            + TEXT("        send({'jsonrpc': '2.0', 'method': 'session/update', 'params': {'sessionId': session_id, 'update': {'sessionUpdate': 'tool_call_update', 'toolCallId': 'read-a', 'title': 'read', 'status': 'completed'}}})\n")
            + TEXT("        send({'jsonrpc': '2.0', 'method': 'session/update', 'params': {'sessionId': session_id, 'update': {'sessionUpdate': 'tool_call', 'toolCallId': 'read-b', 'title': 'read', 'kind': 'read', 'rawInput': {'path': '/Game/SourceB.cpp'}}}})\n")
            + TEXT("        send({'jsonrpc': '2.0', 'method': 'session/update', 'params': {'sessionId': session_id, 'update': {'sessionUpdate': 'tool_call_update', 'toolCallId': 'read-b', 'title': 'read', 'status': 'failed'}}})\n")
            + TEXT("        request_permission(text)\n")
            + TEXT("        if text == 'error after permission path':\n")
            + TEXT("            send({'jsonrpc': '2.0', 'id': prompt_id, 'error': {'code': -32000, 'message': 'fake prompt error'}})\n")
            + TEXT("            prompt_id = None\n")
            + TEXT("            continue\n")
            + TEXT("        if text == 'no options path':\n")
            + TEXT("            send({'jsonrpc': '2.0', 'id': prompt_id, 'result': {'stopReason': 'end_turn'}})\n")
            + TEXT("            prompt_id = None\n")
            + TEXT("    elif method == 'session/cancel':\n")
            + TEXT("        if prompt_id is not None:\n")
            + TEXT("            send({'jsonrpc': '2.0', 'id': prompt_id, 'result': {'stopReason': 'cancelled'}})\n")
            + TEXT("            prompt_id = None\n")
            + TEXT("    elif msg_id is not None and 'result' in msg:\n")
            + TEXT("        outcome = msg.get('result', {}).get('outcome', {})\n")
            + TEXT("        if outcome.get('outcome') == 'selected' and prompt_id is not None:\n")
            + TEXT("            send({'jsonrpc': '2.0', 'method': 'session/update', 'params': {'sessionId': session_id, 'update': {'sessionUpdate': 'agent_message_chunk', 'content': {'type': 'text', 'text': ' permission option ' + outcome.get('optionId', '')}}}})\n")
            + TEXT("            send({'jsonrpc': '2.0', 'id': prompt_id, 'result': {'stopReason': 'end_turn'}})\n")
            + TEXT("            prompt_id = None\n")
            + TEXT("        elif outcome.get('outcome') == 'cancelled':\n")
            + TEXT("            cancelled_permission = True\n");
    }

    bool PumpClientUntil(FOpenCodeAcpClient& Client, TFunctionRef<bool()> Predicate, double TimeoutSeconds = 5.0)
    {
        const double StartedAt = FPlatformTime::Seconds();
        while (FPlatformTime::Seconds() - StartedAt < TimeoutSeconds)
        {
            Client.Tick();
            if (Predicate())
            {
                return true;
            }
            FPlatformProcess::Sleep(0.01f);
        }

        Client.Tick();
        return Predicate();
    }

    bool ContainsTranscript(const TArray<FString>& Entries, const FString& ExpectedText)
    {
        return Entries.ContainsByPredicate([&ExpectedText](const FString& Entry)
        {
            return Entry.Contains(ExpectedText);
        });
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealAgentAcpClientProtocolTest,
    "UnrealAgent.Acp.ClientProtocol",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealAgentAcpClientProtocolTest::RunTest(const FString& Parameters)
{
#if !(PLATFORM_LINUX || PLATFORM_MAC)
    AddInfo(TEXT("Skipping fake ACP process harness on this platform."));
    return true;
#else
    const FString PythonExecutable = TEXT("/usr/bin/python3");
    if (!FPaths::FileExists(PythonExecutable))
    {
        AddInfo(TEXT("Skipping fake ACP process harness because /usr/bin/python3 is unavailable."));
        return true;
    }

    const FString TestDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentAcpHarness")));
    IFileManager::Get().DeleteDirectory(*TestDirectory, false, true);
    if (!IFileManager::Get().MakeDirectory(*TestDirectory, true))
    {
        AddError(FString::Printf(TEXT("Failed to create fake ACP harness directory: %s"), *TestDirectory));
        return false;
    }

    const FString ScriptPath = FPaths::Combine(TestDirectory, TEXT("acp"));
    if (!FFileHelper::SaveStringToFile(MakeFakeAcpScript(), *ScriptPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        AddError(FString::Printf(TEXT("Failed to write fake ACP script: %s"), *ScriptPath));
        return false;
    }
    IFileManager::Get().SetTimeStamp(*ScriptPath, FDateTime::Now());

    int32 ChmodReturnCode = 1;
    FString ChmodOutput;
    FPlatformProcess::ExecProcess(TEXT("/bin/chmod"), *FString::Printf(TEXT("+x \"%s\""), *ScriptPath), &ChmodReturnCode, &ChmodOutput, &ChmodOutput);
    if (ChmodReturnCode != 0)
    {
        AddError(FString::Printf(TEXT("Failed to make fake ACP script executable: %s"), *ChmodOutput));
        return false;
    }

    const FString PreviousOpenCodeCommand = FPlatformMisc::GetEnvironmentVariable(TEXT("OPENCODE_ACP_COMMAND"));
    FPlatformMisc::SetEnvironmentVar(TEXT("OPENCODE_ACP_COMMAND"), *ScriptPath);

    bool bPreviousNativeMcpEnabled = false;
    int32 PreviousNativeMcpPort = 0;
    FString PreviousListenHost;
    bool bPreviousRequireCapabilityToken = false;
    FString PreviousCapabilityToken;
    const bool bHadNativeMcpEnabled = GConfig != nullptr && GConfig->GetBool(TestAutomationBridgeSettingsSection, TEXT("bEnableNativeMCP"), bPreviousNativeMcpEnabled, GGameIni);
    const bool bHadNativeMcpPort = GConfig != nullptr && GConfig->GetInt(TestAutomationBridgeSettingsSection, TEXT("NativeMCPPort"), PreviousNativeMcpPort, GGameIni);
    const bool bHadListenHost = GConfig != nullptr && GConfig->GetString(TestAutomationBridgeSettingsSection, TEXT("ListenHost"), PreviousListenHost, GGameIni);
    const bool bHadRequireCapabilityToken = GConfig != nullptr && GConfig->GetBool(TestAutomationBridgeSettingsSection, TEXT("bRequireCapabilityToken"), bPreviousRequireCapabilityToken, GGameIni);
    const bool bHadCapabilityToken = GConfig != nullptr && GConfig->GetString(TestAutomationBridgeSettingsSection, TEXT("CapabilityToken"), PreviousCapabilityToken, GGameIni);
    if (GConfig != nullptr)
    {
        GConfig->SetBool(TestAutomationBridgeSettingsSection, TEXT("bEnableNativeMCP"), true, GGameIni);
        GConfig->SetInt(TestAutomationBridgeSettingsSection, TEXT("NativeMCPPort"), 43123, GGameIni);
        GConfig->SetString(TestAutomationBridgeSettingsSection, TEXT("ListenHost"), TEXT("0.0.0.0"), GGameIni);
        GConfig->SetBool(TestAutomationBridgeSettingsSection, TEXT("bRequireCapabilityToken"), true, GGameIni);
        GConfig->SetString(TestAutomationBridgeSettingsSection, TEXT("CapabilityToken"), TEXT("fake-capability-token"), GGameIni);
    }

    FOpenCodeAcpClient Client;
    FString LastStatus;
    FString LastPermission;
    TArray<FString> TranscriptEntries;
    int32 ModelChangeCount = 0;
    int32 StopCount = 0;

    Client.OnStatus.BindLambda([&LastStatus](const FString& Status)
    {
        LastStatus = Status;
    });
    Client.OnTranscript.BindLambda([&TranscriptEntries](const FString& Role, const FString& Text)
    {
        TranscriptEntries.Add(Role + TEXT(":") + Text);
    });
    Client.OnPermission.BindLambda([&LastPermission](const FString& Description)
    {
        LastPermission = Description;
    });
    Client.OnModelsChanged.BindLambda([&ModelChangeCount]()
    {
        ++ModelChangeCount;
    });
    Client.OnStopped.BindLambda([&StopCount]()
    {
        ++StopCount;
    });

    bool bPassed = true;
    bool bClientStarted = false;
    {
        FScopedIgnoredSigPwrForTest IgnoredSigPwr;
        bClientStarted = Client.Start(TestDirectory);
    }
    if (TestTrue(TEXT("Fake ACP client starts with ignored parent SIGPWR"), bClientStarted))
    {
        bPassed &= TestEqual(TEXT("Starting the client does not emit a stopped callback"), StopCount, 0);
        FString GeneratedAgentPrompt;
        const FString GeneratedAgentPromptPath = FPaths::Combine(TestDirectory, TEXT(".opencode/agents/unreal-agent.md"));
        bPassed &= TestTrue(TEXT("Generated Unreal Agent prompt is written"), FPaths::FileExists(GeneratedAgentPromptPath) && FFileHelper::LoadFileToString(GeneratedAgentPrompt, *GeneratedAgentPromptPath));
        bPassed &= TestTrue(TEXT("Generated prompt has current version marker"), GeneratedAgentPrompt.Contains(TEXT("unreal_agent_prompt_version: 2")));
        bPassed &= TestTrue(TEXT("Generated prompt has Studio Kit marker"), GeneratedAgentPrompt.Contains(TEXT("unreal_agent_studio_kit_version: 1")));
        bPassed &= TestTrue(TEXT("Generated prompt includes MCP tool playbook"), GeneratedAgentPrompt.Contains(TEXT("MCP tool playbook")));
        bPassed &= TestTrue(TEXT("Generated prompt includes release workflow"), GeneratedAgentPrompt.Contains(TEXT("Shipping: confirm packaging readiness")));
        bPassed &= TestTrue(TEXT("Generated validation skill is written"), FPaths::FileExists(FPaths::Combine(TestDirectory, TEXT(".opencode/skills/unreal-validation-loop/SKILL.md"))));
        bPassed &= TestTrue(TEXT("Generated guardrails plugin is written"), FPaths::FileExists(FPaths::Combine(TestDirectory, TEXT(".opencode/plugins/unreal-agent-guardrails.ts"))));
        bPassed &= TestTrue(TEXT("Generated OpenCode command is written"), FPaths::FileExists(FPaths::Combine(TestDirectory, TEXT(".opencode/commands/unreal-validate.md"))));
        FString GeneratedOpenCodeConfig;
        const FString GeneratedOpenCodeConfigPath = FPaths::Combine(TestDirectory, TEXT(".opencode/opencode.json"));
        bPassed &= TestTrue(TEXT("Generated OpenCode config is written"), FPaths::FileExists(GeneratedOpenCodeConfigPath) && FFileHelper::LoadFileToString(GeneratedOpenCodeConfig, *GeneratedOpenCodeConfigPath));
        bPassed &= TestTrue(TEXT("Generated OpenCode config has Studio Kit comment marker"), GeneratedOpenCodeConfig.Contains(FUnrealAgentStudioKit::GetStudioKitVersionMarker()));
        bPassed &= TestFalse(TEXT("Generated OpenCode config does not contain unknown Studio Kit keys"), GeneratedOpenCodeConfig.Contains(TEXT("\"unreal_agent_studio_kit_version\"")));
        bPassed &= TestTrue(TEXT("Studio Kit summary is exposed"), Client.GetLastStudioKitSummary().Contains(TEXT("Studio Kit:")));

        const FString ContextEnvelope = Client.RefreshEditorContext();
        bPassed &= TestTrue(TEXT("Client builds an editor context envelope"), ContextEnvelope.Contains(TEXT("<unreal_editor_context")));
        bPassed &= TestTrue(TEXT("Client stores editor context summary"), Client.GetLastEditorContextSummary().Contains(TEXT("Editor context:")));

        bPassed &= TestTrue(TEXT("Client validation can run"), Client.RunProjectValidation());
        bPassed &= TestTrue(TEXT("Client stores validation summary"), Client.GetLastValidationSummary().Contains(TEXT("Validation passed")));
        Client.SetAttachEditorContext(false);

        bPassed &= TestTrue(TEXT("Fake ACP client becomes ready"), PumpClientUntil(Client, [&Client]() { return Client.IsReady(); }));
        bPassed &= TestEqual(TEXT("Fake ACP session id parsed"), Client.GetSessionId(), FString(TEXT("fake-session")));
        bPassed &= TestEqual(TEXT("Initial model parsed"), Client.GetCurrentModel(), FString(TEXT("model-a")));
        bPassed &= TestEqual(TEXT("Initial thinking parsed"), Client.GetCurrentThinking(), FString(TEXT("medium")));
        bPassed &= TestEqual(TEXT("Model options parsed"), Client.GetModelOptions().Num(), 3);
        bPassed &= TestEqual(TEXT("Thinking options parsed"), Client.GetThinkingOptions().Num(), 3);
        bPassed &= TestEqual(TEXT("Agent options parsed"), Client.GetAgentOptions().Num(), 2);
        bPassed &= TestTrue(TEXT("Default Unreal agent selection completes"), PumpClientUntil(Client, [&Client]() { return Client.GetCurrentAgent() == TEXT("unreal-agent"); }));
        if (Client.GetAgentOptions().Num() == 2)
        {
            bPassed &= TestEqual(TEXT("Unreal agent id remains the ACP id"), Client.GetAgentOptions()[1].Id, FString(TEXT("unreal-agent")));
            bPassed &= TestEqual(TEXT("Unreal agent display name is user-facing"), Client.GetAgentOptions()[1].GetDisplayName(), FString(TEXT("Unreal - Creator")));
        }
        FOpenCodeAcpAgentOption RawUnrealAgentOption;
        RawUnrealAgentOption.Id = TEXT("unreal-agent");
        bPassed &= TestEqual(TEXT("Raw unreal-agent id displays as Unreal Creator"), RawUnrealAgentOption.GetDisplayName(), FString(TEXT("Unreal - Creator")));
        if (Client.GetModelOptions().Num() == 3)
        {
            bPassed &= TestEqual(TEXT("First model provider parsed"), Client.GetModelOptions()[0].Provider, FString(TEXT("NVIDIA")));
            bPassed &= TestEqual(TEXT("First model context window parsed"), Client.GetModelOptions()[0].ContextWindowTokens, 128000);
            bPassed &= TestEqual(TEXT("Second model provider parsed"), Client.GetModelOptions()[1].Provider, FString(TEXT("Modal")));
            bPassed &= TestEqual(TEXT("Second model context window parsed"), Client.GetModelOptions()[1].ContextWindowTokens, 64000);
            bPassed &= TestEqual(TEXT("Slash model provider is derived"), Client.GetModelOptions()[2].GetProviderName(), FString(TEXT("openai")));
            bPassed &= TestEqual(TEXT("Slash model display strips provider"), Client.GetModelOptions()[2].GetDisplayName(), FString(TEXT("gpt-5.5")));
        }
        if (Client.GetThinkingOptions().Num() == 3)
        {
            bPassed &= TestEqual(TEXT("Second thinking option id parsed"), Client.GetThinkingOptions()[1].Id, FString(TEXT("medium")));
            bPassed &= TestEqual(TEXT("Second thinking option display parsed"), Client.GetThinkingOptions()[1].GetDisplayName(), FString(TEXT("Medium")));
        }
        bPassed &= TestTrue(TEXT("Model changes delegate fired"), ModelChangeCount > 0);

        Client.SetThinking(TEXT("high"));
        bPassed &= TestTrue(TEXT("Thinking switch completes"), PumpClientUntil(Client, [&Client]() { return Client.GetCurrentThinking() == TEXT("high") && Client.CanSelectThinking(); }));

        Client.SetModel(TEXT("model-b"));
        bPassed &= TestTrue(TEXT("Model switch completes"), PumpClientUntil(Client, [&Client]() { return Client.GetCurrentModel() == TEXT("model-b") && Client.CanSelectModel(); }));

        Client.SendPrompt(TEXT("approve once path"));
        bPassed &= TestTrue(TEXT("Permission request arrives for allow once"), PumpClientUntil(Client, [&Client]() { return Client.HasPendingPermission(); }));
        bPassed &= TestTrue(TEXT("Context usage update is parsed"), Client.HasContextWindowUsage());
        bPassed &= TestEqual(TEXT("Context used tokens parsed"), Client.GetContextWindowUsedTokens(), 32000);
        bPassed &= TestEqual(TEXT("Context size tokens parsed"), Client.GetContextWindowSizeTokens(), 64000);
        bPassed &= TestTrue(TEXT("Permission description includes fake tool title"), LastPermission.Contains(TEXT("fake permission")));
        Client.ApprovePermissionOnce();
        bPassed &= TestTrue(TEXT("Allow once prompt completes"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));
        bPassed &= TestFalse(TEXT("Allow once clears pending permission"), Client.HasPendingPermission());
        bPassed &= TestTrue(TEXT("Allow once selected option reached fake ACP"), ContainsTranscript(TranscriptEntries, TEXT("permission option once")));
        bPassed &= TestTrue(TEXT("Reasoning transcript is emitted"), ContainsTranscript(TranscriptEntries, TEXT("Thought:thinking about approve once path")));
        bPassed &= TestTrue(TEXT("Tool call start transcript is emitted"), ContainsTranscript(TranscriptEntries, TEXT("Tool:Started fake tool call")));
        bPassed &= TestTrue(TEXT("Tool call completed transcript is emitted"), ContainsTranscript(TranscriptEntries, TEXT("Tool:fake tool call completed")));
        bPassed &= TestTrue(TEXT("Structured tool call start includes locations"), ContainsTranscript(TranscriptEntries, TEXT("Tool:Started read: /Game/SourceA.cpp")));
        bPassed &= TestTrue(TEXT("Structured tool call update reuses saved location"), ContainsTranscript(TranscriptEntries, TEXT("Tool:read: /Game/SourceA.cpp completed")));
        bPassed &= TestTrue(TEXT("Structured failed tool call preserves failure state"), ContainsTranscript(TranscriptEntries, TEXT("Tool:read: /Game/SourceB.cpp failed")));

        Client.SetModel(TEXT("model-a"));
        bPassed &= TestTrue(TEXT("Model switch after usage completes"), PumpClientUntil(Client, [&Client]() { return Client.GetCurrentModel() == TEXT("model-a") && Client.CanSelectModel(); }));
        bPassed &= TestFalse(TEXT("Context usage resets after model config update"), Client.HasContextWindowUsage());

        Client.SendPrompt(TEXT("approve always path"));
        bPassed &= TestTrue(TEXT("Permission request arrives for always allow"), PumpClientUntil(Client, [&Client]() { return Client.HasPendingPermission(); }));
        bPassed &= TestTrue(TEXT("Always allow option is available"), Client.CanApprovePermissionAlways());
        Client.ApprovePermissionAlways();
        bPassed &= TestTrue(TEXT("Always allow prompt completes"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));
        bPassed &= TestFalse(TEXT("Always allow clears pending permission"), Client.HasPendingPermission());
        bPassed &= TestTrue(TEXT("Always allow selected option reached fake ACP"), ContainsTranscript(TranscriptEntries, TEXT("permission option always")));

        Client.SendPrompt(TEXT("cancel path"));
        bPassed &= TestTrue(TEXT("Permission request arrives before cancel"), PumpClientUntil(Client, [&Client]() { return Client.HasPendingPermission(); }));
        Client.CancelPrompt();
        bPassed &= TestTrue(TEXT("Cancel request is tracked"), Client.IsCancelRequested());
        bPassed &= TestTrue(TEXT("Cancelled prompt completes"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));
        bPassed &= TestFalse(TEXT("Cancel clears pending permission"), Client.HasPendingPermission());
        bPassed &= TestFalse(TEXT("Cancel requested flag resets after prompt response"), Client.IsCancelRequested());
        bPassed &= TestTrue(TEXT("Cancel status reports cancelled stop reason"), LastStatus.Contains(TEXT("cancelled")));

        Client.SendPrompt(TEXT("post cancel path"));
        bPassed &= TestTrue(TEXT("Session survives cancel and requests permission again"), PumpClientUntil(Client, [&Client]() { return Client.HasPendingPermission(); }));
        Client.RejectPermission();
        bPassed &= TestTrue(TEXT("Post-cancel prompt completes"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));
        bPassed &= TestFalse(TEXT("Post-cancel prompt clears pending permission"), Client.HasPendingPermission());
        bPassed &= TestTrue(TEXT("Post-cancel prompt reaches fake ACP"), ContainsTranscript(TranscriptEntries, TEXT("fake response: post cancel path")));

        Client.SendPrompt(TEXT("mismatched always path"));
        bPassed &= TestTrue(TEXT("Permission request arrives for mismatched always option"), PumpClientUntil(Client, [&Client]() { return Client.HasPendingPermission(); }));
        bPassed &= TestFalse(TEXT("Mismatched always option is not available"), Client.CanApprovePermissionAlways());
        Client.RejectPermission();
        bPassed &= TestTrue(TEXT("Mismatched option prompt completes after reject"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));
        bPassed &= TestFalse(TEXT("Mismatched option clears pending permission"), Client.HasPendingPermission());

        Client.SendPrompt(TEXT("error after permission path"));
        bPassed &= TestTrue(TEXT("Prompt error completes"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));
        bPassed &= TestFalse(TEXT("Prompt error clears pending permission"), Client.HasPendingPermission());
        bPassed &= TestTrue(TEXT("Prompt error is reported"), ContainsTranscript(TranscriptEntries, TEXT("fake prompt error")));

        Client.SendPrompt(TEXT("no options path"));
        bPassed &= TestTrue(TEXT("Malformed permission prompt completes"), PumpClientUntil(Client, [&Client]() { return !Client.IsPromptInFlight(); }));
        bPassed &= TestFalse(TEXT("Malformed permission does not stay pending"), Client.HasPendingPermission());
        bPassed &= TestTrue(TEXT("Malformed permission is reported"), ContainsTranscript(TranscriptEntries, TEXT("no selectable options")));
    }
    else
    {
        bPassed = false;
    }

    Client.Stop();
    bPassed &= TestEqual(TEXT("Stopped callback fires only when an active client stops"), StopCount, bClientStarted ? 1 : 0);
    if (GConfig != nullptr)
    {
        if (bHadNativeMcpEnabled)
        {
            GConfig->SetBool(TestAutomationBridgeSettingsSection, TEXT("bEnableNativeMCP"), bPreviousNativeMcpEnabled, GGameIni);
        }
        else
        {
            GConfig->RemoveKey(TestAutomationBridgeSettingsSection, TEXT("bEnableNativeMCP"), GGameIni);
        }
        if (bHadNativeMcpPort)
        {
            GConfig->SetInt(TestAutomationBridgeSettingsSection, TEXT("NativeMCPPort"), PreviousNativeMcpPort, GGameIni);
        }
        else
        {
            GConfig->RemoveKey(TestAutomationBridgeSettingsSection, TEXT("NativeMCPPort"), GGameIni);
        }
        if (bHadListenHost)
        {
            GConfig->SetString(TestAutomationBridgeSettingsSection, TEXT("ListenHost"), *PreviousListenHost, GGameIni);
        }
        else
        {
            GConfig->RemoveKey(TestAutomationBridgeSettingsSection, TEXT("ListenHost"), GGameIni);
        }
        if (bHadRequireCapabilityToken)
        {
            GConfig->SetBool(TestAutomationBridgeSettingsSection, TEXT("bRequireCapabilityToken"), bPreviousRequireCapabilityToken, GGameIni);
        }
        else
        {
            GConfig->RemoveKey(TestAutomationBridgeSettingsSection, TEXT("bRequireCapabilityToken"), GGameIni);
        }
        if (bHadCapabilityToken)
        {
            GConfig->SetString(TestAutomationBridgeSettingsSection, TEXT("CapabilityToken"), *PreviousCapabilityToken, GGameIni);
        }
        else
        {
            GConfig->RemoveKey(TestAutomationBridgeSettingsSection, TEXT("CapabilityToken"), GGameIni);
        }
    }
    FPlatformMisc::SetEnvironmentVar(TEXT("OPENCODE_ACP_COMMAND"), *PreviousOpenCodeCommand);
    IFileManager::Get().DeleteDirectory(*TestDirectory, false, true);
    return bPassed;
#endif
}

#endif
