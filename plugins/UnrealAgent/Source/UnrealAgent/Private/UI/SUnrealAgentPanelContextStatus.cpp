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

FSlateColor SUnrealAgentPanel::GetContextWindowIndicatorColor() const
{
    const int32 UsedPercent = GetContextWindowUsedPercent();
    if (UsedPercent >= 90)
    {
        return FStyleColors::Error;
    }
    if (UsedPercent >= 70)
    {
        return FStyleColors::Warning;
    }
    return FStyleColors::ForegroundHover;
}

FText SUnrealAgentPanel::GetContextWindowStatusText() const
{
    return FText::FromString(FString::Printf(TEXT("%d%% used"), GetContextWindowUsedPercent()));
}

FText SUnrealAgentPanel::GetContextWindowDetailText() const
{
    const FChatHistoryEntry* ActiveEntry = ChatHistoryEntries.FindByPredicate([this](const FChatHistoryEntry& Entry)
    {
        return Entry.Id == ActiveChatHistoryId;
    });

    if (ActiveEntry == nullptr)
    {
        return LOCTEXT("ContextWindowNoChat", "0% used. Waiting for ACP context usage.");
    }

    if (ActiveEntry->ContextWindowSizeTokens > 0)
    {
        return FText::FromString(FString::Printf(
            TEXT("%d%% used. Exact ACP context usage for this chat: %d of %d tokens."),
            GetContextWindowUsedPercent(),
            ActiveEntry->ContextWindowUsedTokens,
            ActiveEntry->ContextWindowSizeTokens));
    }

    return FText::FromString(FString::Printf(
        TEXT("%d%% used. Waiting for ACP context usage; estimated from %d local transcript %s for now."),
        GetContextWindowUsedPercent(),
        ActiveEntry->EntryCount,
        ActiveEntry->EntryCount == 1 ? TEXT("entry") : TEXT("entries")));
}

EVisibility SUnrealAgentPanel::GetContextWindowVisibility() const
{
    return AcpClient.IsValid() && AcpClient->IsReady() && !AcpClient->GetCurrentModel().IsEmpty()
        ? EVisibility::Visible
        : EVisibility::Collapsed;
}

int32 SUnrealAgentPanel::GetContextWindowTokenCapacity() const
{
    const FChatHistoryEntry* ActiveEntry = ChatHistoryEntries.FindByPredicate([this](const FChatHistoryEntry& Entry)
    {
        return Entry.Id == ActiveChatHistoryId;
    });
    if (ActiveEntry != nullptr && ActiveEntry->ContextWindowSizeTokens > 0)
    {
        return ActiveEntry->ContextWindowSizeTokens;
    }

    if (AcpClient.IsValid())
    {
        const FString& CurrentModel = AcpClient->GetCurrentModel();
        const FOpenCodeAcpModelOption* ModelOption = AcpClient->GetModelOptions().FindByPredicate([&CurrentModel](const FOpenCodeAcpModelOption& Option)
        {
            return Option.Id == CurrentModel;
        });
        if (ModelOption != nullptr && ModelOption->ContextWindowTokens > 0)
        {
            return ModelOption->ContextWindowTokens;
        }
    }

    return FallbackContextWindowTokens;
}

int32 SUnrealAgentPanel::GetContextWindowUsedPercent() const
{
    const FChatHistoryEntry* ActiveEntry = ChatHistoryEntries.FindByPredicate([this](const FChatHistoryEntry& Entry)
    {
        return Entry.Id == ActiveChatHistoryId;
    });

    if (ActiveEntry != nullptr && ActiveEntry->ContextWindowSizeTokens > 0)
    {
        return CalculateContextUsagePercent(ActiveEntry->ContextWindowUsedTokens, ActiveEntry->ContextWindowSizeTokens);
    }

    const int32 ContextCharacters = ActiveEntry == nullptr ? 0 : ActiveEntry->ContextCharacters;
    const int32 CapacityCharacters = FMath::Max(1, GetContextWindowTokenCapacity() * EstimatedCharactersPerToken);
    return FMath::Clamp(FMath::CeilToInt((static_cast<float>(ContextCharacters) / static_cast<float>(CapacityCharacters)) * 100.0f), 0, 100);
}

FText SUnrealAgentPanel::GetStudioKitStatusText() const
{
    if (!AcpClient.IsValid() || AcpClient->GetLastStudioKitSummary().IsEmpty())
    {
        return LOCTEXT("StudioKitPending", "Studio Kit ready on connect");
    }
    return FText::FromString(AcpClient->GetLastStudioKitSummary());
}

FText SUnrealAgentPanel::GetEditorContextStatusText() const
{
    if (!bAttachEditorContext)
    {
        return LOCTEXT("EditorContextDetached", "Context detached");
    }
    if (!AcpClient.IsValid() || AcpClient->GetLastEditorContextSummary().IsEmpty())
    {
        return LOCTEXT("EditorContextReady", "Context attaches to prompts");
    }
    return FText::FromString(AcpClient->GetLastEditorContextSummary());
}

FText SUnrealAgentPanel::GetValidationStatusText() const
{
    if (!AcpClient.IsValid() || AcpClient->GetLastValidationSummary().IsEmpty())
    {
        return LOCTEXT("ValidationPending", "Validation not run");
    }

    FString Summary = AcpClient->GetLastValidationSummary();
    Summary.ReplaceInline(TEXT("\n"), TEXT(" "));
    return FText::FromString(Summary.Left(120));
}

#undef LOCTEXT_NAMESPACE
