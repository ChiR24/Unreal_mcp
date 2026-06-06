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

void SUnrealAgentPanel::UpdateActiveChatSummary(const FString& Role, const FString& Text, bool bCountAsTranscriptEntry)
{
    if (Text.TrimStartAndEnd().IsEmpty())
    {
        return;
    }
    if (bRestoringChatHistory)
    {
        return;
    }

    EnsureActiveChatEntry(Text, IsUserTranscriptRole(Role));

    FChatHistoryEntry* ActiveEntry = ChatHistoryEntries.FindByPredicate([this](const FChatHistoryEntry& Entry)
    {
        return Entry.Id == ActiveChatHistoryId;
    });
    if (ActiveEntry == nullptr)
    {
        return;
    }

    const FString SummaryText = RenderTranscriptText(Role, Text).TrimStartAndEnd();
    const FString TranscriptText = ClampTranscriptText(Text);
    if (IsUserTranscriptRole(Role) && !ActiveEntry->bHasCustomTitle)
    {
        ActiveEntry->Title = MakeChatTitleFromPrompt(SummaryText);
    }

    FString PreviewPrefix;
    if (IsUserTranscriptRole(Role))
    {
        PreviewPrefix = TEXT("You: ");
    }
    else if (Role == TEXT("OpenCode"))
    {
        PreviewPrefix = TEXT("OpenCode: ");
    }
    else
    {
        PreviewPrefix = Role + TEXT(": ");
    }

    ActiveEntry->Preview = PreviewPrefix + SummaryText;
    if (ActiveEntry->Preview.Len() > 96)
    {
        ActiveEntry->Preview = ActiveEntry->Preview.Left(93) + TEXT("...");
    }
    if (bCountAsTranscriptEntry)
    {
        const bool bDuplicateSeededUserPrompt = IsUserTranscriptRole(Role)
            && ActiveEntry->EntryCount == 1
            && !ActiveEntry->TranscriptEntries.IsEmpty()
            && IsUserTranscriptRole(ActiveEntry->TranscriptEntries[0].Role)
            && ActiveEntry->TranscriptEntries[0].Text == TranscriptText;
        if (bDuplicateSeededUserPrompt)
        {
            ActiveEntry->LastSummaryRole = Role;
            ActiveEntry->LastSummaryCharacters = SummaryText.Len();
            SaveChatHistory();
            RebuildChatHistoryList();
            return;
        }

        FChatTranscriptEntry TranscriptEntry;
        TranscriptEntry.Role = Role;
        TranscriptEntry.Text = TranscriptText;
        ActiveEntry->TranscriptEntries.Add(MoveTemp(TranscriptEntry));
        while (ActiveEntry->TranscriptEntries.Num() > MaxTranscriptEntries)
        {
            ActiveEntry->TranscriptEntries.RemoveAt(0);
        }

        ++ActiveEntry->EntryCount;
        ActiveEntry->ContextCharacters += SummaryText.Len();
        ActiveEntry->LastSummaryRole = Role;
        ActiveEntry->LastSummaryCharacters = SummaryText.Len();
    }
    else if (ActiveEntry->LastSummaryRole == Role)
    {
        if (!ActiveEntry->TranscriptEntries.IsEmpty() && ActiveEntry->TranscriptEntries.Last().Role == Role)
        {
            ActiveEntry->TranscriptEntries.Last().Text = TranscriptText;
        }
        ActiveEntry->ContextCharacters += FMath::Max(0, SummaryText.Len() - ActiveEntry->LastSummaryCharacters);
        ActiveEntry->LastSummaryCharacters = SummaryText.Len();
    }
    SaveChatHistory();
    RebuildChatHistoryList();
}

#undef LOCTEXT_NAMESPACE
