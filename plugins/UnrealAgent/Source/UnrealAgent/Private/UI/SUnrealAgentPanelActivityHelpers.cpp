#include "SUnrealAgentPanelPrivate.h"

#include "HAL/PlatformTime.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"

#define LOCTEXT_NAMESPACE "SUnrealAgentPanel"

namespace UnrealAgent::Panel
{
FString WorkingDots(const bool bAnimate)
    {
        const int32 DotCount = bAnimate
            ? 1 + (FMath::FloorToInt(FPlatformTime::Seconds() * 2.0) % 3)
            : 3;

        FString Dots;
        for (int32 DotIndex = 0; DotIndex < DotCount; ++DotIndex)
        {
            Dots += TEXT(".");
        }
        return Dots;
    }

FLinearColor RoleAccentColor(const FString& Role)
    {
        if (Role == TEXT("OpenCode"))
        {
            return FStyleColors::Success.GetSpecifiedColor();
        }
        if (Role == TEXT("You") || Role == TEXT("User"))
        {
            return FStyleColors::AccentBlue.GetSpecifiedColor();
        }
        if (Role == TEXT("Thought"))
        {
            return FStyleColors::ForegroundHover.GetSpecifiedColor();
        }
        if (Role == TEXT("Tool"))
        {
            return FStyleColors::AccentYellow.GetSpecifiedColor();
        }
        if (Role == TEXT("Permission"))
        {
            return FStyleColors::Warning.GetSpecifiedColor();
        }
        if (Role == TEXT("Error"))
        {
            return FStyleColors::Error.GetSpecifiedColor();
        }
        return FStyleColors::Foreground.GetSpecifiedColor();
    }

FText RoleLabelText(const FString& Role)
    {
        if (Role == TEXT("OpenCode") || Role == TEXT("You") || Role == TEXT("User"))
        {
            return FText::GetEmpty();
        }
        if (Role == TEXT("Thought"))
        {
            return FText::GetEmpty();
        }
        if (Role == TEXT("Tool"))
        {
            return LOCTEXT("ToolRole", "Tool");
        }
        if (Role == TEXT("Permission"))
        {
            return LOCTEXT("PermissionRole", "Permission");
        }
        if (Role == TEXT("Plan"))
        {
            return LOCTEXT("PlanRole", "Plan");
        }
        return FText::FromString(Role);
    }

FString FormatElapsedSeconds(const double StartedAtSeconds, const TSharedPtr<double> EndSeconds)
    {
        const double FinishedAtSeconds = EndSeconds.IsValid() && *EndSeconds > 0.0
            ? *EndSeconds
            : FPlatformTime::Seconds();
        const double ElapsedSeconds = FMath::Max(0.0, FinishedAtSeconds - StartedAtSeconds);
        if (ElapsedSeconds < 1.0)
        {
            return TEXT("<1 sec");
        }

        return FString::Printf(TEXT("%d sec"), FMath::FloorToInt(ElapsedSeconds));
    }

FText ActivityTitleText(
        const TSharedPtr<double> StartedAtSeconds,
        const TSharedPtr<double> EndSeconds,
        const TSharedPtr<bool> bHasReasoning,
        const TSharedPtr<int32> UpdateCount)
    {
        const bool bHasStartedAt = StartedAtSeconds.IsValid() && *StartedAtSeconds > 0.0;
        const bool bHasEnded = EndSeconds.IsValid() && *EndSeconds > 0.0;
        if (bHasStartedAt)
        {
            const FString ElapsedText = FormatElapsedSeconds(*StartedAtSeconds, EndSeconds);
            return bHasEnded
                ? FText::FromString(FString::Printf(TEXT("Working %s"), *ElapsedText))
                : FText::FromString(FString::Printf(TEXT("Working %s %s"), *ElapsedText, *WorkingDots(true)));
        }

        return bHasEnded
            ? LOCTEXT("WorkingComplete", "Working")
            : FText::FromString(FString::Printf(TEXT("Working %s"), *WorkingDots(true)));
    }

FString FormatActivityTranscriptText(const FString& Role, const FString& Text)
    {
        const FString Label = RoleLabelText(Role).ToString();
        const FString DisplayText = RenderTranscriptText(Role, Text).TrimStartAndEnd();
        return Label.IsEmpty()
            ? DisplayText
            : FString::Printf(TEXT("%s\n%s"), *Label, *DisplayText);
    }

    struct FToolActivityDisplay
    {
        bool bShouldShow = false;
        FString Key;
        FString Title;
        FString Detail;
    };

bool IsToolStatusToken(const FString& Token)
    {
        FString NormalizedToken = Token.TrimStartAndEnd().ToLower();
        NormalizedToken.ReplaceInline(TEXT("-"), TEXT("_"));
        return NormalizedToken == TEXT("started")
            || NormalizedToken == TEXT("running")
            || NormalizedToken == TEXT("in_progress")
            || NormalizedToken == TEXT("completed")
            || NormalizedToken == TEXT("complete")
            || NormalizedToken == TEXT("pending");
    }

FString NormalizeToolDisplayName(FString ToolName)
    {
        ToolName = ToolName.TrimStartAndEnd();
        while (ToolName.EndsWith(TEXT(":")))
        {
            ToolName = ToolName.LeftChop(1).TrimEnd();
        }

        ToolName.ReplaceInline(TEXT("_"), TEXT(" "));
        ToolName.ReplaceInline(TEXT("-"), TEXT(" "));

        TArray<FString> Words;
        ToolName.ParseIntoArray(Words, TEXT(" "), true);
        for (FString& Word : Words)
        {
            if (Word.IsEmpty())
            {
                continue;
            }

            Word[0] = FChar::ToUpper(Word[0]);
            for (int32 CharacterIndex = 1; CharacterIndex < Word.Len(); ++CharacterIndex)
            {
                Word[CharacterIndex] = FChar::ToLower(Word[CharacterIndex]);
            }
        }

        return Words.IsEmpty() ? TEXT("Tool") : FString::Join(Words, TEXT(" "));
    }

FString MakeToolGroupHeaderText(const FString& ToolTitle, const int32 DetailCount)
    {
        return DetailCount > 1
            ? FString::Printf(TEXT("%s · %d"), *ToolTitle, DetailCount)
            : ToolTitle;
    }

FString MakeToolGroupBodyText(const TArray<FString>& Details)
    {
        FString BodyText;
        for (const FString& Detail : Details)
        {
            const FString TrimmedDetail = Detail.TrimStartAndEnd();
            if (TrimmedDetail.IsEmpty())
            {
                continue;
            }

            if (!BodyText.IsEmpty())
            {
                BodyText += LINE_TERMINATOR;
            }
            BodyText += Details.Num() > 1
                ? FString::Printf(TEXT("• %s"), *TrimmedDetail)
                : TrimmedDetail;
        }

        return BodyText.IsEmpty() ? TEXT("No details") : BodyText;
    }

FToolActivityDisplay ParseToolActivityDisplay(const FString& Text)
    {
        FToolActivityDisplay Display;
        FString ToolText = Text.TrimStartAndEnd();
        if (ToolText.IsEmpty())
        {
            return Display;
        }

        if (ToolText.StartsWith(TEXT("Started "), ESearchCase::IgnoreCase))
        {
            ToolText = ToolText.Mid(8).TrimStartAndEnd();
        }

        int32 LastSpaceIndex = INDEX_NONE;
        if (ToolText.FindLastChar(TEXT(' '), LastSpaceIndex))
        {
            const FString LastToken = ToolText.Mid(LastSpaceIndex + 1).TrimStartAndEnd();
            if (IsToolStatusToken(LastToken))
            {
                ToolText = ToolText.Left(LastSpaceIndex).TrimStartAndEnd();
            }
        }
        else if (IsToolStatusToken(ToolText))
        {
            return Display;
        }

        if (ToolText.IsEmpty())
        {
            return Display;
        }

        int32 SpaceIndex = INDEX_NONE;
        int32 ColonIndex = INDEX_NONE;
        ToolText.FindChar(TEXT(' '), SpaceIndex);
        ToolText.FindChar(TEXT(':'), ColonIndex);

        int32 SeparatorIndex = INDEX_NONE;
        if (SpaceIndex != INDEX_NONE && ColonIndex != INDEX_NONE)
        {
            SeparatorIndex = FMath::Min(SpaceIndex, ColonIndex);
        }
        else if (SpaceIndex != INDEX_NONE)
        {
            SeparatorIndex = SpaceIndex;
        }
        else
        {
            SeparatorIndex = ColonIndex;
        }

        FString ToolName = ToolText;
        FString Detail;
        if (SeparatorIndex != INDEX_NONE)
        {
            ToolName = ToolText.Left(SeparatorIndex).TrimStartAndEnd();
            Detail = ToolText.Mid(SeparatorIndex + 1).TrimStartAndEnd();
        }

        const FString ToolTitle = NormalizeToolDisplayName(ToolName);
        if (ToolTitle.IsEmpty())
        {
            return Display;
        }

        if (Detail.IsEmpty())
        {
            Detail = ToolText;
        }

        Display.bShouldShow = true;
        Display.Title = ToolTitle;
        Display.Key = ToolTitle.ToLower();
        Display.Detail = Detail;
        return Display;
    }
}

#undef LOCTEXT_NAMESPACE
