#if WITH_DEV_AUTOMATION_TESTS

#include "UnrealAgentAutomationTestDelegates.h"
#include "UnrealAgentAutomationWidgetHelpers.h"
#include "UnrealAgentPanelAutomation.h"

#include "Misc/AutomationTest.h"
#include "UI/SUnrealAgentPanel.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SWidget.h"

namespace UnrealAgent::AutomationTests
{
    namespace
    {
        struct FRequiredWidget
        {
            const TCHAR* Message;
            const TCHAR* Tag;
        };
    }

    bool RunPanelSmokeTest(FAutomationTestBase& Test)
    {
        FPanelTestFixture Fixture;
        if (!Fixture.Open(Test))
        {
            return false;
        }

        bool bPassed = VerifyPanelLayout(Test, Fixture);
        bPassed &= VerifyPanelTranscriptRendering(Test, Fixture);
        bPassed &= VerifyPanelHistoryInteractions(Test, Fixture);
        return bPassed;
    }

    bool VerifyPanelLayout(FAutomationTestBase& Test, FPanelTestFixture& Fixture)
    {
        const TSharedRef<SWidget> Content = Fixture.Content();
        const TSharedRef<SUnrealAgentPanel> Panel = Fixture.PanelRef();
        auto Find = [&Content](const TCHAR* Tag)
        {
            return FindWidgetByTag(Content, FName(Tag));
        };

        const FRequiredWidget RequiredWidgets[] = {
            { TEXT("Unreal Agent layout is tagged"), TEXT("UnrealAgent.Layout") },
            { TEXT("Main chat column is tagged"), TEXT("UnrealAgent.MainColumn") },
            { TEXT("Sidebar is tagged"), TEXT("UnrealAgent.Sidebar") },
            { TEXT("Sidebar expanded state is tagged"), TEXT("UnrealAgent.Sidebar.Expanded") },
            { TEXT("Sidebar collapsed state is tagged"), TEXT("UnrealAgent.Sidebar.Collapsed") },
            { TEXT("Sidebar toggle button is tagged"), TEXT("UnrealAgent.Sidebar.ToggleButton") },
            { TEXT("Sidebar New Chat button is tagged"), TEXT("UnrealAgent.Sidebar.NewChatButton") },
            { TEXT("Sidebar history list is tagged"), TEXT("UnrealAgent.Sidebar.History.List") },
            { TEXT("Sidebar history starts with an empty state"), TEXT("UnrealAgent.Sidebar.History.Empty") },
            { TEXT("Unreal Agent header is tagged"), TEXT("UnrealAgent.Header") },
            { TEXT("Header title is tagged"), TEXT("UnrealAgent.Header.Title") },
            { TEXT("Connection indicator is tagged"), TEXT("UnrealAgent.Header.ConnectionIndicator") },
            { TEXT("Connect button is tagged in the header"), TEXT("UnrealAgent.Header.ConnectButton") },
            { TEXT("Agent cockpit is tagged"), TEXT("UnrealAgent.Cockpit") },
            { TEXT("Cockpit context toggle is tagged"), TEXT("UnrealAgent.Cockpit.ContextToggle") },
            { TEXT("Cockpit context preview is tagged"), TEXT("UnrealAgent.Cockpit.ContextPreview") },
            { TEXT("Cockpit inspect context button is tagged"), TEXT("UnrealAgent.Cockpit.InspectContextButton") },
            { TEXT("Cockpit validate button is tagged"), TEXT("UnrealAgent.Cockpit.ValidateButton") },
            { TEXT("Cockpit evidence status is tagged"), TEXT("UnrealAgent.Cockpit.EvidenceStatus") },
            { TEXT("Cockpit Studio Kit status is tagged"), TEXT("UnrealAgent.Cockpit.StudioKitStatus") },
            { TEXT("Initial composer is tagged"), TEXT("UnrealAgent.Composer.Center") },
            { TEXT("Composer footer is tagged"), TEXT("UnrealAgent.Composer.Footer") },
            { TEXT("Model controls row is tagged"), TEXT("UnrealAgent.Composer.ModelControls") },
            { TEXT("Composer context window status is tagged"), TEXT("UnrealAgent.Composer.ContextWindow") },
            { TEXT("Composer context status indicator is tagged"), TEXT("UnrealAgent.Composer.ContextWindow.Indicator") },
            { TEXT("Composer context status text is tagged"), TEXT("UnrealAgent.Composer.ContextWindow.Status") },
            { TEXT("Composer action row is tagged"), TEXT("UnrealAgent.Composer.ActionRow") },
            { TEXT("Empty state is tagged"), TEXT("UnrealAgent.EmptyState") },
            { TEXT("Quick prompt grid is tagged"), TEXT("UnrealAgent.EmptyState.QuickPromptGrid") },
            { TEXT("Architecture review quick prompt is tagged"), TEXT("UnrealAgent.EmptyState.QuickPrompt.ArchitectureReview") },
            { TEXT("Gameplay plan quick prompt is tagged"), TEXT("UnrealAgent.EmptyState.QuickPrompt.GameplayPlan") },
            { TEXT("QA risk pass quick prompt is tagged"), TEXT("UnrealAgent.EmptyState.QuickPrompt.QARiskPass") },
            { TEXT("Editor tooling quick prompt is tagged"), TEXT("UnrealAgent.EmptyState.QuickPrompt.EditorTooling") },
            { TEXT("Permission bar is tagged"), TEXT("UnrealAgent.PermissionBar") },
            { TEXT("Allow once permission button is tagged"), TEXT("UnrealAgent.Permission.AllowOnceButton") },
            { TEXT("Always allow permission button is tagged"), TEXT("UnrealAgent.Permission.AllowAlwaysButton") },
            { TEXT("Reject permission button is tagged"), TEXT("UnrealAgent.Permission.RejectButton") },
            { TEXT("Composer input frame is tagged"), TEXT("UnrealAgent.Composer.InputFrame") },
            { TEXT("Prompt input is tagged"), TEXT("UnrealAgent.Composer.Input") },
            { TEXT("Composer helper row is tagged"), TEXT("UnrealAgent.Composer.HelperRow") },
            { TEXT("Composer helper is tagged"), TEXT("UnrealAgent.Composer.Helper") },
            { TEXT("Send button is tagged"), TEXT("UnrealAgent.Composer.SendButton") },
            { TEXT("Model combo is tagged"), TEXT("UnrealAgent.Model.Combo") },
            { TEXT("Thinking combo is tagged"), TEXT("UnrealAgent.Thinking.Combo") },
            { TEXT("Agent combo is tagged"), TEXT("UnrealAgent.Agent.Combo") },
            { TEXT("Transcript scrollbox is tagged"), TEXT("UnrealAgent.Transcript.Scroll") },
        };

        bool bPassed = true;
        for (const FRequiredWidget& RequiredWidget : RequiredWidgets)
        {
            bPassed &= Test.TestTrue(RequiredWidget.Message, Find(RequiredWidget.Tag).IsValid());
        }
        if (!bPassed)
        {
            return false;
        }

        const TSharedPtr<SWidget> Layout = Find(TEXT("UnrealAgent.Layout"));
        const TSharedPtr<SWidget> MainColumn = Find(TEXT("UnrealAgent.MainColumn"));
        const TSharedPtr<SWidget> Header = Find(TEXT("UnrealAgent.Header"));
        const TSharedPtr<SWidget> SidebarExpanded = Find(TEXT("UnrealAgent.Sidebar.Expanded"));
        const TSharedPtr<SWidget> ComposerFooter = Find(TEXT("UnrealAgent.Composer.Footer"));
        const TSharedPtr<SWidget> ComposerModelControls = Find(TEXT("UnrealAgent.Composer.ModelControls"));
        const TSharedPtr<SWidget> ComposerActionRow = Find(TEXT("UnrealAgent.Composer.ActionRow"));
        const TSharedPtr<SWidget> ComposerHelperRow = Find(TEXT("UnrealAgent.Composer.HelperRow"));
        const TSharedPtr<SWidget> ComposerInputFrame = Find(TEXT("UnrealAgent.Composer.InputFrame"));
        const TSharedPtr<SWidget> PermissionBar = Find(TEXT("UnrealAgent.PermissionBar"));
        const TSharedPtr<SWidget> QuickPromptGrid = Find(TEXT("UnrealAgent.EmptyState.QuickPromptGrid"));

        int32 ModelComboIndex = INDEX_NONE;
        int32 ThinkingComboIndex = INDEX_NONE;
        int32 AgentComboIndex = INDEX_NONE;
        const bool bFoundModelComboIndex = FindWidgetTraversalIndexByTag(ComposerModelControls.ToSharedRef(), FName(TEXT("UnrealAgent.Model.Combo")), ModelComboIndex);
        const bool bFoundThinkingComboIndex = FindWidgetTraversalIndexByTag(ComposerModelControls.ToSharedRef(), FName(TEXT("UnrealAgent.Thinking.Combo")), ThinkingComboIndex);
        const bool bFoundAgentComboIndex = FindWidgetTraversalIndexByTag(ComposerModelControls.ToSharedRef(), FName(TEXT("UnrealAgent.Agent.Combo")), AgentComboIndex);

        bPassed &= Test.TestTrue(TEXT("Sidebar is inside the main layout"), FindWidgetByTag(Layout.ToSharedRef(), FName(TEXT("UnrealAgent.Sidebar"))).IsValid());
        bPassed &= Test.TestTrue(TEXT("Main chat column is inside the main layout"), FindWidgetByTag(Layout.ToSharedRef(), FName(TEXT("UnrealAgent.MainColumn"))).IsValid());
        bPassed &= Test.TestTrue(TEXT("Expanded sidebar owns the New Chat button"), FindWidgetByTag(SidebarExpanded.ToSharedRef(), FName(TEXT("UnrealAgent.Sidebar.NewChatButton"))).IsValid());
        bPassed &= Test.TestTrue(TEXT("Expanded sidebar owns the history list"), FindWidgetByTag(SidebarExpanded.ToSharedRef(), FName(TEXT("UnrealAgent.Sidebar.History.List"))).IsValid());
        bPassed &= Test.TestFalse(TEXT("Expanded sidebar does not own context window status"), FindWidgetByTag(SidebarExpanded.ToSharedRef(), FName(TEXT("UnrealAgent.Sidebar.ContextWindow"))).IsValid());
        bPassed &= Test.TestTrue(TEXT("Composer footer owns context window status"), FindWidgetByTag(ComposerFooter.ToSharedRef(), FName(TEXT("UnrealAgent.Composer.ContextWindow"))).IsValid());
        bPassed &= Test.TestTrue(TEXT("Composer footer owns context status indicator"), FindWidgetByTag(ComposerFooter.ToSharedRef(), FName(TEXT("UnrealAgent.Composer.ContextWindow.Indicator"))).IsValid());
        bPassed &= Test.TestFalse(TEXT("Context status is hidden before ACP model load"), Panel->IsContextWindowVisibleForAutomation());
        bPassed &= Test.TestTrue(TEXT("Transcript scrollbox stays in the main chat column"), FindWidgetByTag(MainColumn.ToSharedRef(), FName(TEXT("UnrealAgent.Transcript.Scroll"))).IsValid());
        bPassed &= Test.TestTrue(TEXT("Connect button is inside the header row"), FindWidgetByTag(Header.ToSharedRef(), FName(TEXT("UnrealAgent.Header.ConnectButton"))).IsValid());
        bPassed &= Test.TestFalse(TEXT("Connect button is not inside the composer footer"), FindWidgetByTag(ComposerFooter.ToSharedRef(), FName(TEXT("UnrealAgent.Header.ConnectButton"))).IsValid());
        bPassed &= Test.TestTrue(TEXT("Cockpit is inside the main chat column"), FindWidgetByTag(MainColumn.ToSharedRef(), FName(TEXT("UnrealAgent.Cockpit"))).IsValid());
        bPassed &= Test.TestTrue(TEXT("Quick prompt buttons are inside the quick prompt grid"), FindWidgetByTag(QuickPromptGrid.ToSharedRef(), FName(TEXT("UnrealAgent.EmptyState.QuickPrompt.ArchitectureReview"))).IsValid() && FindWidgetByTag(QuickPromptGrid.ToSharedRef(), FName(TEXT("UnrealAgent.EmptyState.QuickPrompt.GameplayPlan"))).IsValid() && FindWidgetByTag(QuickPromptGrid.ToSharedRef(), FName(TEXT("UnrealAgent.EmptyState.QuickPrompt.QARiskPass"))).IsValid() && FindWidgetByTag(QuickPromptGrid.ToSharedRef(), FName(TEXT("UnrealAgent.EmptyState.QuickPrompt.EditorTooling"))).IsValid());
        bPassed &= Test.TestTrue(TEXT("Model controls row is inside the composer footer"), FindWidgetByTag(ComposerFooter.ToSharedRef(), FName(TEXT("UnrealAgent.Composer.ModelControls"))).IsValid());
        bPassed &= Test.TestTrue(TEXT("Action row is inside the composer footer"), FindWidgetByTag(ComposerFooter.ToSharedRef(), FName(TEXT("UnrealAgent.Composer.ActionRow"))).IsValid());
        bPassed &= Test.TestTrue(TEXT("Model combo is inside the model row"), FindWidgetByTag(ComposerModelControls.ToSharedRef(), FName(TEXT("UnrealAgent.Model.Combo"))).IsValid());
        bPassed &= Test.TestTrue(TEXT("Thinking combo is inside the model row"), FindWidgetByTag(ComposerModelControls.ToSharedRef(), FName(TEXT("UnrealAgent.Thinking.Combo"))).IsValid());
        bPassed &= Test.TestTrue(TEXT("Agent combo is inside the model row"), FindWidgetByTag(ComposerModelControls.ToSharedRef(), FName(TEXT("UnrealAgent.Agent.Combo"))).IsValid());
        bPassed &= Test.TestTrue(TEXT("Agent combo appears before the model combo"), bFoundAgentComboIndex && bFoundModelComboIndex && AgentComboIndex < ModelComboIndex);
        bPassed &= Test.TestTrue(TEXT("Thinking combo appears after the model combo"), bFoundModelComboIndex && bFoundThinkingComboIndex && ModelComboIndex < ThinkingComboIndex);
        bPassed &= Test.TestTrue(TEXT("Context window status is inside the right side of the action row"), FindWidgetByTag(ComposerActionRow.ToSharedRef(), FName(TEXT("UnrealAgent.Composer.ContextWindow"))).IsValid());
        bPassed &= Test.TestFalse(TEXT("Context window status is not inside the model controls"), FindWidgetByTag(ComposerModelControls.ToSharedRef(), FName(TEXT("UnrealAgent.Composer.ContextWindow"))).IsValid());
        bPassed &= Test.TestTrue(TEXT("Composer helper row is inside the composer footer"), FindWidgetByTag(ComposerFooter.ToSharedRef(), FName(TEXT("UnrealAgent.Composer.HelperRow"))).IsValid());
        bPassed &= Test.TestFalse(TEXT("Composer helper is below the text box, not inside the action row"), FindWidgetByTag(ComposerActionRow.ToSharedRef(), FName(TEXT("UnrealAgent.Composer.HelperRow"))).IsValid());
        bPassed &= Test.TestTrue(TEXT("Composer helper is in the bottom centered helper row"), FindWidgetByTag(ComposerHelperRow.ToSharedRef(), FName(TEXT("UnrealAgent.Composer.Helper"))).IsValid());
        bPassed &= Test.TestFalse(TEXT("Composer helper is separated from the action row"), FindWidgetByTag(ComposerActionRow.ToSharedRef(), FName(TEXT("UnrealAgent.Composer.Helper"))).IsValid());
        bPassed &= Test.TestTrue(TEXT("Send button is inside the composer input frame"), FindWidgetByTag(ComposerInputFrame.ToSharedRef(), FName(TEXT("UnrealAgent.Composer.SendButton"))).IsValid());
        bPassed &= Test.TestFalse(TEXT("Send button is not inside the action row"), FindWidgetByTag(ComposerActionRow.ToSharedRef(), FName(TEXT("UnrealAgent.Composer.SendButton"))).IsValid());
        bPassed &= Test.TestTrue(TEXT("Permission actions are inside the permission bar"), FindWidgetByTag(PermissionBar.ToSharedRef(), FName(TEXT("UnrealAgent.Permission.AllowOnceButton"))).IsValid() && FindWidgetByTag(PermissionBar.ToSharedRef(), FName(TEXT("UnrealAgent.Permission.AllowAlwaysButton"))).IsValid() && FindWidgetByTag(PermissionBar.ToSharedRef(), FName(TEXT("UnrealAgent.Permission.RejectButton"))).IsValid());
        bPassed &= Test.TestEqual(TEXT("Unreal Agent opens as a dockable Level Editor panel tab"), Fixture.Tab->GetTabRole(), ETabRole::PanelTab);
        return bPassed;
    }
}

#endif
