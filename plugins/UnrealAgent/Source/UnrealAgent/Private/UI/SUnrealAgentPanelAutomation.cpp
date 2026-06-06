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

#if WITH_DEV_AUTOMATION_TESTS
void SUnrealAgentPanel::SetChatHistoryStoragePathOverrideForAutomation(const FString& StoragePath)
{
    ChatHistoryStoragePathOverride = StoragePath;
}

void SUnrealAgentPanel::ClearChatHistoryStoragePathOverrideForAutomation()
{
    ChatHistoryStoragePathOverride.Reset();
}

void SUnrealAgentPanel::AddTranscriptEntryForAutomation(const FString& Role, const FString& Text)
{
    AddTranscriptEntry(Role, Text);
    FlushPendingTranscript(true);
}

void SUnrealAgentPanel::SetActiveContextWindowUsageForAutomation(int32 UsedTokens, int32 SizeTokens)
{
    FChatHistoryEntry* ActiveEntry = ChatHistoryEntries.FindByPredicate([this](const FChatHistoryEntry& Entry)
    {
        return Entry.Id == ActiveChatHistoryId;
    });
    if (ActiveEntry == nullptr)
    {
        return;
    }

    ActiveEntry->ContextWindowUsedTokens = FMath::Max(0, UsedTokens);
    ActiveEntry->ContextWindowSizeTokens = FMath::Max(1, SizeTokens);
    SaveChatHistory();
}

FString SUnrealAgentPanel::GetContextWindowStatusTextForAutomation() const
{
    return GetContextWindowStatusText().ToString();
}

void SUnrealAgentPanel::ToggleSidebarForAutomation()
{
    OnSidebarToggleClicked();
}

bool SUnrealAgentPanel::IsSidebarCollapsedForAutomation() const
{
    return bSidebarCollapsed;
}

int32 SUnrealAgentPanel::GetChatHistoryCountForAutomation() const
{
    return ChatHistoryEntries.Num();
}

void SUnrealAgentPanel::ResetChatHistoryForAutomation()
{
    ChatHistoryEntries.Reset();
    ActiveChatHistoryId = INDEX_NONE;
    NextChatHistoryId = 1;
    RenamingChatHistoryId = INDEX_NONE;
    PendingRenameTitle.Reset();
    if (!ChatHistoryStoragePathOverride.IsEmpty())
    {
        IFileManager::Get().Delete(*GetChatHistoryStoragePath());
    }
    RebuildChatHistoryList();
}
#endif
#endif

#undef LOCTEXT_NAMESPACE
