#if WITH_DEV_AUTOMATION_TESTS

#include "UnrealAgentAutomationWidgetHelpers.h"
#include "UnrealAgentPanelAutomation.h"

#include "Layout/Children.h"
#include "Misc/AutomationTest.h"
#include "UI/SUnrealAgentPanel.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/SWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace UnrealAgent::AutomationTests
{
    bool VerifyPanelTranscriptRendering(FAutomationTestBase& Test, FPanelTestFixture& Fixture)
    {
        const TSharedRef<SWidget> Content = Fixture.Content();
        const TSharedRef<SUnrealAgentPanel> Panel = Fixture.PanelRef();
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

        auto Find = [&Content](const TCHAR* Tag)
        {
            return FindWidgetByTag(Content, FName(Tag));
        };
        const TSharedPtr<SWidget> UserBubble = Find(TEXT("UnrealAgent.Transcript.UserBubble"));
        const TSharedPtr<SWidget> AssistantText = Find(TEXT("UnrealAgent.Transcript.AssistantText"));
        const TSharedPtr<SWidget> WorkingWidget = Find(TEXT("UnrealAgent.Transcript.Working"));
        const TSharedPtr<SWidget> WorkingHeaderWidget = Find(TEXT("UnrealAgent.Transcript.Working.Header"));
        const TSharedPtr<SWidget> ReasoningBubble = Find(TEXT("UnrealAgent.Transcript.ReasoningBubble"));
        const TSharedPtr<SWidget> ActivityText = Find(TEXT("UnrealAgent.Transcript.ActivityText"));
        const TSharedPtr<SWidget> WorkingTextWidget = Find(TEXT("UnrealAgent.Transcript.Working.Text"));
        const TSharedPtr<SWidget> ToolGroupWidget = Find(TEXT("UnrealAgent.Transcript.ToolGroup"));
        const TSharedPtr<SWidget> ToolGroupHeaderWidget = Find(TEXT("UnrealAgent.Transcript.ToolGroup.Header"));
        const TSharedPtr<SWidget> ToolGroupBodyWidget = Find(TEXT("UnrealAgent.Transcript.ToolGroup.Body"));
        const TSharedPtr<SWidget> HistoryRowContainer = Find(TEXT("UnrealAgent.Sidebar.History.RowContainer"));
        const TSharedPtr<SWidget> HistoryActiveTitle = Find(TEXT("UnrealAgent.Sidebar.History.ActiveTitle"));
        const TSharedPtr<SWidget> UpdatedContextStatus = Find(TEXT("UnrealAgent.Composer.ContextWindow.Status"));
        const TSharedPtr<SWidget> ComposerContextIndicator = Find(TEXT("UnrealAgent.Composer.ContextWindow.Indicator"));
        const TSharedPtr<SExpandableArea> WorkingArea = WorkingWidget.IsValid() ? StaticCastSharedPtr<SExpandableArea>(WorkingWidget) : nullptr;
        const TSharedPtr<SExpandableArea> ToolGroupArea = ToolGroupWidget.IsValid() ? StaticCastSharedPtr<SExpandableArea>(ToolGroupWidget) : nullptr;
        const TSharedPtr<STextBlock> WorkingHeaderText = WorkingHeaderWidget.IsValid() ? StaticCastSharedPtr<STextBlock>(WorkingHeaderWidget) : nullptr;
        const TSharedPtr<STextBlock> ToolGroupHeaderText = ToolGroupHeaderWidget.IsValid() ? StaticCastSharedPtr<STextBlock>(ToolGroupHeaderWidget) : nullptr;
        const TSharedPtr<STextBlock> ToolGroupBodyText = ToolGroupBodyWidget.IsValid() ? StaticCastSharedPtr<STextBlock>(ToolGroupBodyWidget) : nullptr;
        const TSharedPtr<STextBlock> WorkingTextBlock = WorkingTextWidget.IsValid() ? StaticCastSharedPtr<STextBlock>(WorkingTextWidget) : nullptr;
        const TSharedPtr<STextBlock> AssistantTextBlock = AssistantText.IsValid() ? StaticCastSharedPtr<STextBlock>(AssistantText) : nullptr;
        const TSharedPtr<STextBlock> HistoryTitleBlock = HistoryActiveTitle.IsValid() ? StaticCastSharedPtr<STextBlock>(HistoryActiveTitle) : nullptr;
        const TSharedPtr<STextBlock> UpdatedContextStatusBlock = UpdatedContextStatus.IsValid() ? StaticCastSharedPtr<STextBlock>(UpdatedContextStatus) : nullptr;
        FChildren* ContextIndicatorChildren = ComposerContextIndicator.IsValid() ? ComposerContextIndicator->GetAllChildren() : nullptr;
        const TSharedPtr<STextBlock> ContextIndicatorTextBlock = ContextIndicatorChildren != nullptr && ContextIndicatorChildren->Num() > 0
            ? StaticCastSharedRef<STextBlock>(ContextIndicatorChildren->GetChildAt(0)).ToSharedPtr()
            : nullptr;

        bool bPassed = true;
        bPassed &= Test.TestTrue(TEXT("User chat keeps a boxed bubble"), UserBubble.IsValid());
        bPassed &= Test.TestTrue(TEXT("Assistant chat renders as plain text"), AssistantText.IsValid());
        bPassed &= Test.TestTrue(TEXT("Sidebar history row container is tagged"), HistoryRowContainer.IsValid());
        bPassed &= Test.TestTrue(TEXT("Sidebar history adds a row for the active chat"), Find(TEXT("UnrealAgent.Sidebar.History.Row")).IsValid());
        bPassed &= Test.TestTrue(TEXT("Sidebar history row exposes rename option"), FindWidgetByTag(HistoryRowContainer.ToSharedRef(), FName(TEXT("UnrealAgent.Sidebar.History.RenameButton"))).IsValid());
        bPassed &= Test.TestTrue(TEXT("Sidebar history row uses a rename icon"), FindWidgetByTag(HistoryRowContainer.ToSharedRef(), FName(TEXT("UnrealAgent.Sidebar.History.RenameIcon"))).IsValid());
        bPassed &= Test.TestTrue(TEXT("Sidebar history row exposes delete option"), FindWidgetByTag(HistoryRowContainer.ToSharedRef(), FName(TEXT("UnrealAgent.Sidebar.History.DeleteButton"))).IsValid());
        bPassed &= Test.TestTrue(TEXT("Sidebar history row uses a delete icon"), FindWidgetByTag(HistoryRowContainer.ToSharedRef(), FName(TEXT("UnrealAgent.Sidebar.History.DeleteIcon"))).IsValid());
        bPassed &= Test.TestTrue(TEXT("Sidebar history uses the user prompt as the active chat title"), HistoryTitleBlock.IsValid() && HistoryTitleBlock->GetText().ToString() == TEXT("hi"));
        bPassed &= Test.TestFalse(TEXT("Sidebar history row is title-only without assistant preview"), Find(TEXT("UnrealAgent.Sidebar.History.Preview")).IsValid());
        bPassed &= Test.TestFalse(TEXT("Sidebar history row is title-only without entry count"), Find(TEXT("UnrealAgent.Sidebar.History.Count")).IsValid());
        bPassed &= Test.TestEqual(TEXT("Sidebar history stores one active chat entry"), Panel->GetChatHistoryCountForAutomation(), 1);

        const FString ContextStatusText = UpdatedContextStatusBlock.IsValid() ? UpdatedContextStatusBlock->GetText().ToString() : FString();
        const FString ContextIndicatorText = ContextIndicatorTextBlock.IsValid() ? ContextIndicatorTextBlock->GetText().ToString() : FString();
        bPassed &= Test.TestEqual(TEXT("Context status indicator renders a hollow circle glyph"), ContextIndicatorText, FString(TEXT("○")));
        bPassed &= Test.TestTrue(TEXT("Context window status shows only used percentage"), ContextStatusText.Contains(TEXT("used")) && !ContextStatusText.Contains(TEXT("free")) && !ContextStatusText.Contains(TEXT("remaining")) && !ContextStatusText.Contains(TEXT("/")) && !ContextStatusText.Contains(TEXT("Context window")));
        bPassed &= Test.TestEqual(TEXT("Context window status follows the active chat usage"), Panel->GetContextWindowStatusTextForAutomation(), FString(TEXT("5% used")));
        bPassed &= Test.TestTrue(TEXT("Assistant Markdown tables render as readable lines"), AssistantTextBlock.IsValid() && AssistantTextBlock->GetText().ToString().Contains(TEXT("Engine: UE 5.x")) && AssistantTextBlock->GetText().ToString().Contains(TEXT("Multiplayer: Ready")));
        bPassed &= Test.TestFalse(TEXT("Assistant Markdown table pipes are not shown raw"), AssistantTextBlock.IsValid() && AssistantTextBlock->GetText().ToString().Contains(TEXT("|")));
        bPassed &= Test.TestTrue(TEXT("Tool and reasoning activity collapse into working row"), WorkingArea.IsValid());
        bPassed &= Test.TestTrue(TEXT("Reasoning activity text remains visible"), WorkingTextBlock.IsValid() && WorkingTextBlock->GetText().ToString().Contains(TEXT("thinking about the answer")));
        bPassed &= Test.TestFalse(TEXT("Reasoning activity text does not show a redundant label"), WorkingTextBlock.IsValid() && WorkingTextBlock->GetText().ToString().Contains(TEXT("Reasoning")));
        bPassed &= Test.TestTrue(TEXT("Working header stays clean"), WorkingHeaderText.IsValid() && WorkingHeaderText->GetText().ToString().StartsWith(TEXT("Working")) && !WorkingHeaderText->GetText().ToString().Contains(TEXT("Reasoned")) && !WorkingHeaderText->GetText().ToString().Contains(TEXT("updates")) && !WorkingHeaderText->GetText().ToString().Contains(TEXT("...")));
        bPassed &= Test.TestTrue(TEXT("Repeated same-tool activity is grouped"), ToolGroupArea.IsValid());
        bPassed &= Test.TestTrue(TEXT("Tool group has a readable header"), ToolGroupHeaderText.IsValid() && ToolGroupHeaderText->GetText().ToString().Contains(TEXT("Read")) && ToolGroupHeaderText->GetText().ToString().Contains(TEXT("2")));
        bPassed &= Test.TestTrue(TEXT("Tool group body lists useful tool details"), ToolGroupBodyText.IsValid() && ToolGroupBodyText->GetText().ToString().Contains(TEXT("/Game/SourceA.cpp")) && ToolGroupBodyText->GetText().ToString().Contains(TEXT("/Game/SourceB.cpp")));
        bPassed &= Test.TestFalse(TEXT("Tool group body suppresses raw start/status logs"), ToolGroupBodyText.IsValid() && (ToolGroupBodyText->GetText().ToString().Contains(TEXT("Started")) || ToolGroupBodyText->GetText().ToString().Contains(TEXT("in_progress")) || ToolGroupBodyText->GetText().ToString().Contains(TEXT("completed"))));
        bPassed &= Test.TestFalse(TEXT("Reasoning no longer uses a boxed bubble"), ReasoningBubble.IsValid());
        bPassed &= Test.TestFalse(TEXT("Tool activity is not shown as loose transcript text"), ActivityText.IsValid());

        TArray<TSharedPtr<SWidget>> WorkingHeaderWidgets;
        FindWidgetsByTag(Content, FName(TEXT("UnrealAgent.Transcript.Working.Header")), WorkingHeaderWidgets);
        bPassed &= Test.TestTrue(TEXT("Each activity burst gets its own working row"), WorkingHeaderWidgets.Num() >= 2);
        for (const TSharedPtr<SWidget>& HeaderWidget : WorkingHeaderWidgets)
        {
            const TSharedPtr<STextBlock> HeaderTextBlock = HeaderWidget.IsValid() ? StaticCastSharedPtr<STextBlock>(HeaderWidget) : nullptr;
            const FString HeaderText = HeaderTextBlock.IsValid() ? HeaderTextBlock->GetText().ToString() : FString();
            bPassed &= Test.TestTrue(TEXT("Completed working rows keep elapsed text"), HeaderText.StartsWith(TEXT("Working")) && HeaderText.Contains(TEXT("sec")));
            bPassed &= Test.TestFalse(TEXT("Completed working rows stop animating"), HeaderText.Contains(TEXT("...")));
        }
        if (WorkingArea.IsValid())
        {
            bPassed &= Test.TestFalse(TEXT("Working activity starts collapsed"), WorkingArea->IsExpanded());
        }
        if (ToolGroupArea.IsValid())
        {
            bPassed &= Test.TestFalse(TEXT("Same-tool group starts collapsed"), ToolGroupArea->IsExpanded());
        }
        return bPassed;
    }
}

#endif
