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

void SUnrealAgentPanel::ResetTranscriptView()
{
    FlushPendingTranscript(true);
    if (TranscriptScrollBox.IsValid())
    {
        TranscriptScrollBox->ClearChildren();
    }

    TranscriptEntryWidgets.Reset();
    LastTranscriptTextBlock.Reset();
    ResetActiveActivityState();
    ActiveReasoningStartedSeconds.Reset();
    ActiveReasoningEndSeconds.Reset();
    ActiveActivityHasReasoning.Reset();
    ActiveActivityUpdateCount.Reset();
    LastTranscriptRole.Reset();
    LastActivityTranscriptRole.Reset();
    LastTranscriptText.Reset();
    PendingTranscriptRole.Reset();
    PendingTranscriptText.Reset();
    bHasConversationContent = false;
    ClearPromptTextBoxes();
}

void SUnrealAgentPanel::RestoreChatHistoryEntry(const FChatHistoryEntry& Entry)
{
    const int32 EntryId = Entry.Id;
    const FString EntryTitle = Entry.Title;
    const FString EntryPreview = Entry.Preview;
    const TArray<FChatTranscriptEntry> TranscriptEntries = Entry.TranscriptEntries;

    ResetTranscriptView();
    ActiveChatHistoryId = EntryId;
    LastUserPrompt.Reset();

    auto RestoreTranscriptEntry = [this](const FString& Role, const FString& Text)
    {
        if (!IsRestorableHistoryRole(Role))
        {
            return;
        }

        AddTranscriptEntryImmediately(Role, Text);
    };

    bRestoringChatHistory = true;
    if (TranscriptEntries.IsEmpty())
    {
        if (!EntryTitle.IsEmpty())
        {
            LastUserPrompt = EntryTitle;
            RestoreTranscriptEntry(TEXT("User"), EntryTitle);
        }

        FString PreviewText = EntryPreview;
        FString PreviewRole = TEXT("OpenCode");
        if (PreviewText.StartsWith(TEXT("You: ")))
        {
            PreviewRole = TEXT("User");
            PreviewText.RightChopInline(5, EAllowShrinking::No);
        }
        else if (PreviewText.StartsWith(TEXT("OpenCode: ")))
        {
            PreviewText.RightChopInline(10, EAllowShrinking::No);
        }
        if (!PreviewText.IsEmpty())
        {
            RestoreTranscriptEntry(PreviewRole, PreviewText);
        }
    }
    else
    {
        const bool bHasSavedUserPrompt = TranscriptEntries.ContainsByPredicate([](const FChatTranscriptEntry& TranscriptEntry)
        {
            return IsUserTranscriptRole(TranscriptEntry.Role);
        });
        if (!bHasSavedUserPrompt && !EntryTitle.IsEmpty())
        {
            LastUserPrompt = EntryTitle;
            RestoreTranscriptEntry(TEXT("User"), EntryTitle);
        }

        for (const FChatTranscriptEntry& TranscriptEntry : TranscriptEntries)
        {
            if (LastUserPrompt.IsEmpty() && IsUserTranscriptRole(TranscriptEntry.Role))
            {
                LastUserPrompt = TranscriptEntry.Text;
            }
            RestoreTranscriptEntry(TranscriptEntry.Role, TranscriptEntry.Text);
        }
    }
    bRestoringChatHistory = false;

    bHasConversationContent = !TranscriptEntries.IsEmpty() || !EntryTitle.IsEmpty() || !EntryPreview.IsEmpty();
    RebuildChatHistoryList();
}

void SUnrealAgentPanel::StoreActiveContextWindowUsage()
{
    if (!AcpClient.IsValid() || !AcpClient->HasContextWindowUsage() || ActiveChatHistoryId == INDEX_NONE)
    {
        return;
    }

    FChatHistoryEntry* ActiveEntry = ChatHistoryEntries.FindByPredicate([this](const FChatHistoryEntry& Entry)
    {
        return Entry.Id == ActiveChatHistoryId;
    });
    if (ActiveEntry == nullptr)
    {
        return;
    }

    const int32 UsedTokens = FMath::Max(0, AcpClient->GetContextWindowUsedTokens());
    const int32 SizeTokens = FMath::Max(1, AcpClient->GetContextWindowSizeTokens());
    if (ActiveEntry->ContextWindowUsedTokens == UsedTokens && ActiveEntry->ContextWindowSizeTokens == SizeTokens)
    {
        return;
    }

    ActiveEntry->ContextWindowUsedTokens = UsedTokens;
    ActiveEntry->ContextWindowSizeTokens = SizeTokens;
    SaveChatHistory();
}

void SUnrealAgentPanel::EnsureActiveChatEntry(const FString& SeedText, bool bSeedUserTranscript)
{
    if (ActiveChatHistoryId != INDEX_NONE)
    {
        return;
    }

    FChatHistoryEntry NewEntry;
    NewEntry.Id = NextChatHistoryId++;
    NewEntry.Title = MakeChatTitleFromPrompt(SeedText);
    NewEntry.Preview = SeedText.TrimStartAndEnd();
    if (NewEntry.Preview.Len() > 96)
    {
        NewEntry.Preview = NewEntry.Preview.Left(93) + TEXT("...");
    }

    if (bSeedUserTranscript && !NewEntry.Preview.IsEmpty())
    {
        FChatTranscriptEntry TranscriptEntry;
        TranscriptEntry.Role = TEXT("User");
        TranscriptEntry.Text = ClampTranscriptText(SeedText);
        NewEntry.TranscriptEntries.Add(MoveTemp(TranscriptEntry));
        NewEntry.EntryCount = 1;
        NewEntry.ContextCharacters = NewEntry.Preview.Len();
        NewEntry.LastSummaryRole = TEXT("User");
        NewEntry.LastSummaryCharacters = NewEntry.Preview.Len();
    }

    ActiveChatHistoryId = NewEntry.Id;
    ChatHistoryEntries.Add(MoveTemp(NewEntry));
    SaveChatHistory();
    RebuildChatHistoryList();
}

#undef LOCTEXT_NAMESPACE
