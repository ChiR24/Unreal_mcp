#if WITH_DEV_AUTOMATION_TESTS

#include "UnrealAgentAutomationWidgetHelpers.h"
#include "UnrealAgentPanelAutomation.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "UI/SUnrealAgentPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace UnrealAgent::AutomationTests
{
    namespace
    {
        TSharedPtr<SButton> FindButtonInHistoryContainer(const TSharedPtr<SWidget>& Container, const TCHAR* ButtonTag)
        {
            const TSharedPtr<SWidget> ButtonWidget = Container.IsValid()
                ? FindWidgetByTag(Container.ToSharedRef(), FName(ButtonTag))
                : nullptr;
            return ButtonWidget.IsValid() ? StaticCastSharedPtr<SButton>(ButtonWidget) : nullptr;
        }
    }

    bool VerifyPanelHistoryInteractions(FAutomationTestBase& Test, FPanelTestFixture& Fixture)
    {
        const TSharedRef<SWidget> Content = Fixture.Content();
        const TSharedRef<SUnrealAgentPanel> Panel = Fixture.PanelRef();
        bool bPassed = true;

        const TSharedPtr<SWidget> NewChatWidget = FindWidgetByTag(Content, FName(TEXT("UnrealAgent.Sidebar.NewChatButton")));
        const TSharedPtr<SButton> NewChatButton = NewChatWidget.IsValid() ? StaticCastSharedPtr<SButton>(NewChatWidget) : nullptr;
        if (NewChatButton.IsValid())
        {
            ClickSlateButton(NewChatButton);
        }
        bPassed &= Test.TestTrue(TEXT("New Chat clears the visible transcript"), !FindWidgetByTag(Content, FName(TEXT("UnrealAgent.Transcript.UserBubble"))).IsValid() && !FindWidgetByTag(Content, FName(TEXT("UnrealAgent.Transcript.AssistantText"))).IsValid());
        bPassed &= Test.TestEqual(TEXT("New Chat resets context usage to an empty chat"), Panel->GetContextWindowStatusTextForAutomation(), FString(TEXT("0% used")));
        bPassed &= Test.TestTrue(TEXT("New Chat keeps the previous chat in sidebar history"), FindWidgetByTag(Content, FName(TEXT("UnrealAgent.Sidebar.History.Row"))).IsValid());

        Panel->AddTranscriptEntryForAutomation(TEXT("User"), TEXT("second chat"));
        Panel->AddTranscriptEntryForAutomation(TEXT("OpenCode"), TEXT("second answer"));
        Panel->SetActiveContextWindowUsageForAutomation(32000, 64000);
        bPassed &= Test.TestEqual(TEXT("Second chat creates another sidebar history entry"), Panel->GetChatHistoryCountForAutomation(), 2);
        bPassed &= Test.TestEqual(TEXT("Second chat shows its own context usage"), Panel->GetContextWindowStatusTextForAutomation(), FString(TEXT("50% used")));

        const TSharedPtr<SButton> SavedHistoryButton = FindHistoryButtonByTitle(Content, TEXT("hi"));
        bPassed &= Test.TestTrue(TEXT("Sidebar history row is clickable"), SavedHistoryButton.IsValid());
        if (SavedHistoryButton.IsValid())
        {
            ClickSlateButton(SavedHistoryButton);
        }

        const TSharedPtr<SWidget> RestoredUserText = FindWidgetByTag(Content, FName(TEXT("UnrealAgent.Transcript.UserText")));
        const TSharedPtr<STextBlock> RestoredUserTextBlock = RestoredUserText.IsValid() ? StaticCastSharedPtr<STextBlock>(RestoredUserText) : nullptr;
        TArray<TSharedPtr<SWidget>> RestoredAssistantTexts;
        FindWidgetsByTag(Content, FName(TEXT("UnrealAgent.Transcript.AssistantText")), RestoredAssistantTexts);
        TArray<TSharedPtr<SWidget>> RestoredWorkingHeaderWidgets;
        FindWidgetsByTag(Content, FName(TEXT("UnrealAgent.Transcript.Working.Header")), RestoredWorkingHeaderWidgets);
        const TSharedPtr<SWidget> RestoredWorkingTextWidget = FindWidgetByTag(Content, FName(TEXT("UnrealAgent.Transcript.Working.Text")));
        const TSharedPtr<SWidget> RestoredToolGroupBodyWidget = FindWidgetByTag(Content, FName(TEXT("UnrealAgent.Transcript.ToolGroup.Body")));
        const TSharedPtr<STextBlock> RestoredWorkingTextBlock = RestoredWorkingTextWidget.IsValid() ? StaticCastSharedPtr<STextBlock>(RestoredWorkingTextWidget) : nullptr;
        const TSharedPtr<STextBlock> RestoredToolGroupBodyText = RestoredToolGroupBodyWidget.IsValid() ? StaticCastSharedPtr<STextBlock>(RestoredToolGroupBodyWidget) : nullptr;
        const bool bRestoredLatestAssistantText = RestoredAssistantTexts.ContainsByPredicate([](const TSharedPtr<SWidget>& AssistantWidget)
        {
            const TSharedPtr<STextBlock> AssistantText = AssistantWidget.IsValid() ? StaticCastSharedPtr<STextBlock>(AssistantWidget) : nullptr;
            return AssistantText.IsValid() && AssistantText->GetText().ToString().Contains(TEXT("next answer streamed tail"));
        });
        bPassed &= Test.TestTrue(TEXT("Clicking a history row restores the user transcript"), FindWidgetByTag(Content, FName(TEXT("UnrealAgent.Transcript.UserBubble"))).IsValid());
        bPassed &= Test.TestTrue(TEXT("Clicking a history row restores the user text"), RestoredUserTextBlock.IsValid() && RestoredUserTextBlock->GetText().ToString() == TEXT("hi"));
        bPassed &= Test.TestEqual(TEXT("Clicking a history row restores assistant turns as separate chat rows"), RestoredAssistantTexts.Num(), 2);
        bPassed &= Test.TestTrue(TEXT("Clicking a history row restores the assistant transcript"), bRestoredLatestAssistantText);
        bPassed &= Test.TestTrue(TEXT("Clicking a history row restores working activity rows"), FindWidgetByTag(Content, FName(TEXT("UnrealAgent.Transcript.Working"))).IsValid() && RestoredWorkingHeaderWidgets.Num() >= 2);
        bPassed &= Test.TestTrue(TEXT("Clicking a history row restores reasoning activity text"), RestoredWorkingTextBlock.IsValid() && RestoredWorkingTextBlock->GetText().ToString().Contains(TEXT("thinking about the answer")));
        bPassed &= Test.TestTrue(TEXT("Clicking a history row restores tool activity groups"), RestoredToolGroupBodyText.IsValid() && RestoredToolGroupBodyText->GetText().ToString().Contains(TEXT("/Game/SourceA.cpp")));
        bPassed &= Test.TestTrue(TEXT("Clicking a history row marks it active again"), FindWidgetByTag(Content, FName(TEXT("UnrealAgent.Sidebar.History.ActiveTitle"))).IsValid());
        bPassed &= Test.TestEqual(TEXT("Clicking a history row restores that chat's context usage"), Panel->GetContextWindowStatusTextForAutomation(), FString(TEXT("5% used")));

        const TSharedPtr<SButton> RestoredRenameButton = FindButtonInHistoryContainer(FindHistoryContainerByTitle(Content, TEXT("hi")), TEXT("UnrealAgent.Sidebar.History.RenameButton"));
        bPassed &= Test.TestTrue(TEXT("Restored history row exposes rename action"), RestoredRenameButton.IsValid());
        if (RestoredRenameButton.IsValid())
        {
            ClickSlateButton(RestoredRenameButton);
        }

        const TSharedPtr<SWidget> CancelRenameInputWidget = FindWidgetByTag(Content, FName(TEXT("UnrealAgent.Sidebar.History.RenameInput")));
        const TSharedPtr<SButton> CancelRenameButton = FindButtonInHistoryContainer(Content, TEXT("UnrealAgent.Sidebar.History.RenameCancelButton"));
        bPassed &= Test.TestTrue(TEXT("Rename mode shows an editable title input"), CancelRenameInputWidget.IsValid());
        bPassed &= Test.TestTrue(TEXT("Rename mode shows a cancel option"), CancelRenameButton.IsValid());
        if (CancelRenameButton.IsValid())
        {
            ClickSlateButton(CancelRenameButton);
        }
        bPassed &= Test.TestTrue(TEXT("Cancelling rename keeps the original title"), FindHistoryContainerByTitle(Content, TEXT("hi")).IsValid());
        bPassed &= Test.TestFalse(TEXT("Cancelling rename hides the edit input"), FindWidgetByTag(Content, FName(TEXT("UnrealAgent.Sidebar.History.RenameInput"))).IsValid());

        const TSharedPtr<SButton> RenameButton = FindButtonInHistoryContainer(FindHistoryContainerByTitle(Content, TEXT("hi")), TEXT("UnrealAgent.Sidebar.History.RenameButton"));
        if (RenameButton.IsValid())
        {
            ClickSlateButton(RenameButton);
        }

        const TSharedPtr<SWidget> RenameInputWidget = FindWidgetByTag(Content, FName(TEXT("UnrealAgent.Sidebar.History.RenameInput")));
        const TSharedPtr<SWidget> RenameSaveButtonWidget = FindWidgetByTag(Content, FName(TEXT("UnrealAgent.Sidebar.History.RenameSaveButton")));
        const TSharedPtr<SEditableTextBox> RenameInput = RenameInputWidget.IsValid() ? StaticCastSharedPtr<SEditableTextBox>(RenameInputWidget) : nullptr;
        const TSharedPtr<SButton> RenameSaveButton = RenameSaveButtonWidget.IsValid() ? StaticCastSharedPtr<SButton>(RenameSaveButtonWidget) : nullptr;
        bPassed &= Test.TestTrue(TEXT("Rename mode shows a save option"), RenameSaveButton.IsValid());
        if (RenameInput.IsValid())
        {
            RenameInput->SetText(FText::FromString(TEXT("renamed chat")));
        }
        if (RenameSaveButton.IsValid())
        {
            ClickSlateButton(RenameSaveButton);
        }
        bPassed &= Test.TestTrue(TEXT("Saving rename updates the visible chat title"), FindHistoryContainerByTitle(Content, TEXT("renamed chat")).IsValid());
        bPassed &= Test.TestFalse(TEXT("Saving rename removes the previous chat title"), FindHistoryContainerByTitle(Content, TEXT("hi")).IsValid());

        Panel->AddTranscriptEntryForAutomation(TEXT("User"), TEXT("prompt after rename"));
        Panel->AddTranscriptEntryForAutomation(TEXT("OpenCode"), TEXT("answer after rename"));
        bPassed &= Test.TestTrue(TEXT("Manual chat rename survives continuing the active chat"), FindHistoryContainerByTitle(Content, TEXT("renamed chat")).IsValid());
        bPassed &= Test.TestFalse(TEXT("Manual chat rename is not replaced by later prompts"), FindHistoryContainerByTitle(Content, TEXT("prompt after rename")).IsValid());

        FString SavedChatHistoryJson;
        bPassed &= Test.TestTrue(TEXT("Sidebar history is written to isolated test storage"), FPaths::FileExists(Fixture.ChatHistoryPath) && FFileHelper::LoadFileToString(SavedChatHistoryJson, *Fixture.ChatHistoryPath));
        bPassed &= Test.TestTrue(TEXT("Saved sidebar history includes the renamed active chat title"), SavedChatHistoryJson.Contains(TEXT("renamed chat")));
        bPassed &= Test.TestTrue(TEXT("Saved sidebar history keeps transcript entries added after rename"), SavedChatHistoryJson.Contains(TEXT("prompt after rename")) && SavedChatHistoryJson.Contains(TEXT("answer after rename")));
        bPassed &= Test.TestTrue(TEXT("Saved sidebar history includes restorable transcript entries"), SavedChatHistoryJson.Contains(TEXT("\"Transcript\"")) && SavedChatHistoryJson.Contains(TEXT("next answer streamed tail")));
        bPassed &= Test.TestTrue(TEXT("Saved sidebar history includes visible activity entries"), SavedChatHistoryJson.Contains(TEXT("thinking about the answer")) && SavedChatHistoryJson.Contains(TEXT("/Game/SourceA.cpp")));

        const TSharedRef<SUnrealAgentPanel> ReloadedPanel = SNew(SUnrealAgentPanel);
        const TSharedRef<SWidget> ReloadedPanelWidget = StaticCastSharedRef<SWidget>(ReloadedPanel);
        bPassed &= Test.TestEqual(TEXT("Saved sidebar history reloads in a new panel instance"), ReloadedPanel->GetChatHistoryCountForAutomation(), 2);
        bPassed &= Test.TestTrue(TEXT("Saved sidebar history reloads the renamed previous chat"), FindHistoryContainerByTitle(ReloadedPanelWidget, TEXT("renamed chat")).IsValid());
        bPassed &= Test.TestTrue(TEXT("Saved sidebar history reloads the second saved chat"), FindHistoryContainerByTitle(ReloadedPanelWidget, TEXT("second chat")).IsValid());

        const TSharedPtr<SButton> DeleteButton = FindButtonInHistoryContainer(FindHistoryContainerByTitle(Content, TEXT("renamed chat")), TEXT("UnrealAgent.Sidebar.History.DeleteButton"));
        bPassed &= Test.TestTrue(TEXT("Renamed history row exposes delete action"), DeleteButton.IsValid());
        if (DeleteButton.IsValid())
        {
            ClickSlateButton(DeleteButton);
        }
        bPassed &= Test.TestEqual(TEXT("Deleting a chat removes it from sidebar history"), Panel->GetChatHistoryCountForAutomation(), 1);
        bPassed &= Test.TestTrue(TEXT("Deleting the active chat clears the visible transcript"), !FindWidgetByTag(Content, FName(TEXT("UnrealAgent.Transcript.UserBubble"))).IsValid() && !FindWidgetByTag(Content, FName(TEXT("UnrealAgent.Transcript.AssistantText"))).IsValid());

        FString DeletedChatHistoryJson;
        bPassed &= Test.TestTrue(TEXT("Deleting a chat persists the history file"), FPaths::FileExists(Fixture.ChatHistoryPath) && FFileHelper::LoadFileToString(DeletedChatHistoryJson, *Fixture.ChatHistoryPath));
        bPassed &= Test.TestFalse(TEXT("Deleted chat title is removed from persisted history"), DeletedChatHistoryJson.Contains(TEXT("renamed chat")));

        const bool bSidebarCollapsedBeforeToggle = Panel->IsSidebarCollapsedForAutomation();
        Panel->ToggleSidebarForAutomation();
        bPassed &= Test.TestEqual(TEXT("Sidebar toggle flips the sidebar collapsed state"), Panel->IsSidebarCollapsedForAutomation(), !bSidebarCollapsedBeforeToggle);
        Panel->ToggleSidebarForAutomation();
        bPassed &= Test.TestEqual(TEXT("Sidebar toggle restores the original sidebar state"), Panel->IsSidebarCollapsedForAutomation(), bSidebarCollapsedBeforeToggle);
        return bPassed;
    }
}

#endif
