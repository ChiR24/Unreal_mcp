#include "SUnrealAgentPanelPrivate.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Layout/SScrollBar.h"

#define LOCTEXT_NAMESPACE "SUnrealAgentPanel"

namespace UnrealAgent::Panel
{
#if WITH_DEV_AUTOMATION_TESTS
FString ChatHistoryStoragePathOverride;
#endif

FString GetChatHistoryStoragePath()
    {
#if WITH_DEV_AUTOMATION_TESTS
        if (!ChatHistoryStoragePathOverride.IsEmpty())
        {
            return ChatHistoryStoragePathOverride;
        }
#endif
        return FPaths::Combine(FPaths::ProjectSavedDir(), ChatHistoryDirectoryName, ChatHistoryFileName);
    }

int32 CalculateContextUsagePercent(const int32 UsedTokens, const int32 SizeTokens)
    {
        return FMath::Clamp(
            FMath::CeilToInt((static_cast<float>(FMath::Max(0, UsedTokens)) / static_cast<float>(FMath::Max(1, SizeTokens))) * 100.0f),
            0,
            100);
    }

bool IsStreamTranscriptRole(const FString& Role)
    {
        return Role == TEXT("OpenCode") || Role == TEXT("User") || Role == TEXT("Thought");
    }

bool IsConversationRole(const FString& Role)
    {
        return Role == TEXT("OpenCode") || Role == TEXT("User") || Role == TEXT("You") || Role == TEXT("Thought") || Role == TEXT("Tool") || Role == TEXT("Permission") || Role == TEXT("Plan") || Role == TEXT("Error");
    }

bool IsUserTranscriptRole(const FString& Role)
    {
        return Role == TEXT("User") || Role == TEXT("You");
    }

bool IsActivityTranscriptRole(const FString& Role)
    {
        return Role == TEXT("Thought") || Role == TEXT("Tool") || Role == TEXT("Permission") || Role == TEXT("Plan");
    }

bool IsRestorableHistoryRole(const FString& Role)
    {
        return IsConversationRole(Role);
    }
}

#undef LOCTEXT_NAMESPACE
