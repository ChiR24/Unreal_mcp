#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateColor.h"
#include "Templates/SharedPointer.h"
#include "Widgets/Layout/SScrollBar.h"

class FComboButtonStyle;
class SScrollBar;
struct FSlateBrush;

namespace UnrealAgent::Panel
{
    inline constexpr int32 MaxTranscriptEntries = 200;
    inline constexpr int32 MaxTranscriptEntryChars = 20000;
    inline constexpr double TranscriptFlushIntervalSeconds = 0.05;
    inline constexpr float TranscriptEntryWidth = 980.0f;
    inline constexpr float UserTranscriptMaxWidth = 560.0f;
    inline constexpr int32 FallbackContextWindowTokens = 128000;
    inline constexpr int32 EstimatedCharactersPerToken = 4;
    inline constexpr const TCHAR* ChatHistoryDirectoryName = TEXT("UnrealAgent");
    inline constexpr const TCHAR* ChatHistoryFileName = TEXT("ChatHistory.json");

#if WITH_DEV_AUTOMATION_TESTS
    extern FString ChatHistoryStoragePathOverride;
#endif

    struct FToolActivityDisplay
    {
        bool bShouldShow = false;
        FString Key;
        FString Title;
        FString Detail;
    };

    FString GetChatHistoryStoragePath();
    int32 CalculateContextUsagePercent(int32 UsedTokens, int32 SizeTokens);
    bool IsStreamTranscriptRole(const FString& Role);
    bool IsConversationRole(const FString& Role);
    bool IsUserTranscriptRole(const FString& Role);
    bool IsActivityTranscriptRole(const FString& Role);
    bool IsRestorableHistoryRole(const FString& Role);

    FString RenderTranscriptText(const FString& Role, const FString& Text);
    FLinearColor RoleAccentColor(const FString& Role);
    FText RoleLabelText(const FString& Role);
    FString FormatElapsedSeconds(double StartedAtSeconds, TSharedPtr<double> EndSeconds);
    FText ActivityTitleText(TSharedPtr<double> StartedAtSeconds, TSharedPtr<double> EndSeconds, TSharedPtr<bool> bHasReasoning, TSharedPtr<int32> UpdateCount);
    FString FormatActivityTranscriptText(const FString& Role, const FString& Text);
    FString MakeToolGroupHeaderText(const FString& ToolTitle, int32 DetailCount);
    FString MakeToolGroupBodyText(const TArray<FString>& Details);
    FToolActivityDisplay ParseToolActivityDisplay(const FString& Text);

    const FSlateBrush* GetModelComboOutlineBrush();
    const FSlateBrush* GetModelProviderHeaderBrush();
    const FSlateBrush* GetUserTranscriptBrush();
    const FSlateBrush* GetActivityTranscriptBrush();
    const FSlateBrush* GetSidebarBrush();
    const FSlateBrush* GetSidebarActiveChatBrush();
    const FSlateBrush* GetSidebarInactiveChatBrush();
    const FSlateBrush* GetHeaderBrush();
    const FComboButtonStyle* GetTransparentModelComboButtonStyle();
    const FSlateBrush* GetMenuSelectionBrush();
    TSharedRef<SScrollBar> MakeHiddenScrollBar(EOrientation Orientation);
}
