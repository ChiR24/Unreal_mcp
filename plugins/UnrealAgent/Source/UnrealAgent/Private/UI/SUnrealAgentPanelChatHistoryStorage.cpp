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

void SUnrealAgentPanel::LoadChatHistory()
{
    ChatHistoryEntries.Reset();
    ActiveChatHistoryId = INDEX_NONE;
    NextChatHistoryId = 1;

    FString HistoryJson;
    if (!FFileHelper::LoadFileToString(HistoryJson, *GetChatHistoryStoragePath()))
    {
        return;
    }

    TSharedPtr<FJsonObject> RootObject;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(HistoryJson);
    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>* EntriesJson = nullptr;
    if (!RootObject->TryGetArrayField(TEXT("entries"), EntriesJson))
    {
        return;
    }

    int32 MaxEntryId = 0;
    for (const TSharedPtr<FJsonValue>& EntryValue : *EntriesJson)
    {
        const TSharedPtr<FJsonObject> EntryObject = EntryValue.IsValid() ? EntryValue->AsObject() : nullptr;
        if (!EntryObject.IsValid())
        {
            continue;
        }

        auto ReadIntField = [&EntryObject](const TCHAR* FieldName) -> int32
        {
            int32 Value = 0;
            return EntryObject->TryGetNumberField(FieldName, Value) ? FMath::Max(0, Value) : 0;
        };

        FChatHistoryEntry Entry;
        Entry.Id = ReadIntField(TEXT("Id"));
        EntryObject->TryGetStringField(TEXT("Title"), Entry.Title);
        EntryObject->TryGetStringField(TEXT("Preview"), Entry.Preview);
        EntryObject->TryGetStringField(TEXT("LastSummaryRole"), Entry.LastSummaryRole);
        EntryObject->TryGetBoolField(TEXT("HasCustomTitle"), Entry.bHasCustomTitle);
        Entry.EntryCount = ReadIntField(TEXT("EntryCount"));
        Entry.ContextCharacters = ReadIntField(TEXT("ContextCharacters"));
        Entry.ContextWindowUsedTokens = ReadIntField(TEXT("ContextWindowUsedTokens"));
        Entry.ContextWindowSizeTokens = ReadIntField(TEXT("ContextWindowSizeTokens"));
        Entry.LastSummaryCharacters = ReadIntField(TEXT("LastSummaryCharacters"));

        const TArray<TSharedPtr<FJsonValue>>* TranscriptJson = nullptr;
        if (EntryObject->TryGetArrayField(TEXT("Transcript"), TranscriptJson))
        {
            for (const TSharedPtr<FJsonValue>& TranscriptValue : *TranscriptJson)
            {
                const TSharedPtr<FJsonObject> TranscriptObject = TranscriptValue.IsValid() ? TranscriptValue->AsObject() : nullptr;
                if (!TranscriptObject.IsValid())
                {
                    continue;
                }

                FChatTranscriptEntry TranscriptEntry;
                TranscriptObject->TryGetStringField(TEXT("Role"), TranscriptEntry.Role);
                TranscriptObject->TryGetStringField(TEXT("Text"), TranscriptEntry.Text);
                if (!IsRestorableHistoryRole(TranscriptEntry.Role) || TranscriptEntry.Text.IsEmpty())
                {
                    continue;
                }

                TranscriptEntry.Text = ClampTranscriptText(TranscriptEntry.Text);
                Entry.TranscriptEntries.Add(MoveTemp(TranscriptEntry));
                if (Entry.TranscriptEntries.Num() >= MaxTranscriptEntries)
                {
                    break;
                }
            }
        }

        if (Entry.Id <= 0 || Entry.Title.IsEmpty())
        {
            continue;
        }

        MaxEntryId = FMath::Max(MaxEntryId, Entry.Id);
        ChatHistoryEntries.Add(MoveTemp(Entry));
    }

    NextChatHistoryId = FMath::Max(1, MaxEntryId + 1);
}

void SUnrealAgentPanel::SaveChatHistory() const
{
    const FString HistoryPath = GetChatHistoryStoragePath();
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(HistoryPath), true);

    TArray<TSharedPtr<FJsonValue>> EntriesJson;
    for (const FChatHistoryEntry& Entry : ChatHistoryEntries)
    {
        TSharedRef<FJsonObject> EntryObject = MakeShared<FJsonObject>();
        EntryObject->SetNumberField(TEXT("Id"), Entry.Id);
        EntryObject->SetStringField(TEXT("Title"), Entry.Title);
        EntryObject->SetStringField(TEXT("Preview"), Entry.Preview);
        EntryObject->SetStringField(TEXT("LastSummaryRole"), Entry.LastSummaryRole);
        EntryObject->SetBoolField(TEXT("HasCustomTitle"), Entry.bHasCustomTitle);
        EntryObject->SetNumberField(TEXT("EntryCount"), Entry.EntryCount);
        EntryObject->SetNumberField(TEXT("ContextCharacters"), Entry.ContextCharacters);
        EntryObject->SetNumberField(TEXT("ContextWindowUsedTokens"), Entry.ContextWindowUsedTokens);
        EntryObject->SetNumberField(TEXT("ContextWindowSizeTokens"), Entry.ContextWindowSizeTokens);
        EntryObject->SetNumberField(TEXT("LastSummaryCharacters"), Entry.LastSummaryCharacters);

        TArray<TSharedPtr<FJsonValue>> TranscriptJson;
        for (const FChatTranscriptEntry& TranscriptEntry : Entry.TranscriptEntries)
        {
            if (!IsRestorableHistoryRole(TranscriptEntry.Role) || TranscriptEntry.Text.IsEmpty())
            {
                continue;
            }

            TSharedRef<FJsonObject> TranscriptObject = MakeShared<FJsonObject>();
            TranscriptObject->SetStringField(TEXT("Role"), TranscriptEntry.Role);
            TranscriptObject->SetStringField(TEXT("Text"), TranscriptEntry.Text);
            TranscriptJson.Add(MakeShared<FJsonValueObject>(TranscriptObject));
        }
        EntryObject->SetArrayField(TEXT("Transcript"), TranscriptJson);

        EntriesJson.Add(MakeShared<FJsonValueObject>(EntryObject));
    }

    TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
    RootObject->SetArrayField(TEXT("entries"), EntriesJson);

    FString OutputJson;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputJson);
    if (FJsonSerializer::Serialize(RootObject, Writer))
    {
        FFileHelper::SaveStringToFile(OutputJson, *HistoryPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
}

void SUnrealAgentPanel::RebuildChatHistoryList()
{
    if (!ChatHistoryList.IsValid())
    {
        return;
    }

    ChatHistoryList->ClearChildren();
    if (ChatHistoryEntries.IsEmpty())
    {
        ChatHistoryList->AddSlot()
        .AutoHeight()
        .Padding(FMargin(2.0f, 4.0f, 2.0f, 0.0f))
        [
            SNew(STextBlock)
            .Tag(FName(TEXT("UnrealAgent.Sidebar.History.Empty")))
            .Text(this, &SUnrealAgentPanel::GetChatHistoryEmptyText)
            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
            .AutoWrapText(true)
        ];
        return;
    }

    for (int32 EntryIndex = ChatHistoryEntries.Num() - 1; EntryIndex >= 0; --EntryIndex)
    {
        ChatHistoryList->AddSlot()
        .AutoHeight()
        .Padding(FMargin(0.0f, 0.0f, 0.0f, 8.0f))
        [
            MakeChatHistoryRow(ChatHistoryEntries[EntryIndex])
        ];
    }
}

FString SUnrealAgentPanel::MakeChatTitleFromPrompt(const FString& Prompt) const
{
    FString Title = Prompt.TrimStartAndEnd();
    Title.ReplaceInline(TEXT("\r"), TEXT(" "));
    Title.ReplaceInline(TEXT("\n"), TEXT(" "));
    while (Title.Contains(TEXT("  ")))
    {
        Title.ReplaceInline(TEXT("  "), TEXT(" "));
    }

    if (Title.IsEmpty())
    {
        return TEXT("Untitled chat");
    }

    return Title.Len() <= 48
        ? Title
        : Title.Left(45) + TEXT("...");
}

#undef LOCTEXT_NAMESPACE
