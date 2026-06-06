#include "SUnrealAgentPanel.h"
#include "SUnrealAgentPanelPrivate.h"

#include "../Acp/McpOpenCodeAcpClient.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SUnrealAgentPanel"

using namespace UnrealAgent::Panel;

FReply SUnrealAgentPanel::OnChatHistoryEntryClicked(int32 EntryId)
{
    if (AcpClient.IsValid() && AcpClient->IsPromptInFlight())
    {
        return FReply::Handled();
    }

    FlushPendingTranscript(true);
    StoreActiveContextWindowUsage();

    const FChatHistoryEntry* Entry = ChatHistoryEntries.FindByPredicate([EntryId](const FChatHistoryEntry& Candidate)
    {
        return Candidate.Id == EntryId;
    });
    if (Entry != nullptr)
    {
        RestoreChatHistoryEntry(*Entry);
    }

    return FReply::Handled();
}

FReply SUnrealAgentPanel::OnChatHistoryRenameClicked(int32 EntryId)
{
    if (AcpClient.IsValid() && AcpClient->IsPromptInFlight())
    {
        return FReply::Handled();
    }

    const FChatHistoryEntry* Entry = ChatHistoryEntries.FindByPredicate([EntryId](const FChatHistoryEntry& Candidate)
    {
        return Candidate.Id == EntryId;
    });
    if (Entry == nullptr)
    {
        return FReply::Handled();
    }

    RenamingChatHistoryId = EntryId;
    PendingRenameTitle = Entry->Title;
    RebuildChatHistoryList();
    return FReply::Handled();
}

void SUnrealAgentPanel::OnChatHistoryRenameTextChanged(const FText& NewText, int32 EntryId)
{
    if (RenamingChatHistoryId == EntryId)
    {
        PendingRenameTitle = NewText.ToString();
    }
}

FReply SUnrealAgentPanel::OnChatHistoryRenameSaveClicked(int32 EntryId)
{
    FChatHistoryEntry* Entry = ChatHistoryEntries.FindByPredicate([EntryId](const FChatHistoryEntry& Candidate)
    {
        return Candidate.Id == EntryId;
    });
    if (Entry != nullptr)
    {
        Entry->Title = MakeChatTitleFromPrompt(PendingRenameTitle);
        Entry->bHasCustomTitle = true;
        SaveChatHistory();
    }

    RenamingChatHistoryId = INDEX_NONE;
    PendingRenameTitle.Reset();
    RebuildChatHistoryList();
    return FReply::Handled();
}

FReply SUnrealAgentPanel::OnChatHistoryRenameCancelClicked()
{
    RenamingChatHistoryId = INDEX_NONE;
    PendingRenameTitle.Reset();
    RebuildChatHistoryList();
    return FReply::Handled();
}

FReply SUnrealAgentPanel::OnChatHistoryDeleteClicked(int32 EntryId)
{
    if (AcpClient.IsValid() && AcpClient->IsPromptInFlight())
    {
        return FReply::Handled();
    }

    if (ActiveChatHistoryId == EntryId)
    {
        StoreActiveContextWindowUsage();
    }

    const int32 RemovedCount = ChatHistoryEntries.RemoveAll([EntryId](const FChatHistoryEntry& Entry)
    {
        return Entry.Id == EntryId;
    });
    if (RemovedCount == 0)
    {
        return FReply::Handled();
    }

    if (RenamingChatHistoryId == EntryId)
    {
        RenamingChatHistoryId = INDEX_NONE;
        PendingRenameTitle.Reset();
    }
    if (ActiveChatHistoryId == EntryId)
    {
        ResetTranscriptView();
        ActiveChatHistoryId = INDEX_NONE;
        LastUserPrompt.Reset();
    }

    SaveChatHistory();
    RebuildChatHistoryList();
    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
