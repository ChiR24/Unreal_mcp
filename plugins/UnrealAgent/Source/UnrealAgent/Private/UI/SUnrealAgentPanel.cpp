#include "SUnrealAgentPanel.h"

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

namespace
{
    constexpr int32 MaxTranscriptEntries = 200;
    constexpr int32 MaxTranscriptEntryChars = 20000;
    constexpr double TranscriptFlushIntervalSeconds = 0.05;
    constexpr float TranscriptEntryWidth = 980.0f;
    constexpr float UserTranscriptMaxWidth = 560.0f;
    constexpr int32 FallbackContextWindowTokens = 128000;
    constexpr int32 EstimatedCharactersPerToken = 4;
    constexpr const TCHAR* ChatHistoryDirectoryName = TEXT("UnrealAgent");
    constexpr const TCHAR* ChatHistoryFileName = TEXT("ChatHistory.json");

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

    void AppendRenderedLine(FString& RenderedText, const FString& Line)
    {
        if (!RenderedText.IsEmpty())
        {
            RenderedText += LINE_TERMINATOR;
        }
        RenderedText += Line;
    }

    FString SanitizeInlineMarkdown(FString Line)
    {
        Line.ReplaceInline(TEXT("**"), TEXT(""));
        Line.ReplaceInline(TEXT("__"), TEXT(""));
        Line.ReplaceInline(TEXT("`"), TEXT(""));
        Line.ReplaceInline(TEXT("\t"), TEXT("    "));
        return Line;
    }

    bool ParseMarkdownTableCells(FString Line, TArray<FString>& OutCells)
    {
        OutCells.Reset();
        if (!Line.Contains(TEXT("|")))
        {
            return false;
        }

        Line = Line.TrimStartAndEnd();
        while (Line.StartsWith(TEXT("|")))
        {
            Line = Line.Mid(1).TrimStart();
        }
        while (Line.EndsWith(TEXT("|")))
        {
            Line = Line.LeftChop(1).TrimEnd();
        }

        Line.ParseIntoArray(OutCells, TEXT("|"), false);
        for (FString& Cell : OutCells)
        {
            Cell = SanitizeInlineMarkdown(Cell.TrimStartAndEnd());
        }

        return OutCells.Num() >= 2;
    }

    bool IsMarkdownTableSeparator(const FString& Line)
    {
        TArray<FString> Cells;
        if (!Line.Contains(TEXT("-")) || !ParseMarkdownTableCells(Line, Cells))
        {
            return false;
        }

        for (FString Cell : Cells)
        {
            Cell.ReplaceInline(TEXT("-"), TEXT(""));
            Cell.ReplaceInline(TEXT(":"), TEXT(""));
            Cell.ReplaceInline(TEXT(" "), TEXT(""));
            Cell.ReplaceInline(TEXT("\t"), TEXT(""));
            if (!Cell.IsEmpty())
            {
                return false;
            }
        }

        return true;
    }

    FString RenderMarkdownTableDataRow(const TArray<FString>& HeaderCells, const TArray<FString>& RowCells)
    {
        if (RowCells.Num() == 2)
        {
            return FString::Printf(TEXT("%s: %s"), *RowCells[0], *RowCells[1]);
        }

        TArray<FString> DetailParts;
        for (int32 CellIndex = 1; CellIndex < RowCells.Num(); ++CellIndex)
        {
            const FString& Cell = RowCells[CellIndex];
            if (Cell.IsEmpty())
            {
                continue;
            }

            const FString Label = HeaderCells.IsValidIndex(CellIndex) ? HeaderCells[CellIndex] : FString::Printf(TEXT("Column %d"), CellIndex + 1);
            DetailParts.Add(Label.IsEmpty() ? Cell : FString::Printf(TEXT("%s: %s"), *Label, *Cell));
        }

        if (DetailParts.IsEmpty())
        {
            return RowCells.IsEmpty() ? FString() : RowCells[0];
        }

        const FString Details = FString::Join(DetailParts, TEXT("; "));
        return RowCells[0].IsEmpty() ? Details : FString::Printf(TEXT("%s: %s"), *RowCells[0], *Details);
    }

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

    bool TryNormalizeNumberedList(FString& Line)
    {
        int32 DigitCount = 0;
        while (DigitCount < Line.Len() && FChar::IsDigit(Line[DigitCount]))
        {
            ++DigitCount;
        }
        if (DigitCount == 0 || DigitCount + 1 >= Line.Len())
        {
            return false;
        }

        const TCHAR Marker = Line[DigitCount];
        if ((Marker != TEXT('.') && Marker != TEXT(')')) || !FChar::IsWhitespace(Line[DigitCount + 1]))
        {
            return false;
        }

        const FString Number = Line.Left(DigitCount);
        const FString Body = Line.Mid(DigitCount + 1).TrimStartAndEnd();
        Line = FString::Printf(TEXT("%s. %s"), *Number, *Body);
        return true;
    }

    FString RenderMarkdownForDisplay(const FString& Text)
    {
        TArray<FString> Lines;
        Text.ParseIntoArrayLines(Lines, false);

        FString RenderedText;
        bool bInCodeBlock = false;
        for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
        {
            const FString& Line = Lines[LineIndex];
            const FString TrimmedLine = Line.TrimStartAndEnd();
            if (TrimmedLine.StartsWith(TEXT("```")) || TrimmedLine.StartsWith(TEXT("~~~")))
            {
                bInCodeBlock = !bInCodeBlock;
                continue;
            }

            FString RenderedLine = bInCodeBlock ? Line : TrimmedLine;
            if (!bInCodeBlock)
            {
                TArray<FString> TableHeaderCells;
                if (ParseMarkdownTableCells(RenderedLine, TableHeaderCells)
                    && Lines.IsValidIndex(LineIndex + 1)
                    && IsMarkdownTableSeparator(Lines[LineIndex + 1].TrimStartAndEnd()))
                {
                    LineIndex += 2;
                    for (; LineIndex < Lines.Num(); ++LineIndex)
                    {
                        const FString TableRowLine = Lines[LineIndex].TrimStartAndEnd();
                        TArray<FString> RowCells;
                        if (!ParseMarkdownTableCells(TableRowLine, RowCells) || IsMarkdownTableSeparator(TableRowLine))
                        {
                            --LineIndex;
                            break;
                        }

                        AppendRenderedLine(RenderedText, RenderMarkdownTableDataRow(TableHeaderCells, RowCells));
                    }
                    continue;
                }

                if (RenderedLine == TEXT("---") || RenderedLine == TEXT("***") || RenderedLine == TEXT("___") || IsMarkdownTableSeparator(RenderedLine))
                {
                    continue;
                }

                int32 HeadingMarkerCount = 0;
                while (HeadingMarkerCount < RenderedLine.Len() && RenderedLine[HeadingMarkerCount] == TEXT('#'))
                {
                    ++HeadingMarkerCount;
                }
                if (HeadingMarkerCount > 0 && HeadingMarkerCount < RenderedLine.Len() && FChar::IsWhitespace(RenderedLine[HeadingMarkerCount]))
                {
                    RenderedLine = RenderedLine.Mid(HeadingMarkerCount).TrimStartAndEnd();
                }

                if (RenderedLine.StartsWith(TEXT("- [ ] ")))
                {
                    RenderedLine = FString::Printf(TEXT("☐ %s"), *RenderedLine.Mid(6).TrimStartAndEnd());
                }
                else if (RenderedLine.StartsWith(TEXT("- [x] ")) || RenderedLine.StartsWith(TEXT("- [X] ")))
                {
                    RenderedLine = FString::Printf(TEXT("☑ %s"), *RenderedLine.Mid(6).TrimStartAndEnd());
                }
                else if (RenderedLine.StartsWith(TEXT("- ")) || RenderedLine.StartsWith(TEXT("* ")) || RenderedLine.StartsWith(TEXT("+ ")))
                {
                    RenderedLine = FString::Printf(TEXT("• %s"), *RenderedLine.Mid(2).TrimStartAndEnd());
                }
                else if (TryNormalizeNumberedList(RenderedLine))
                {
                }
                else
                {
                    TArray<FString> LooseTableCells;
                    if (ParseMarkdownTableCells(RenderedLine, LooseTableCells))
                    {
                        RenderedLine = RenderMarkdownTableDataRow(TArray<FString>(), LooseTableCells);
                    }
                }

                RenderedLine = SanitizeInlineMarkdown(RenderedLine);
            }
            else
            {
                RenderedLine.ReplaceInline(TEXT("\t"), TEXT("    "));
            }

            AppendRenderedLine(RenderedText, RenderedLine);
        }

        return RenderedText;
    }

    FString RenderTranscriptText(const FString& Role, const FString& Text)
    {
        return Role == TEXT("OpenCode") ? RenderMarkdownForDisplay(Text) : Text;
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

    const FSlateBrush* GetModelComboOutlineBrush()
    {
        static const FSlateRoundedBoxBrush OutlineBrush(
            FStyleColors::Input,
            4.0f,
            FStyleColors::InputOutline,
            1.0f);
        return &OutlineBrush;
    }

    const FSlateBrush* GetModelProviderHeaderBrush()
    {
        static const FSlateRoundedBoxBrush HeaderBrush(
            FStyleColors::Header,
            2.0f,
            FStyleColors::Recessed,
            1.0f);
        return &HeaderBrush;
    }

    const FSlateBrush* GetUserTranscriptBrush()
    {
        static const FSlateRoundedBoxBrush UserBrush(
            FStyleColors::Input,
            4.0f,
            FStyleColors::InputOutline,
            1.0f);
        return &UserBrush;
    }

    const FSlateBrush* GetActivityTranscriptBrush()
    {
        static const FSlateRoundedBoxBrush ActivityBrush(
            FStyleColors::Recessed,
            3.0f,
            FStyleColors::Header,
            1.0f);
        return &ActivityBrush;
    }

    const FSlateBrush* GetSidebarBrush()
    {
        static const FSlateRoundedBoxBrush SidebarBrush(
            FStyleColors::Panel,
            0.0f,
            FStyleColors::Recessed,
            1.0f);
        return &SidebarBrush;
    }

    const FSlateBrush* GetSidebarActiveChatBrush()
    {
        static const FSlateRoundedBoxBrush ActiveChatBrush(
            FStyleColors::Select,
            3.0f,
            FStyleColors::SelectHover,
            1.0f);
        return &ActiveChatBrush;
    }

    const FSlateBrush* GetSidebarInactiveChatBrush()
    {
        static const FSlateRoundedBoxBrush InactiveChatBrush(
            FStyleColors::Header,
            3.0f,
            FStyleColors::Recessed,
            1.0f);
        return &InactiveChatBrush;
    }

    const FSlateBrush* GetHeaderBrush()
    {
        static const FSlateRoundedBoxBrush HeaderBrush(
            FStyleColors::Header,
            0.0f,
            FStyleColors::Recessed,
            1.0f);
        return &HeaderBrush;
    }

    const FComboButtonStyle* GetTransparentModelComboButtonStyle()
    {
        static const FComboButtonStyle TransparentStyle = []()
        {
            FComboButtonStyle Style = FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("ComboButton");
            return Style;
        }();
        return &TransparentStyle;
    }

    const FSlateBrush* GetMenuSelectionBrush()
    {
        static const FSlateRoundedBoxBrush SelectionBrush(
            FStyleColors::Select,
            2.0f,
            FStyleColors::SelectHover,
            1.0f);
        return &SelectionBrush;
    }

    TSharedRef<SScrollBar> MakeHiddenScrollBar(EOrientation Orientation)
    {
        return SNew(SScrollBar)
            .Orientation(Orientation)
            .Visibility(EVisibility::Collapsed)
            .Thickness(FVector2D(0.0f, 0.0f));
    }

}

SUnrealAgentPanel::~SUnrealAgentPanel()
{
    if (AcpClient.IsValid())
    {
        AcpClient->OnStatus.Unbind();
        AcpClient->OnTranscript.Unbind();
        AcpClient->OnPermission.Unbind();
        AcpClient->OnModelsChanged.Unbind();
        AcpClient->OnStopped.Unbind();
        AcpClient->Stop();
        AcpClient.Reset();
    }
}

void SUnrealAgentPanel::Construct(const FArguments&)
{
    StatusText.Reset();

    AcpClient = MakeUnique<FOpenCodeAcpClient>();
    AcpClient->OnStatus.BindRaw(this, &SUnrealAgentPanel::SetStatus);
    AcpClient->OnTranscript.BindRaw(this, &SUnrealAgentPanel::AddTranscriptEntry);
    AcpClient->OnPermission.BindRaw(this, &SUnrealAgentPanel::HandlePermissionRequest);
    AcpClient->OnModelsChanged.BindRaw(this, &SUnrealAgentPanel::RefreshModelOptions);
    AcpClient->OnStopped.BindRaw(this, &SUnrealAgentPanel::HandleClientStopped);
    AcpClient->SetAttachEditorContext(bAttachEditorContext);

    LoadChatHistory();

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(FStyleColors::Recessed)
        .Padding(FMargin(0.0f))
        [
            SNew(SHorizontalBox)
            .Tag(FName(TEXT("UnrealAgent.Layout")))
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(FMargin(0.0f))
            [
                MakeSidebar()
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .Padding(FMargin(12.0f, 0.0f, 8.0f, 8.0f))
            [
                SNew(SVerticalBox)
                .Tag(FName(TEXT("UnrealAgent.MainColumn")))
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.0f, 0.0f, 0.0f, 8.0f))
            [
                SNew(SBorder)
                .Tag(FName(TEXT("UnrealAgent.Header")))
                .BorderImage(GetHeaderBrush())
                .Padding(FMargin(10.0f, 6.0f))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Tag(FName(TEXT("UnrealAgent.Header.Title")))
                        .Text(LOCTEXT("Title", "Unreal Agent"))
                        .Font(FAppStyle::Get().GetFontStyle("SmallBoldFont"))
                        .ColorAndOpacity(FSlateColor::UseForeground())
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(SButton)
                        .Tag(FName(TEXT("UnrealAgent.Header.ConnectButton")))
                        .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
                        .ContentPadding(FMargin(6.0f, 2.0f))
                        .ToolTipText_Lambda([this]()
                        {
                            const FString LowerStatus = StatusText.ToLower();
                            if (LowerStatus.Contains(TEXT("error")) || LowerStatus.Contains(TEXT("failed")) || LowerStatus.Contains(TEXT("timed out")) || LowerStatus.Contains(TEXT("exited")))
                            {
                                return FText::FromString(StatusText);
                            }

                            return FText::GetEmpty();
                        })
                        .OnClicked(this, &SUnrealAgentPanel::OnConnectClicked)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            [
                                SNew(SImage)
                                .Tag(FName(TEXT("UnrealAgent.Header.ConnectionIndicator")))
                                .Image(FAppStyle::Get().GetBrush("Icons.FilledCircle"))
                                .ColorAndOpacity(this, &SUnrealAgentPanel::GetConnectionIndicatorColor)
                                .DesiredSizeOverride(FVector2D(8.0f, 8.0f))
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            .Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
                            [
                                SNew(STextBlock)
                                .Text(this, &SUnrealAgentPanel::GetConnectionButtonText)
                                .ColorAndOpacity(FSlateColor::UseForeground())
                            ]
                        ]
                    ]
                ]
            ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.0f, 0.0f, 0.0f, 8.0f))
                [
                    MakeCockpit()
                ]
                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                .Padding(FMargin(0.0f, 0.0f, 0.0f, 12.0f))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .FillHeight(1.0f)
                    [
                        SNew(SVerticalBox)
                        .Visibility(this, &SUnrealAgentPanel::GetInitialComposerVisibility)
                        + SVerticalBox::Slot()
                        .FillHeight(1.0f)
                        [
                            SNullWidget::NullWidget
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .HAlign(HAlign_Fill)
                        .Padding(FMargin(8.0f, 0.0f, 0.0f, 8.0f))
                        [
                            SNew(SBorder)
                            .BorderImage(GetHeaderBrush())
                            .Padding(FMargin(12.0f, 10.0f))
                            [
                                SNew(SVerticalBox)
                                .Tag(FName(TEXT("UnrealAgent.EmptyState")))
                                + SVerticalBox::Slot()
                                .AutoHeight()
                                [
                                    SNew(SVerticalBox)
                                    + SVerticalBox::Slot()
                                    .AutoHeight()
                                    [
                                        SNew(STextBlock)
                                        .Text(LOCTEXT("EmptyStateTitle", "What should Unreal Agent help with?"))
                                        .Font(FAppStyle::Get().GetFontStyle("SmallBoldFont"))
                                        .ColorAndOpacity(FSlateColor::UseForeground())
                                    ]
                                    + SVerticalBox::Slot()
                                    .AutoHeight()
                                    .Padding(FMargin(0.0f, 3.0f, 0.0f, 8.0f))
                                    [
                                        SNew(STextBlock)
                                        .Text(LOCTEXT("EmptyStateSubtitle", "Start with a focused editor task, or type your own prompt below."))
                                        .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                                        .AutoWrapText(true)
                                    ]
                                    + SVerticalBox::Slot()
                                    .AutoHeight()
                                    .HAlign(HAlign_Fill)
                                    [
                                        SNew(SVerticalBox)
                                        .Tag(FName(TEXT("UnrealAgent.EmptyState.QuickPromptGrid")))
                                        + SVerticalBox::Slot()
                                        .AutoHeight()
                                        [
                                            SNew(SHorizontalBox)
                                            + SHorizontalBox::Slot()
                                            .FillWidth(1.0f)
                                            .Padding(FMargin(0.0f, 0.0f, 6.0f, 6.0f))
                                            [
                                                SNew(SButton)
                                                .Tag(FName(TEXT("UnrealAgent.EmptyState.QuickPrompt.ArchitectureReview")))
                                                .HAlign(HAlign_Center)
                                                .ContentPadding(FMargin(6.0f, 3.0f))
                                                .Text(LOCTEXT("QuickPromptArchitectureReview", "Architecture review"))
                                                .OnClicked(this, &SUnrealAgentPanel::OnQuickPromptClicked, FString(TEXT("Act as Unreal Agent's production technical director. Inspect available project context and, when connected, use unreal-engine MCP tools such as manage_tools and inspect before making editor-state claims. Review architecture for a shippable game: modules, GameMode/GameState/Pawn/Controller/HUD/GameInstance ownership, C++ versus Blueprint boundaries, save/load, multiplayer, asset/content conventions, performance risks, and the smallest reversible next steps.")))
                                            ]
                                            + SHorizontalBox::Slot()
                                            .FillWidth(1.0f)
                                            .Padding(FMargin(6.0f, 0.0f, 0.0f, 6.0f))
                                            [
                                                SNew(SButton)
                                                .Tag(FName(TEXT("UnrealAgent.EmptyState.QuickPrompt.GameplayPlan")))
                                                .HAlign(HAlign_Center)
                                                .ContentPadding(FMargin(6.0f, 3.0f))
                                                .Text(LOCTEXT("QuickPromptGameplayPlan", "Gameplay plan"))
                                                .OnClicked(this, &SUnrealAgentPanel::OnQuickPromptClicked, FString(TEXT("Create a complete Unreal game production plan for the concept I describe next. Start from the smallest playable prototype, then vertical slice, production, polish, and release readiness. Map each phase to concrete MCP tool domains for assets, Blueprints, levels, actors, UI, AI, audio, VFX, animation, combat, networking, inventory, interaction, tests, screenshots, profiling, and packaging. Include acceptance criteria, non-goals, risks, and what you need to inspect before implementation.")))
                                            ]
                                        ]
                                        + SVerticalBox::Slot()
                                        .AutoHeight()
                                        [
                                            SNew(SHorizontalBox)
                                            + SHorizontalBox::Slot()
                                            .FillWidth(1.0f)
                                            .Padding(FMargin(0.0f, 6.0f, 6.0f, 0.0f))
                                            [
                                                SNew(SButton)
                                                .Tag(FName(TEXT("UnrealAgent.EmptyState.QuickPrompt.QARiskPass")))
                                                .HAlign(HAlign_Center)
                                                .ContentPadding(FMargin(6.0f, 3.0f))
                                                .Text(LOCTEXT("QuickPromptQARiskPass", "QA risk pass"))
                                                .OnClicked(this, &SUnrealAgentPanel::OnQuickPromptClicked, FString(TEXT("Run a ship-readiness QA pass on the Unreal game, feature, or change I describe next. Define observable acceptance criteria, likely regressions, deterministic MCP/editor verification steps, PIE or automation coverage, screenshot/log evidence to collect, performance/accessibility/localization checks, and the release blockers that must be fixed before approval.")))
                                            ]
                                            + SHorizontalBox::Slot()
                                            .FillWidth(1.0f)
                                            .Padding(FMargin(6.0f, 6.0f, 0.0f, 0.0f))
                                            [
                                                SNew(SButton)
                                                .Tag(FName(TEXT("UnrealAgent.EmptyState.QuickPrompt.EditorTooling")))
                                                .HAlign(HAlign_Center)
                                                .ContentPadding(FMargin(6.0f, 3.0f))
                                                .Text(LOCTEXT("QuickPromptEditorTooling", "Editor tooling"))
                                                .OnClicked(this, &SUnrealAgentPanel::OnQuickPromptClicked, FString(TEXT("Design or execute an MCP-backed Unreal editor automation workflow for the production task I describe next. Use manage_tools to confirm capabilities when connected, choose the safest tool domain, keep operations project-scoped and reversible, ask before destructive/bulk changes, and verify through deterministic editor inspection, asset compilation, screenshots, logs, tests, or build checks.")))
                                            ]
                                        ]
                                    ]
                                ]
                            ]
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .HAlign(HAlign_Fill)
                        [
                            MakeComposer(CenterPromptTextBox, CenterModelComboButton, CenterThinkingComboButton, CenterAgentComboButton, FName(TEXT("UnrealAgent.Composer.Center")))
                        ]
                    ]
                    + SVerticalBox::Slot()
                    .FillHeight(1.0f)
                    [
                        SNew(SBox)
                        .Visibility(this, &SUnrealAgentPanel::GetConversationVisibility)
                        [
                            StaticCastSharedRef<SWidget>(SAssignNew(TranscriptScrollBox, SScrollBox)
                                .Tag(FName(TEXT("UnrealAgent.Transcript.Scroll"))))
                        ]
                    ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 0.0f, 0.0f, 10.0f))
            [
                SNew(SBorder)
                .BorderImage(GetModelComboOutlineBrush())
                .BorderBackgroundColor(FStyleColors::Warning)
                .Padding(FMargin(1.0f))
                .Visibility_Lambda([this]()
                {
                    return HasPermissionRequest() ? EVisibility::Visible : EVisibility::Collapsed;
                })
                [
                    SNew(SBorder)
                    .Tag(FName(TEXT("UnrealAgent.PermissionBar")))
                    .BorderImage(GetHeaderBrush())
                    .BorderBackgroundColor(FStyleColors::Header)
                    .Padding(FMargin(8.0f, 6.0f))
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        .VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                            .Text(this, &SUnrealAgentPanel::GetPermissionText)
                            .ColorAndOpacity(FStyleColors::Warning)
                            .AutoWrapText(true)
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
                        [
                            SNew(SButton)
                            .Tag(FName(TEXT("UnrealAgent.Permission.AllowOnceButton")))
                            .Text(LOCTEXT("ApproveOnce", "Allow once"))
                            .OnClicked(this, &SUnrealAgentPanel::OnApprovePermissionClicked)
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
                        [
                            SNew(SButton)
                            .Tag(FName(TEXT("UnrealAgent.Permission.AllowAlwaysButton")))
                            .Text(LOCTEXT("ApproveAlways", "Always allow"))
                            .ToolTipText(LOCTEXT("ApproveAlwaysTooltip", "Allow this matching permission for the rest of the current OpenCode session when ACP offers that option."))
                            .IsEnabled(this, &SUnrealAgentPanel::CanApprovePermissionAlways)
                            .OnClicked(this, &SUnrealAgentPanel::OnApprovePermissionAlwaysClicked)
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
                        [
                            SNew(SButton)
                            .Tag(FName(TEXT("UnrealAgent.Permission.RejectButton")))
                            .Text(LOCTEXT("Reject", "Reject"))
                            .OnClicked(this, &SUnrealAgentPanel::OnRejectPermissionClicked)
                        ]
                    ]
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SBox)
                .Visibility(this, &SUnrealAgentPanel::GetConversationVisibility)
                [
                    MakeComposer(BottomPromptTextBox, BottomModelComboButton, BottomThinkingComboButton, BottomAgentComboButton, FName(TEXT("UnrealAgent.Composer.Footer")))
                ]
            ]
        ]
        ]
    ];

    RebuildChatHistoryList();
    AddTranscriptEntry(TEXT("System"), TEXT("Unreal Agent is ready. Connect when you want live OpenCode ACP help."));
}

void SUnrealAgentPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
    if (AcpClient.IsValid())
    {
        AcpClient->Tick();
    }
    if (!HasPermissionRequest())
    {
        bHasPendingPermission = false;
        PendingPermissionDescription.Reset();
    }
    if (ActiveReasoningEndSeconds.IsValid() && (!AcpClient.IsValid() || !AcpClient->IsPromptInFlight()))
    {
        FinalizeActiveReasoning();
    }
    FlushPendingTranscript();
}

FReply SUnrealAgentPanel::OnConnectClicked()
{
    if (!AcpClient.IsValid())
    {
        return FReply::Handled();
    }

    if (AcpClient->IsRunning())
    {
        AcpClient->Stop();
        SetStatus(FString());
        AddTranscriptEntry(TEXT("System"), TEXT("OpenCode ACP stopped."));
        return FReply::Handled();
    }

    const FString WorkingDirectory = FPaths::ProjectDir();
    if (AcpClient->Start(WorkingDirectory))
    {
        AcpClient->SetAttachEditorContext(bAttachEditorContext);
        AddTranscriptEntry(TEXT("System"), FString::Printf(TEXT("Starting opencode acp in %s"), *WorkingDirectory));
    }
    else
    {
        AddTranscriptEntry(TEXT("Error"), StatusText);
    }
    return FReply::Handled();
}

FReply SUnrealAgentPanel::OnSendClicked()
{
    if (AcpClient.IsValid() && AcpClient->IsPromptInFlight())
    {
        AcpClient->CancelPrompt();
        bHasPendingPermission = AcpClient->HasPendingPermission();
        if (!bHasPendingPermission)
        {
            PendingPermissionDescription.Reset();
        }
        return FReply::Handled();
    }

    const TSharedPtr<SMultiLineEditableTextBox> ActivePromptTextBox = GetActivePromptTextBox();
    if (!ActivePromptTextBox.IsValid() || !CanSendPrompt())
    {
        return FReply::Handled();
    }

    const FString Prompt = ActivePromptTextBox->GetText().ToString().TrimStartAndEnd();
    AcpClient->SetAttachEditorContext(bAttachEditorContext);
    if (AcpClient->SendPrompt(Prompt))
    {
        EnsureActiveChatEntry(Prompt, true);
        bHasConversationContent = true;
        LastUserPrompt = Prompt;
        ClearPromptTextBoxes();
    }
    else
    {
        const TSharedPtr<SMultiLineEditableTextBox> VisiblePromptTextBox = GetActivePromptTextBox();
        if (VisiblePromptTextBox.IsValid())
        {
            VisiblePromptTextBox->SetText(FText::FromString(Prompt));
        }
    }
    return FReply::Handled();
}

FReply SUnrealAgentPanel::OnClearChatClicked()
{
    StoreActiveContextWindowUsage();
    ResetTranscriptView();
    ActiveChatHistoryId = INDEX_NONE;
    LastUserPrompt.Reset();
    RebuildChatHistoryList();
    AddTranscriptEntry(TEXT("System"), TEXT("Chat cleared."));
    return FReply::Handled();
}

FReply SUnrealAgentPanel::OnNewChatClicked()
{
    if (AcpClient.IsValid() && AcpClient->IsPromptInFlight())
    {
        return FReply::Handled();
    }

    StoreActiveContextWindowUsage();
    ResetTranscriptView();
    ActiveChatHistoryId = INDEX_NONE;
    LastUserPrompt.Reset();
    RebuildChatHistoryList();
    AddTranscriptEntry(TEXT("System"), TEXT("Started a new chat."));
    return FReply::Handled();
}

FReply SUnrealAgentPanel::OnSidebarToggleClicked()
{
    bSidebarCollapsed = !bSidebarCollapsed;
    return FReply::Handled();
}

FReply SUnrealAgentPanel::OnRetryLastPromptClicked()
{
    if (!CanRetryLastPrompt() || !AcpClient.IsValid())
    {
        return FReply::Handled();
    }

    AcpClient->SetAttachEditorContext(bAttachEditorContext);
    AcpClient->SendPrompt(LastUserPrompt);
    return FReply::Handled();
}

FReply SUnrealAgentPanel::OnApprovePermissionClicked()
{
    if (AcpClient.IsValid())
    {
        AcpClient->ApprovePermissionOnce();
        bHasPendingPermission = AcpClient->HasPendingPermission();
    }
    if (!bHasPendingPermission)
    {
        PendingPermissionDescription.Reset();
    }
    return FReply::Handled();
}

FReply SUnrealAgentPanel::OnApprovePermissionAlwaysClicked()
{
    if (AcpClient.IsValid())
    {
        AcpClient->ApprovePermissionAlways();
        bHasPendingPermission = AcpClient->HasPendingPermission();
    }
    if (!bHasPendingPermission)
    {
        PendingPermissionDescription.Reset();
    }
    return FReply::Handled();
}

FReply SUnrealAgentPanel::OnRejectPermissionClicked()
{
    if (AcpClient.IsValid())
    {
        AcpClient->RejectPermission();
        bHasPendingPermission = AcpClient->HasPendingPermission();
    }
    if (!bHasPendingPermission)
    {
        PendingPermissionDescription.Reset();
    }
    return FReply::Handled();
}

FReply SUnrealAgentPanel::OnQuickPromptClicked(FString PromptText)
{
    const TSharedPtr<SMultiLineEditableTextBox> ActivePromptTextBox = GetActivePromptTextBox();
    if (ActivePromptTextBox.IsValid())
    {
        ActivePromptTextBox->SetText(FText::FromString(PromptText));
    }

    if (AcpClient.IsValid() && AcpClient->IsReady() && !AcpClient->IsPromptInFlight())
    {
        AcpClient->SetAttachEditorContext(bAttachEditorContext);
        if (AcpClient->SendPrompt(PromptText))
        {
            EnsureActiveChatEntry(PromptText, true);
            bHasConversationContent = true;
            LastUserPrompt = PromptText;
            ClearPromptTextBoxes();
        }
        else
        {
            const TSharedPtr<SMultiLineEditableTextBox> VisiblePromptTextBox = GetActivePromptTextBox();
            if (VisiblePromptTextBox.IsValid())
            {
                VisiblePromptTextBox->SetText(FText::FromString(PromptText));
            }
        }
    }

    return FReply::Handled();
}

FReply SUnrealAgentPanel::OnInspectContextClicked()
{
    if (!AcpClient.IsValid())
    {
        return FReply::Handled();
    }

    AcpClient->RefreshEditorContext();
    const FString Summary = AcpClient->GetLastEditorContextSummary().IsEmpty()
        ? FString(TEXT("Editor context refreshed."))
        : AcpClient->GetLastEditorContextSummary();
    AddTranscriptEntry(TEXT("Plan"), Summary);
    return FReply::Handled();
}

FReply SUnrealAgentPanel::OnValidateProjectClicked()
{
    if (AcpClient.IsValid())
    {
        AcpClient->RunProjectValidation();
    }
    return FReply::Handled();
}

void SUnrealAgentPanel::OnAttachContextCheckStateChanged(ECheckBoxState NewState)
{
    bAttachEditorContext = NewState == ECheckBoxState::Checked;
    if (AcpClient.IsValid())
    {
        AcpClient->SetAttachEditorContext(bAttachEditorContext);
    }
}

FReply SUnrealAgentPanel::OnPromptKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent)
{
    if (KeyEvent.GetKey() == EKeys::Enter && KeyEvent.IsShiftDown())
    {
        return OnSendClicked();
    }

    if (KeyEvent.GetKey() == EKeys::Escape && AcpClient.IsValid() && AcpClient->IsPromptInFlight())
    {
        return OnSendClicked();
    }

    return FReply::Unhandled();
}

FReply SUnrealAgentPanel::OnModelOptionClicked(TSharedPtr<FOpenCodeAcpModelOption> SelectedModel)
{
    OnModelSelected(SelectedModel);

    if (CenterModelComboButton.IsValid() && CenterModelComboButton->IsOpen())
    {
        CenterModelComboButton->SetIsOpen(false);
    }
    if (BottomModelComboButton.IsValid() && BottomModelComboButton->IsOpen())
    {
        BottomModelComboButton->SetIsOpen(false);
    }

    return FReply::Handled();
}

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

void SUnrealAgentPanel::OnModelSelected(TSharedPtr<FOpenCodeAcpModelOption> SelectedModel)
{
    SelectedModelOption = SelectedModel;

    if (!SelectedModel.IsValid() || !AcpClient.IsValid())
    {
        return;
    }

    AcpClient->SetModel(SelectedModel->Id);
}

FReply SUnrealAgentPanel::OnAgentOptionClicked(TSharedPtr<FOpenCodeAcpAgentOption> SelectedAgent)
{
    OnAgentSelected(SelectedAgent);

    if (CenterAgentComboButton.IsValid() && CenterAgentComboButton->IsOpen())
    {
        CenterAgentComboButton->SetIsOpen(false);
    }
    if (BottomAgentComboButton.IsValid() && BottomAgentComboButton->IsOpen())
    {
        BottomAgentComboButton->SetIsOpen(false);
    }

    return FReply::Handled();
}

FReply SUnrealAgentPanel::OnThinkingOptionClicked(TSharedPtr<FOpenCodeAcpThinkingOption> SelectedThinking)
{
    OnThinkingSelected(SelectedThinking);

    if (CenterThinkingComboButton.IsValid() && CenterThinkingComboButton->IsOpen())
    {
        CenterThinkingComboButton->SetIsOpen(false);
    }
    if (BottomThinkingComboButton.IsValid() && BottomThinkingComboButton->IsOpen())
    {
        BottomThinkingComboButton->SetIsOpen(false);
    }

    return FReply::Handled();
}

void SUnrealAgentPanel::OnThinkingSelected(TSharedPtr<FOpenCodeAcpThinkingOption> SelectedThinking)
{
    SelectedThinkingOption = SelectedThinking;

    if (!SelectedThinking.IsValid() || !AcpClient.IsValid())
    {
        return;
    }

    AcpClient->SetThinking(SelectedThinking->Id);
}

void SUnrealAgentPanel::OnAgentSelected(TSharedPtr<FOpenCodeAcpAgentOption> SelectedAgent)
{
    SelectedAgentOption = SelectedAgent;

    if (!SelectedAgent.IsValid() || !AcpClient.IsValid())
    {
        return;
    }

    AcpClient->SetAgent(SelectedAgent->Id);
}

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

bool SUnrealAgentPanel::IsContextWindowVisibleForAutomation() const
{
    return GetContextWindowVisibility() == EVisibility::Visible;
}

void SUnrealAgentPanel::OnModelMenuOpened()
{
    ModelSearchText.Reset();
    if (ModelSearchBox.IsValid())
    {
        ModelSearchBox->SetText(FText::GetEmpty());
    }
    RebuildFilteredModelOptions();
}

void SUnrealAgentPanel::OnModelSearchChanged(const FText& SearchText)
{
    ModelSearchText = SearchText.ToString();
    RebuildFilteredModelOptions();
}

bool SUnrealAgentPanel::CanSendPrompt() const
{
    const TSharedPtr<SMultiLineEditableTextBox> ActivePromptTextBox = GetActivePromptTextBox();
    if (!AcpClient.IsValid() || !AcpClient->IsReady() || AcpClient->IsPromptInFlight() || !ActivePromptTextBox.IsValid())
    {
        return false;
    }

    return !ActivePromptTextBox->GetText().ToString().TrimStartAndEnd().IsEmpty();
}

bool SUnrealAgentPanel::CanRetryLastPrompt() const
{
    return AcpClient.IsValid() && AcpClient->IsReady() && !AcpClient->IsPromptInFlight() && !LastUserPrompt.IsEmpty();
}

bool SUnrealAgentPanel::CanSelectModel() const
{
    return AcpClient.IsValid() && AcpClient->CanSelectModel();
}

bool SUnrealAgentPanel::CanSelectThinking() const
{
    return AcpClient.IsValid() && AcpClient->CanSelectThinking();
}

bool SUnrealAgentPanel::CanSelectAgent() const
{
    return AcpClient.IsValid() && AcpClient->CanSelectAgent();
}

bool SUnrealAgentPanel::CanApprovePermissionAlways() const
{
    return AcpClient.IsValid() && AcpClient->CanApprovePermissionAlways();
}

bool SUnrealAgentPanel::HasPermissionRequest() const
{
    return AcpClient.IsValid() && AcpClient->HasPendingPermission();
}

EVisibility SUnrealAgentPanel::GetEmptyStateVisibility() const
{
    return bHasConversationContent ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility SUnrealAgentPanel::GetConversationVisibility() const
{
    return bHasConversationContent ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SUnrealAgentPanel::GetInitialComposerVisibility() const
{
    return bHasConversationContent ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility SUnrealAgentPanel::GetModelControlsVisibility() const
{
    return AcpClient.IsValid() && AcpClient->IsReady() && (ModelOptions.Num() > 0 || AcpClient->GetThinkingOptions().Num() > 0 || AcpClient->GetAgentOptions().Num() > 0)
        ? EVisibility::Visible
        : EVisibility::Collapsed;
}

EVisibility SUnrealAgentPanel::GetThinkingSelectorVisibility() const
{
    return AcpClient.IsValid() && AcpClient->GetThinkingOptions().Num() > 0
        ? EVisibility::Visible
        : EVisibility::Collapsed;
}

FText SUnrealAgentPanel::GetStatusText() const
{
    return FText::FromString(StatusText);
}

FText SUnrealAgentPanel::GetPermissionText() const
{
    return PendingPermissionDescription.IsEmpty()
        ? LOCTEXT("PermissionWaiting", "OpenCode is waiting for tool permission.")
        : FText::FromString(PendingPermissionDescription);
}

FSlateColor SUnrealAgentPanel::GetStatusBadgeColor() const
{
    const FString LowerStatus = StatusText.ToLower();
    if (HasPermissionRequest())
    {
        return FStyleColors::Warning;
    }
    if (AcpClient.IsValid() && AcpClient->IsPromptInFlight())
    {
        return FStyleColors::AccentBlue;
    }
    if (LowerStatus.Contains(TEXT("error")) || LowerStatus.Contains(TEXT("failed")) || LowerStatus.Contains(TEXT("timed out")) || LowerStatus.Contains(TEXT("exited")))
    {
        return FStyleColors::Error;
    }
    if (AcpClient.IsValid() && AcpClient->IsReady())
    {
        return FStyleColors::Success;
    }
    if (AcpClient.IsValid() && AcpClient->IsRunning())
    {
        return FStyleColors::AccentBlue;
    }
    return FStyleColors::Foreground;
}

FSlateColor SUnrealAgentPanel::GetConnectionIndicatorColor() const
{
    const FString LowerStatus = StatusText.ToLower();
    if (LowerStatus.Contains(TEXT("error")) || LowerStatus.Contains(TEXT("failed")) || LowerStatus.Contains(TEXT("timed out")) || LowerStatus.Contains(TEXT("exited")))
    {
        return FStyleColors::Error;
    }
    if (AcpClient.IsValid() && AcpClient->IsReady())
    {
        return FStyleColors::Success;
    }
    if (AcpClient.IsValid() && AcpClient->IsRunning())
    {
        return FStyleColors::AccentBlue;
    }

    return FStyleColors::Error;
}

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

FText SUnrealAgentPanel::GetSelectedModelText() const
{
    if (SelectedModelOption.IsValid())
    {
        return FText::FromString(SelectedModelOption->GetDisplayName());
    }

    if (AcpClient.IsValid() && !AcpClient->GetCurrentModel().IsEmpty())
    {
        return FText::FromString(AcpClient->GetCurrentModel());
    }

    return FText::GetEmpty();
}

FText SUnrealAgentPanel::GetSelectedThinkingText() const
{
    if (SelectedThinkingOption.IsValid())
    {
        return FText::FromString(SelectedThinkingOption->GetDisplayName());
    }

    if (AcpClient.IsValid() && !AcpClient->GetCurrentThinking().IsEmpty())
    {
        return FText::FromString(AcpClient->GetCurrentThinking());
    }

    return FText::GetEmpty();
}

FText SUnrealAgentPanel::GetSelectedAgentText() const
{
    if (SelectedAgentOption.IsValid())
    {
        return FText::FromString(SelectedAgentOption->GetDisplayName());
    }

    if (AcpClient.IsValid() && !AcpClient->GetCurrentAgent().IsEmpty())
    {
        const FString& CurrentAgent = AcpClient->GetCurrentAgent();
        if (CurrentAgent == TEXT("unreal-agent"))
        {
            return LOCTEXT("UnrealCreatorAgent", "Unreal - Creator");
        }
        return FText::FromString(CurrentAgent);
    }

    return FText::GetEmpty();
}

FText SUnrealAgentPanel::GetConnectionButtonText() const
{
    if (AcpClient.IsValid() && AcpClient->IsRunning() && !AcpClient->IsReady())
    {
        const int32 DotCount = static_cast<int32>(FPlatformTime::Seconds() * 2.5) % 4;
        return FText::FromString(FString::Printf(TEXT("Connecting%s"), *FString::ChrN(DotCount, TCHAR('.'))));
    }
    if (AcpClient.IsValid() && AcpClient->IsRunning())
    {
        return LOCTEXT("ConnectionDisconnect", "Disconnect");
    }

    return LOCTEXT("ConnectionConnect", "Connect");
}

FText SUnrealAgentPanel::GetComposerHelperText() const
{
    if (HasPermissionRequest())
    {
        return LOCTEXT("ComposerHelperPermission", "Review the permission request above so OpenCode can continue.");
    }
    if (AcpClient.IsValid() && AcpClient->IsPromptInFlight())
    {
        return AcpClient->IsCancelRequested()
            ? LOCTEXT("ComposerHelperCancelling", "Cancellation requested. Waiting for OpenCode to finish the turn cleanly.")
            : LOCTEXT("ComposerHelperWorking", "OpenCode is working. Cancel turn asks ACP to stop the current prompt without ending the session.");
    }
    if (AcpClient.IsValid() && AcpClient->IsReady())
    {
        return LOCTEXT("ComposerHelperReady", "Enter adds a new line. Shift+Enter sends. Esc cancels the current turn while OpenCode is working.");
    }
    if (AcpClient.IsValid() && AcpClient->IsRunning())
    {
        return LOCTEXT("ComposerHelperStarting", "Loading models from OpenCode ACP...");
    }

    return FText::GetEmpty();
}

FText SUnrealAgentPanel::GetSidebarToggleText() const
{
    return bSidebarCollapsed ? LOCTEXT("ExpandSidebar", ">") : LOCTEXT("CollapseSidebar", "<");
}

FText SUnrealAgentPanel::GetChatHistoryEmptyText() const
{
    return LOCTEXT("NoChatHistory", "No chat history yet. Start a prompt, then use New Chat to keep it listed here.");
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

EVisibility SUnrealAgentPanel::GetExpandedSidebarVisibility() const
{
    return bSidebarCollapsed ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility SUnrealAgentPanel::GetCollapsedSidebarVisibility() const
{
    return bSidebarCollapsed ? EVisibility::Visible : EVisibility::Collapsed;
}

const FSlateBrush* SUnrealAgentPanel::GetSendButtonIconBrush() const
{
    return FAppStyle::Get().GetBrush(AcpClient.IsValid() && AcpClient->IsPromptInFlight() ? TEXT("Icons.X") : TEXT("Icons.ArrowRight"));
}

FSlateColor SUnrealAgentPanel::GetSendButtonIconColor() const
{
    if (AcpClient.IsValid() && AcpClient->IsCancelRequested())
    {
        return FSlateColor::UseSubduedForeground();
    }
    if (AcpClient.IsValid() && AcpClient->IsPromptInFlight())
    {
        return FStyleColors::Error;
    }
    return FSlateColor::UseForeground();
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

ECheckBoxState SUnrealAgentPanel::GetAttachContextCheckState() const
{
    return bAttachEditorContext ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SUnrealAgentPanel::SetStatus(const FString& NewStatus)
{
    StatusText = NewStatus;
}

TSharedRef<SWidget> SUnrealAgentPanel::MakeCockpit()
{
    return SNew(SBorder)
        .Tag(FName(TEXT("UnrealAgent.Cockpit")))
        .BorderImage(GetHeaderBrush())
        .Padding(FMargin(8.0f, 6.0f))
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SCheckBox)
                .Tag(FName(TEXT("UnrealAgent.Cockpit.ContextToggle")))
                .ToolTipText(LOCTEXT("AttachEditorContextTooltip", "Attach a compact redacted editor context envelope to each prompt."))
                .IsChecked(this, &SUnrealAgentPanel::GetAttachContextCheckState)
                .OnCheckStateChanged(this, &SUnrealAgentPanel::OnAttachContextCheckStateChanged)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("AttachEditorContextLabel", "Editor context"))
                    .ColorAndOpacity(FSlateColor::UseForeground())
                ]
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            .Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
            [
                SNew(STextBlock)
                .Tag(FName(TEXT("UnrealAgent.Cockpit.ContextPreview")))
                .Text(this, &SUnrealAgentPanel::GetEditorContextStatusText)
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
            [
                SNew(STextBlock)
                .Tag(FName(TEXT("UnrealAgent.Cockpit.StudioKitStatus")))
                .Text(this, &SUnrealAgentPanel::GetStudioKitStatusText)
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
            [
                SNew(SButton)
                .Tag(FName(TEXT("UnrealAgent.Cockpit.InspectContextButton")))
                .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
                .ContentPadding(FMargin(6.0f, 2.0f))
                .ToolTipText(LOCTEXT("InspectContextTooltip", "Refresh the editor context envelope now."))
                .OnClicked(this, &SUnrealAgentPanel::OnInspectContextClicked)
                [
                    SNew(SImage)
                    .Image(FAppStyle::Get().GetBrush("Icons.Search"))
                    .ColorAndOpacity(FSlateColor::UseForeground())
                    .DesiredSizeOverride(FVector2D(14.0f, 14.0f))
                ]
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
            [
                SNew(SButton)
                .Tag(FName(TEXT("UnrealAgent.Cockpit.ValidateButton")))
                .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
                .ContentPadding(FMargin(6.0f, 2.0f))
                .ToolTipText(this, &SUnrealAgentPanel::GetValidationStatusText)
                .OnClicked(this, &SUnrealAgentPanel::OnValidateProjectClicked)
                [
                    SNew(SImage)
                    .Image(FAppStyle::Get().GetBrush("Icons.Check"))
                    .ColorAndOpacity(FSlateColor::UseForeground())
                    .DesiredSizeOverride(FVector2D(14.0f, 14.0f))
                ]
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
            [
                SNew(STextBlock)
                .Tag(FName(TEXT("UnrealAgent.Cockpit.EvidenceStatus")))
                .Text(this, &SUnrealAgentPanel::GetValidationStatusText)
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
            ]
        ];
}

TSharedRef<SWidget> SUnrealAgentPanel::MakeSidebar()
{
    return SNew(SOverlay)
        .Tag(FName(TEXT("UnrealAgent.Sidebar")))
        + SOverlay::Slot()
        [
            MakeExpandedSidebar()
        ]
        + SOverlay::Slot()
        [
            MakeCollapsedSidebar()
        ];
}

TSharedRef<SWidget> SUnrealAgentPanel::MakeExpandedSidebar()
{
    return SNew(SBox)
        .Tag(FName(TEXT("UnrealAgent.Sidebar.Expanded")))
        .WidthOverride(260.0f)
        .Visibility(this, &SUnrealAgentPanel::GetExpandedSidebarVisibility)
        [
            SNew(SBorder)
            .BorderImage(GetSidebarBrush())
            .Padding(FMargin(10.0f, 8.0f, 8.0f, 8.0f))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    [
                        SNullWidget::NullWidget
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(SButton)
                        .Tag(FName(TEXT("UnrealAgent.Sidebar.ToggleButton")))
                        .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
                        .ContentPadding(FMargin(6.0f, 3.0f))
                        .ToolTipText(LOCTEXT("CollapseSidebarTooltip", "Collapse sidebar"))
                        .Text(this, &SUnrealAgentPanel::GetSidebarToggleText)
                        .OnClicked(this, &SUnrealAgentPanel::OnSidebarToggleClicked)
                    ]
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.0f, 4.0f, 0.0f, 8.0f))
                [
                    SNew(SButton)
                    .Tag(FName(TEXT("UnrealAgent.Sidebar.NewChatButton")))
                    .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
                    .HAlign(HAlign_Center)
                    .ContentPadding(FMargin(8.0f, 4.0f))
                    .ToolTipText(LOCTEXT("NewChatTooltip", "Start a fresh local chat view while keeping previous chats in the sidebar history."))
                    .IsEnabled_Lambda([this]()
                    {
                        return !AcpClient.IsValid() || !AcpClient->IsPromptInFlight();
                    })
                    .OnClicked(this, &SUnrealAgentPanel::OnNewChatClicked)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("NewChatButton", "New Chat"))
                        .Font(FAppStyle::Get().GetFontStyle("SmallBoldFont"))
                        .ColorAndOpacity(FSlateColor::UseForeground())
                    ]
                ]
                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                [
                    SNew(SScrollBox)
                    .Tag(FName(TEXT("UnrealAgent.Sidebar.History.Scroll")))
                    + SScrollBox::Slot()
                    [
                        SAssignNew(ChatHistoryList, SVerticalBox)
                        .Tag(FName(TEXT("UnrealAgent.Sidebar.History.List")))
                    ]
                ]
            ]
        ];
}

TSharedRef<SWidget> SUnrealAgentPanel::MakeCollapsedSidebar()
{
    return SNew(SBox)
        .Tag(FName(TEXT("UnrealAgent.Sidebar.Collapsed")))
        .WidthOverride(44.0f)
        .Visibility(this, &SUnrealAgentPanel::GetCollapsedSidebarVisibility)
        [
            SNew(SBorder)
            .BorderImage(GetSidebarBrush())
            .Padding(FMargin(5.0f, 8.0f))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SButton)
                    .Tag(FName(TEXT("UnrealAgent.Sidebar.Collapsed.ToggleButton")))
                    .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
                    .HAlign(HAlign_Center)
                    .ContentPadding(FMargin(4.0f, 3.0f))
                    .ToolTipText(LOCTEXT("ExpandSidebarTooltip", "Expand sidebar"))
                    .Text(this, &SUnrealAgentPanel::GetSidebarToggleText)
                    .OnClicked(this, &SUnrealAgentPanel::OnSidebarToggleClicked)
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.0f, 10.0f, 0.0f, 0.0f))
                [
                    SNew(SButton)
                    .Tag(FName(TEXT("UnrealAgent.Sidebar.Collapsed.NewChatButton")))
                    .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
                    .HAlign(HAlign_Center)
                    .ContentPadding(FMargin(5.0f, 3.0f))
                    .Text(LOCTEXT("CollapsedNewChatButton", "+"))
                    .ToolTipText(LOCTEXT("CollapsedNewChatTooltip", "New Chat"))
                    .IsEnabled_Lambda([this]()
                    {
                        return !AcpClient.IsValid() || !AcpClient->IsPromptInFlight();
                    })
                    .OnClicked(this, &SUnrealAgentPanel::OnNewChatClicked)
                ]
                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                [
                    SNullWidget::NullWidget
                ]
            ]
        ];
}

TSharedRef<SWidget> SUnrealAgentPanel::MakeChatHistoryRow(const FChatHistoryEntry& Entry)
{
    const bool bIsActive = Entry.Id == ActiveChatHistoryId;
    const bool bIsRenaming = Entry.Id == RenamingChatHistoryId;
    const FSlateColor ActionColor = FSlateColor::UseSubduedForeground();

    return SNew(SBorder)
        .Tag(FName(TEXT("UnrealAgent.Sidebar.History.RowContainer")))
        .BorderImage(bIsActive ? GetSidebarActiveChatBrush() : GetSidebarInactiveChatBrush())
        .Padding(FMargin(6.0f, 5.0f))
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                bIsRenaming
                    ? StaticCastSharedRef<SWidget>(
                        SNew(SEditableTextBox)
                        .Tag(FName(TEXT("UnrealAgent.Sidebar.History.RenameInput")))
                        .Text(FText::FromString(PendingRenameTitle.IsEmpty() ? Entry.Title : PendingRenameTitle))
                        .SelectAllTextWhenFocused(true)
                        .ClearKeyboardFocusOnCommit(false)
                        .OnTextChanged(this, &SUnrealAgentPanel::OnChatHistoryRenameTextChanged, Entry.Id)
                    )
                    : StaticCastSharedRef<SWidget>(
                        SNew(SButton)
                        .Tag(FName(TEXT("UnrealAgent.Sidebar.History.Row")))
                        .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
                        .ButtonColorAndOpacity(FStyleColors::Transparent)
                        .ContentPadding(FMargin(2.0f, 1.0f))
                        .HAlign(HAlign_Fill)
                        .ToolTipText(FText::FromString(Entry.Title))
                        .OnClicked(this, &SUnrealAgentPanel::OnChatHistoryEntryClicked, Entry.Id)
                        [
                            SNew(STextBlock)
                            .Tag(bIsActive ? FName(TEXT("UnrealAgent.Sidebar.History.ActiveTitle")) : FName(TEXT("UnrealAgent.Sidebar.History.Title")))
                            .Text(FText::FromString(Entry.Title))
                            .ToolTipText(FText::FromString(Entry.Title))
                            .Font(FAppStyle::Get().GetFontStyle("SmallBoldFont"))
                            .ColorAndOpacity(FSlateColor::UseForeground())
                            .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                        ]
                    )
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(5.0f, 0.0f, 0.0f, 0.0f))
            [
                bIsRenaming
                    ? StaticCastSharedRef<SWidget>(
                        SNew(SButton)
                        .Tag(FName(TEXT("UnrealAgent.Sidebar.History.RenameSaveButton")))
                        .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
                        .ContentPadding(FMargin(4.0f, 1.0f))
                        .ToolTipText(LOCTEXT("SaveChatRenameTooltip", "Save chat title"))
                        .OnClicked(this, &SUnrealAgentPanel::OnChatHistoryRenameSaveClicked, Entry.Id)
                        [
                            SNew(STextBlock)
                            .Text(LOCTEXT("SaveChatRename", "Save"))
                            .ColorAndOpacity(ActionColor)
                        ]
                    )
                    : StaticCastSharedRef<SWidget>(
                        SNew(SButton)
                        .Tag(FName(TEXT("UnrealAgent.Sidebar.History.RenameButton")))
                        .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
                        .ContentPadding(FMargin(4.0f))
                        .HAlign(HAlign_Center)
                        .VAlign(VAlign_Center)
                        .ToolTipText(LOCTEXT("RenameChatTooltip", "Rename chat"))
                        .OnClicked(this, &SUnrealAgentPanel::OnChatHistoryRenameClicked, Entry.Id)
                        [
                            SNew(SImage)
                            .Tag(FName(TEXT("UnrealAgent.Sidebar.History.RenameIcon")))
                            .Image(FAppStyle::Get().GetBrush("GenericCommands.Rename"))
                            .ColorAndOpacity(ActionColor)
                            .DesiredSizeOverride(FVector2D(16.0f, 16.0f))
                        ]
                    )
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(3.0f, 0.0f, 0.0f, 0.0f))
            [
                bIsRenaming
                    ? StaticCastSharedRef<SWidget>(
                        SNew(SButton)
                        .Tag(FName(TEXT("UnrealAgent.Sidebar.History.RenameCancelButton")))
                        .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
                        .ContentPadding(FMargin(4.0f, 1.0f))
                        .ToolTipText(LOCTEXT("CancelChatRenameTooltip", "Cancel rename"))
                        .OnClicked(this, &SUnrealAgentPanel::OnChatHistoryRenameCancelClicked)
                        [
                            SNew(STextBlock)
                            .Text(LOCTEXT("CancelChatRename", "Cancel"))
                            .ColorAndOpacity(ActionColor)
                        ]
                    )
                    : StaticCastSharedRef<SWidget>(
                        SNew(SButton)
                        .Tag(FName(TEXT("UnrealAgent.Sidebar.History.DeleteButton")))
                        .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
                        .ContentPadding(FMargin(4.0f))
                        .HAlign(HAlign_Center)
                        .VAlign(VAlign_Center)
                        .ToolTipText(LOCTEXT("DeleteChatTooltip", "Delete chat"))
                        .OnClicked(this, &SUnrealAgentPanel::OnChatHistoryDeleteClicked, Entry.Id)
                        [
                            SNew(SImage)
                            .Tag(FName(TEXT("UnrealAgent.Sidebar.History.DeleteIcon")))
                            .Image(FAppStyle::Get().GetBrush("Icons.Delete"))
                            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                            .DesiredSizeOverride(FVector2D(16.0f, 16.0f))
                        ]
                    )
            ]
        ];
}

TSharedRef<SWidget> SUnrealAgentPanel::MakeComposer(
    TSharedPtr<SMultiLineEditableTextBox>& OutPromptTextBox,
    TSharedPtr<SComboButton>& OutModelComboButton,
    TSharedPtr<SComboButton>& OutThinkingComboButton,
    TSharedPtr<SComboButton>& OutAgentComboButton,
    const FName& ComposerTag)
{
    TSharedRef<SVerticalBox> Composer = SNew(SVerticalBox)
        .Tag(ComposerTag)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FMargin(4.0f, 0.0f, 0.0f, 2.0f))
        [
            SNew(SBox)
            .HeightOverride(84.0f)
            [
                SNew(SOverlay)
                .Tag(FName(TEXT("UnrealAgent.Composer.InputFrame")))
                + SOverlay::Slot()
                [
                    SAssignNew(OutPromptTextBox, SMultiLineEditableTextBox)
                    .Tag(FName(TEXT("UnrealAgent.Composer.Input")))
                    .HintText(LOCTEXT("PromptHint", "Ask anything... \"Help me build an editor tool\""))
                    .AutoWrapText(true)
                    .WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)
                    .AllowMultiLine(true)
                    .ClearKeyboardFocusOnCommit(false)
                    .Padding(FMargin(7.0f, 6.0f, 38.0f, 8.0f))
                    .HScrollBar(MakeHiddenScrollBar(Orient_Horizontal))
                    .VScrollBar(MakeHiddenScrollBar(Orient_Vertical))
                    .HScrollBarPadding(FMargin(0.0f))
                    .VScrollBarPadding(FMargin(0.0f))
                    .OnKeyDownHandler(this, &SUnrealAgentPanel::OnPromptKeyDown)
                ]
                + SOverlay::Slot()
                .HAlign(HAlign_Right)
                .VAlign(VAlign_Bottom)
                .Padding(FMargin(0.0f, 0.0f, 6.0f, 6.0f))
                [
                    SNew(SBox)
                    .WidthOverride(36.0f)
                    .HeightOverride(28.0f)
                    [
                        SNew(SButton)
                        .Tag(FName(TEXT("UnrealAgent.Composer.SendButton")))
                        .ToolTipText(this, &SUnrealAgentPanel::GetComposerHelperText)
                        .HAlign(HAlign_Center)
                        .VAlign(VAlign_Center)
                        .IsEnabled_Lambda([this]()
                        {
                            return (AcpClient.IsValid() && AcpClient->IsPromptInFlight() && !AcpClient->IsCancelRequested()) || CanSendPrompt();
                        })
                        .OnClicked(this, &SUnrealAgentPanel::OnSendClicked)
                        [
                            SNew(SImage)
                            .Image(this, &SUnrealAgentPanel::GetSendButtonIconBrush)
                            .ColorAndOpacity(this, &SUnrealAgentPanel::GetSendButtonIconColor)
                            .DesiredSizeOverride(FVector2D(14.0f, 14.0f))
                        ]
                    ]
                ]
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FMargin(4.0f, 0.0f, 0.0f, 4.0f))
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SHorizontalBox)
                .Tag(FName(TEXT("UnrealAgent.Composer.ActionRow")))
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SHorizontalBox)
                    .Tag(FName(TEXT("UnrealAgent.Composer.ModelControls")))
                    .Visibility(this, &SUnrealAgentPanel::GetModelControlsVisibility)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(SBox)
                        .WidthOverride(156.0f)
                        [
                            SAssignNew(OutAgentComboButton, SComboButton)
                            .Tag(FName(TEXT("UnrealAgent.Agent.Combo")))
                            .ComboButtonStyle(GetTransparentModelComboButtonStyle())
                            .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
                            .ButtonColorAndOpacity(FStyleColors::Transparent)
                            .ForegroundColor(FSlateColor::UseForeground())
                            .ContentPadding(FMargin(0.0f))
                            .HasDownArrow(false)
                            .MenuPlacement(MenuPlacement_AboveAnchor)
                            .Method(EPopupMethod::UseCurrentWindow)
                            .OnGetMenuContent(this, &SUnrealAgentPanel::MakeAgentMenuContent)
                            .IsEnabled(this, &SUnrealAgentPanel::CanSelectAgent)
                            .ButtonContent()
                            [
                                SNew(SBorder)
                                .BorderImage(GetModelComboOutlineBrush())
                                .Padding(FMargin(8.0f, 3.0f, 6.0f, 3.0f))
                                [
                                    SNew(SHorizontalBox)
                                    + SHorizontalBox::Slot()
                                    .FillWidth(1.0f)
                                    .VAlign(VAlign_Center)
                                    [
                                        SNew(STextBlock)
                                        .Text(this, &SUnrealAgentPanel::GetSelectedAgentText)
                                        .ToolTipText(this, &SUnrealAgentPanel::GetSelectedAgentText)
                                        .ColorAndOpacity(FSlateColor::UseForeground())
                                        .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                                    ]
                                    + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .VAlign(VAlign_Center)
                                    .Padding(FMargin(7.0f, 0.0f, 0.0f, 0.0f))
                                    [
                                        SNew(SImage)
                                        .Image(FAppStyle::Get().GetBrush("Icons.ChevronDown"))
                                        .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                                        .DesiredSizeOverride(FVector2D(9.0f, 9.0f))
                                    ]
                                ]
                            ]
                        ]
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
                    [
                        SNew(SBox)
                        .WidthOverride(196.0f)
                        [
                            SAssignNew(OutModelComboButton, SComboButton)
                            .Tag(FName(TEXT("UnrealAgent.Model.Combo")))
                            .ComboButtonStyle(GetTransparentModelComboButtonStyle())
                            .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
                            .ButtonColorAndOpacity(FStyleColors::Transparent)
                            .ForegroundColor(FSlateColor::UseForeground())
                            .ContentPadding(FMargin(0.0f))
                            .HasDownArrow(false)
                            .MenuPlacement(MenuPlacement_AboveAnchor)
                            .Method(EPopupMethod::UseCurrentWindow)
                            .OnComboBoxOpened(this, &SUnrealAgentPanel::OnModelMenuOpened)
                            .OnGetMenuContent(this, &SUnrealAgentPanel::MakeModelMenuContent)
                            .IsEnabled(this, &SUnrealAgentPanel::CanSelectModel)
                            .ButtonContent()
                            [
                                SNew(SBorder)
                                .BorderImage(GetModelComboOutlineBrush())
                                .Padding(FMargin(8.0f, 3.0f, 6.0f, 3.0f))
                                [
                                    SNew(SHorizontalBox)
                                    + SHorizontalBox::Slot()
                                    .FillWidth(1.0f)
                                    .VAlign(VAlign_Center)
                                    [
                                        SNew(STextBlock)
                                        .Text(this, &SUnrealAgentPanel::GetSelectedModelText)
                                        .ToolTipText(this, &SUnrealAgentPanel::GetSelectedModelText)
                                        .ColorAndOpacity(FSlateColor::UseForeground())
                                        .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                                    ]
                                    + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .VAlign(VAlign_Center)
                                    .Padding(FMargin(7.0f, 0.0f, 0.0f, 0.0f))
                                    [
                                        SNew(SImage)
                                        .Image(FAppStyle::Get().GetBrush("Icons.ChevronDown"))
                                        .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                                        .DesiredSizeOverride(FVector2D(9.0f, 9.0f))
                                    ]
                                ]
                            ]
                        ]
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
                    [
                        SNew(SBox)
                        .WidthOverride(136.0f)
                        .Visibility(this, &SUnrealAgentPanel::GetThinkingSelectorVisibility)
                        [
                            SAssignNew(OutThinkingComboButton, SComboButton)
                            .Tag(FName(TEXT("UnrealAgent.Thinking.Combo")))
                            .ComboButtonStyle(GetTransparentModelComboButtonStyle())
                            .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
                            .ButtonColorAndOpacity(FStyleColors::Transparent)
                            .ForegroundColor(FSlateColor::UseForeground())
                            .ContentPadding(FMargin(0.0f))
                            .HasDownArrow(false)
                            .MenuPlacement(MenuPlacement_AboveAnchor)
                            .Method(EPopupMethod::UseCurrentWindow)
                            .OnGetMenuContent(this, &SUnrealAgentPanel::MakeThinkingMenuContent)
                            .IsEnabled(this, &SUnrealAgentPanel::CanSelectThinking)
                            .ButtonContent()
                            [
                                SNew(SBorder)
                                .BorderImage(GetModelComboOutlineBrush())
                                .Padding(FMargin(8.0f, 3.0f, 6.0f, 3.0f))
                                [
                                    SNew(SHorizontalBox)
                                    + SHorizontalBox::Slot()
                                    .FillWidth(1.0f)
                                    .VAlign(VAlign_Center)
                                    [
                                        SNew(STextBlock)
                                        .Text(this, &SUnrealAgentPanel::GetSelectedThinkingText)
                                        .ToolTipText(this, &SUnrealAgentPanel::GetSelectedThinkingText)
                                        .ColorAndOpacity(FSlateColor::UseForeground())
                                        .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                                    ]
                                    + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    .VAlign(VAlign_Center)
                                    .Padding(FMargin(7.0f, 0.0f, 0.0f, 0.0f))
                                    [
                                        SNew(SImage)
                                        .Image(FAppStyle::Get().GetBrush("Icons.ChevronDown"))
                                        .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                                        .DesiredSizeOverride(FVector2D(9.0f, 9.0f))
                                    ]
                                ]
                            ]
                        ]
                    ]
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNullWidget::NullWidget
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .HAlign(HAlign_Right)
                .VAlign(VAlign_Center)
                .Padding(FMargin(12.0f, 0.0f, 0.0f, 0.0f))
                [
                    SNew(SBorder)
                    .Tag(FName(TEXT("UnrealAgent.Composer.ContextWindow")))
                    .BorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
                    .Padding(FMargin(0.0f))
                    .Visibility(this, &SUnrealAgentPanel::GetContextWindowVisibility)
                    .ToolTipText(this, &SUnrealAgentPanel::GetContextWindowDetailText)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(SBox)
                            .Tag(FName(TEXT("UnrealAgent.Composer.ContextWindow.Indicator")))
                            .WidthOverride(12.0f)
                            .HeightOverride(12.0f)
                            .HAlign(HAlign_Center)
                            .VAlign(VAlign_Center)
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("ContextWindowIndicatorRing", "○"))
                                .Font(FAppStyle::Get().GetFontStyle("SmallBoldFont"))
                                .ColorAndOpacity(this, &SUnrealAgentPanel::GetContextWindowIndicatorColor)
                            ]
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
                        [
                            SNew(STextBlock)
                            .Tag(FName(TEXT("UnrealAgent.Composer.ContextWindow.Status")))
                            .Text(this, &SUnrealAgentPanel::GetContextWindowStatusText)
                            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                            .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                        ]
                    ]
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Fill)
            .Padding(FMargin(0.0f, 8.0f, 0.0f, 2.0f))
            [
                SNew(SHorizontalBox)
                .Tag(FName(TEXT("UnrealAgent.Composer.HelperRow")))
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNullWidget::NullWidget
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .HAlign(HAlign_Center)
                [
                    SNew(SBox)
                    .WidthOverride(520.0f)
                    [
                        SNew(STextBlock)
                        .Tag(FName(TEXT("UnrealAgent.Composer.Helper")))
                        .Text(this, &SUnrealAgentPanel::GetComposerHelperText)
                        .Justification(ETextJustify::Center)
                        .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                        .AutoWrapText(true)
                    ]
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNullWidget::NullWidget
                ]
            ]
        ];

    if (OutModelComboButton.IsValid())
    {
        OutModelComboButton->bShowMenuBackground = true;
    }
    if (OutThinkingComboButton.IsValid())
    {
        OutThinkingComboButton->bShowMenuBackground = true;
    }
    if (OutAgentComboButton.IsValid())
    {
        OutAgentComboButton->bShowMenuBackground = true;
    }

    return Composer;
}

TSharedRef<SWidget> SUnrealAgentPanel::MakeModelMenuContent()
{
    ModelSearchText.Reset();
    RebuildFilteredModelOptions();

    TSharedRef<SWidget> MenuContent = SNew(SBox)
        .WidthOverride(286.0f)
        .MaxDesiredHeight(320.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin(8.0f, 8.0f, 8.0f, 8.0f))
            [
                SNew(SHorizontalBox)
                .Tag(FName(TEXT("UnrealAgent.Model.MenuHeader")))
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SAssignNew(ModelSearchBox, SSearchBox)
                    .Tag(FName(TEXT("UnrealAgent.Model.Search")))
                    .HintText(LOCTEXT("ModelSearchHint", "Search models"))
                    .InitialText(FText::FromString(ModelSearchText))
                    .OnTextChanged(this, &SUnrealAgentPanel::OnModelSearchChanged)
                    .DelayChangeNotificationsWhileTyping(false)
                ]
            ]
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            .Padding(FMargin(8.0f, 0.0f, 8.0f, 10.0f))
            [
                SNew(SScrollBox)
                .Tag(FName(TEXT("UnrealAgent.Model.Scroll")))
                + SScrollBox::Slot()
                [
                    SAssignNew(ModelMenuList, SVerticalBox)
                    .Tag(FName(TEXT("UnrealAgent.Model.List")))
                ]
            ]
        ];

    if (CenterModelComboButton.IsValid())
    {
        CenterModelComboButton->SetMenuContentWidgetToFocus(ModelSearchBox);
    }
    if (BottomModelComboButton.IsValid())
    {
        BottomModelComboButton->SetMenuContentWidgetToFocus(ModelSearchBox);
    }

    PopulateModelMenuList();
    return MenuContent;
}

TSharedRef<SWidget> SUnrealAgentPanel::MakeModelMenuEntry(TSharedPtr<FOpenCodeAcpModelOption> ModelOption)
{
    const bool bIsSelected = ModelOption.IsValid() && SelectedModelOption.IsValid() && ModelOption->Id == SelectedModelOption->Id;

    return SNew(SButton)
        .Tag(FName(TEXT("UnrealAgent.Model.Row")))
        .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
        .ContentPadding(FMargin(0.0f))
        .OnClicked(this, &SUnrealAgentPanel::OnModelOptionClicked, ModelOption)
        [
            SNew(SBorder)
            .BorderImage(bIsSelected ? GetMenuSelectionBrush() : FCoreStyle::Get().GetBrush("NoBrush"))
            .Padding(FMargin(10.0f, 7.0f))
            [
                SNew(STextBlock)
                .Text(ModelOption.IsValid() ? FText::FromString(ModelOption->GetDisplayName()) : LOCTEXT("MissingModelOption", "Unknown model"))
                .ToolTipText(ModelOption.IsValid() ? FText::FromString(FString::Printf(TEXT("%s · %s"), *ModelOption->GetProviderName(), *ModelOption->Id)) : FText::GetEmpty())
                .HighlightText(FText::FromString(ModelSearchText))
                .ColorAndOpacity(FSlateColor::UseForeground())
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
            ]
        ];
}

TSharedRef<SWidget> SUnrealAgentPanel::MakeThinkingMenuContent()
{
    TSharedRef<SVerticalBox> ThinkingMenuList = SNew(SVerticalBox)
        .Tag(FName(TEXT("UnrealAgent.Thinking.List")));

    PopulateThinkingMenuList(ThinkingMenuList);

    return SNew(SBox)
        .WidthOverride(204.0f)
        .MaxDesiredHeight(220.0f)
        [
            SNew(SScrollBox)
            .Tag(FName(TEXT("UnrealAgent.Thinking.Scroll")))
            + SScrollBox::Slot()
            [
                ThinkingMenuList
            ]
        ];
}

TSharedRef<SWidget> SUnrealAgentPanel::MakeThinkingMenuEntry(TSharedPtr<FOpenCodeAcpThinkingOption> ThinkingOption)
{
    const bool bIsSelected = ThinkingOption.IsValid() && SelectedThinkingOption.IsValid() && ThinkingOption->Id == SelectedThinkingOption->Id;
    const FText Label = ThinkingOption.IsValid() ? FText::FromString(ThinkingOption->GetDisplayName()) : LOCTEXT("MissingThinkingOption", "Unknown thinking");
    const FText Tooltip = ThinkingOption.IsValid()
        ? FText::FromString(ThinkingOption->Description.IsEmpty() ? ThinkingOption->Id : FString::Printf(TEXT("%s\n%s"), *ThinkingOption->Id, *ThinkingOption->Description))
        : FText::GetEmpty();

    return SNew(SButton)
        .Tag(FName(TEXT("UnrealAgent.Thinking.Row")))
        .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
        .ContentPadding(FMargin(0.0f))
        .OnClicked(this, &SUnrealAgentPanel::OnThinkingOptionClicked, ThinkingOption)
        [
            SNew(SBorder)
            .BorderImage(bIsSelected ? GetMenuSelectionBrush() : FCoreStyle::Get().GetBrush("NoBrush"))
            .Padding(FMargin(10.0f, 7.0f))
            [
                SNew(STextBlock)
                .Text(Label)
                .ToolTipText(Tooltip)
                .ColorAndOpacity(FSlateColor::UseForeground())
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
            ]
        ];
}

TSharedRef<SWidget> SUnrealAgentPanel::MakeAgentMenuContent()
{
    TSharedRef<SVerticalBox> AgentMenuList = SNew(SVerticalBox)
        .Tag(FName(TEXT("UnrealAgent.Agent.List")));

    PopulateAgentMenuList(AgentMenuList);

    return SNew(SBox)
        .WidthOverride(236.0f)
        .MaxDesiredHeight(260.0f)
        [
            SNew(SScrollBox)
            .Tag(FName(TEXT("UnrealAgent.Agent.Scroll")))
            + SScrollBox::Slot()
            [
                AgentMenuList
            ]
        ];
}

TSharedRef<SWidget> SUnrealAgentPanel::MakeAgentMenuEntry(TSharedPtr<FOpenCodeAcpAgentOption> AgentOption)
{
    const bool bIsSelected = AgentOption.IsValid() && SelectedAgentOption.IsValid() && AgentOption->Id == SelectedAgentOption->Id;
    const FText Label = AgentOption.IsValid() ? FText::FromString(AgentOption->GetDisplayName()) : LOCTEXT("MissingAgentOption", "Unknown agent");
    const FText Tooltip = AgentOption.IsValid()
        ? FText::FromString(AgentOption->Description.IsEmpty() ? AgentOption->Id : FString::Printf(TEXT("%s\n%s"), *AgentOption->Id, *AgentOption->Description))
        : FText::GetEmpty();

    return SNew(SButton)
        .Tag(FName(TEXT("UnrealAgent.Agent.Row")))
        .ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
        .ContentPadding(FMargin(0.0f))
        .OnClicked(this, &SUnrealAgentPanel::OnAgentOptionClicked, AgentOption)
        [
            SNew(SBorder)
            .BorderImage(bIsSelected ? GetMenuSelectionBrush() : FCoreStyle::Get().GetBrush("NoBrush"))
            .Padding(FMargin(10.0f, 7.0f))
            [
                SNew(STextBlock)
                .Text(Label)
                .ToolTipText(Tooltip)
                .ColorAndOpacity(FSlateColor::UseForeground())
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
            ]
        ];
}

void SUnrealAgentPanel::PopulateModelMenuList()
{
    if (!ModelMenuList.IsValid())
    {
        return;
    }

    ModelMenuList->ClearChildren();

    FString LastProviderName;
    for (const TSharedPtr<FOpenCodeAcpModelOption>& ModelOption : FilteredModelOptions)
    {
        if (!ModelOption.IsValid())
        {
            continue;
        }

        const FString ProviderName = ModelOption->GetProviderName();
        if (ProviderName != LastProviderName)
        {
            LastProviderName = ProviderName;
            ModelMenuList->AddSlot()
            .AutoHeight()
            .Padding(FMargin(4.0f, 10.0f, 4.0f, 5.0f))
            [
                SNew(SBorder)
                .BorderImage(GetModelProviderHeaderBrush())
                .Padding(FMargin(9.0f, 4.0f))
                [
                    SNew(STextBlock)
                    .Tag(FName(TEXT("UnrealAgent.Model.ProviderHeader")))
                    .Text(FText::FromString(ProviderName.ToUpper()))
                    .Font(FAppStyle::Get().GetFontStyle("SmallBoldFont"))
                    .ColorAndOpacity(FSlateColor::UseForeground())
                ]
            ];
        }

        ModelMenuList->AddSlot()
        .AutoHeight()
        .Padding(FMargin(4.0f, 2.0f))
        [
            MakeModelMenuEntry(ModelOption)
        ];
    }

    if (FilteredModelOptions.Num() == 0)
    {
        ModelMenuList->AddSlot()
        .AutoHeight()
        .Padding(FMargin(10.0f, 14.0f))
        [
            SNew(STextBlock)
            .Tag(FName(TEXT("UnrealAgent.Model.Empty")))
            .Text(LOCTEXT("NoModelsMatchSearch", "No matching models"))
            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
        ];
    }
}

void SUnrealAgentPanel::PopulateThinkingMenuList(TSharedRef<SVerticalBox> ThinkingMenuList)
{
    if (!AcpClient.IsValid() || AcpClient->GetThinkingOptions().IsEmpty())
    {
        ThinkingMenuList->AddSlot()
        .AutoHeight()
        .Padding(FMargin(10.0f, 14.0f))
        [
            SNew(STextBlock)
            .Tag(FName(TEXT("UnrealAgent.Thinking.Empty")))
            .Text(LOCTEXT("NoThinkingOptionsAvailable", "No thinking options available"))
            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
        ];
        return;
    }

    TArray<TSharedPtr<FOpenCodeAcpThinkingOption>> ThinkingOptions;
    for (const FOpenCodeAcpThinkingOption& ThinkingOption : AcpClient->GetThinkingOptions())
    {
        ThinkingOptions.Add(MakeShared<FOpenCodeAcpThinkingOption>(ThinkingOption));
    }


    for (const TSharedPtr<FOpenCodeAcpThinkingOption>& ThinkingOption : ThinkingOptions)
    {
        ThinkingMenuList->AddSlot()
        .AutoHeight()
        .Padding(FMargin(4.0f, 2.0f))
        [
            MakeThinkingMenuEntry(ThinkingOption)
        ];
    }
}

void SUnrealAgentPanel::PopulateAgentMenuList(TSharedRef<SVerticalBox> AgentMenuList)
{
    if (!AcpClient.IsValid() || AcpClient->GetAgentOptions().IsEmpty())
    {
        AgentMenuList->AddSlot()
        .AutoHeight()
        .Padding(FMargin(10.0f, 14.0f))
        [
            SNew(STextBlock)
            .Tag(FName(TEXT("UnrealAgent.Agent.Empty")))
            .Text(LOCTEXT("NoAgentsAvailable", "No agents available"))
            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
        ];
        return;
    }

    TArray<TSharedPtr<FOpenCodeAcpAgentOption>> AgentOptions;
    for (const FOpenCodeAcpAgentOption& AgentOption : AcpClient->GetAgentOptions())
    {
        AgentOptions.Add(MakeShared<FOpenCodeAcpAgentOption>(AgentOption));
    }

    AgentOptions.Sort([](const TSharedPtr<FOpenCodeAcpAgentOption>& Left, const TSharedPtr<FOpenCodeAcpAgentOption>& Right)
    {
        if (!Left.IsValid() || !Right.IsValid())
        {
            return Left.IsValid();
        }
        return Left->GetDisplayName().Compare(Right->GetDisplayName(), ESearchCase::IgnoreCase) < 0;
    });

    for (const TSharedPtr<FOpenCodeAcpAgentOption>& AgentOption : AgentOptions)
    {
        AgentMenuList->AddSlot()
        .AutoHeight()
        .Padding(FMargin(4.0f, 2.0f))
        [
            MakeAgentMenuEntry(AgentOption)
        ];
    }
}

TSharedPtr<SMultiLineEditableTextBox> SUnrealAgentPanel::GetActivePromptTextBox() const
{
    return bHasConversationContent ? BottomPromptTextBox : CenterPromptTextBox;
}

void SUnrealAgentPanel::ClearPromptTextBoxes()
{
    if (CenterPromptTextBox.IsValid())
    {
        CenterPromptTextBox->SetText(FText::GetEmpty());
    }
    if (BottomPromptTextBox.IsValid())
    {
        BottomPromptTextBox->SetText(FText::GetEmpty());
    }
}

void SUnrealAgentPanel::RefreshModelComboButtons()
{
    PopulateModelMenuList();
}

void SUnrealAgentPanel::RebuildFilteredModelOptions()
{
    FilteredModelOptions.Reset();

    for (const TSharedPtr<FOpenCodeAcpModelOption>& ModelOption : ModelOptions)
    {
        if (!ModelOption.IsValid())
        {
            continue;
        }

        if (ModelSearchText.IsEmpty()
            || ModelOption->GetDisplayName().Contains(ModelSearchText, ESearchCase::IgnoreCase)
            || ModelOption->GetProviderName().Contains(ModelSearchText, ESearchCase::IgnoreCase)
            || ModelOption->Id.Contains(ModelSearchText, ESearchCase::IgnoreCase))
        {
            FilteredModelOptions.Add(ModelOption);
        }
    }

    RefreshModelComboButtons();
}

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

void SUnrealAgentPanel::AddTranscriptEntry(const FString& Role, const FString& Text)
{
    if (Role == TEXT("System"))
    {
        FlushPendingTranscript(true);
        return;
    }

    if (!IsConversationRole(Role))
    {
        FlushPendingTranscript(true);
        return;
    }

    if (IsStreamTranscriptRole(Role))
    {
        if (!PendingTranscriptText.IsEmpty() && PendingTranscriptRole != Role)
        {
            FlushPendingTranscript(true);
            PendingTranscriptText.Reset();
        }

        PendingTranscriptRole = Role;
        PendingTranscriptText = ClampTranscriptText(PendingTranscriptText + Text);
        return;
    }

    FlushPendingTranscript(true);
    AddTranscriptEntryImmediately(Role, Text);
}

void SUnrealAgentPanel::AddTranscriptEntryImmediately(const FString& Role, const FString& Text)
{
    if (Role == TEXT("System") || !IsConversationRole(Role) || !TranscriptScrollBox.IsValid() || Text.IsEmpty())
    {
        return;
    }

    TSharedPtr<SHorizontalBox> EntryWidget;
    TSharedPtr<STextBlock> EntryTextBlock;
    const FLinearColor AccentColor = RoleAccentColor(Role);
    const bool bUserEntry = IsUserTranscriptRole(Role);
    const bool bActivityEntry = IsActivityTranscriptRole(Role);
    const bool bReasoningEntry = Role == TEXT("Thought");
    const EHorizontalAlignment EntryAlignment = bUserEntry ? HAlign_Right : HAlign_Fill;
    const ETextJustify::Type TextJustification = bUserEntry ? ETextJustify::Right : ETextJustify::Left;
    const FText RoleLabel = RoleLabelText(Role);
    const bool bShowRoleLabel = !RoleLabel.IsEmpty();
    const FString RawText = ClampTranscriptText(Text);
    const FString DisplayText = RenderTranscriptText(Role, RawText);

    if (bActivityEntry)
    {
        if (Role == TEXT("Tool") && !ParseToolActivityDisplay(RawText).bShouldShow)
        {
            return;
        }

        bHasConversationContent = true;
        if (!bRestoringChatHistory)
        {
            UpdateActiveChatSummary(Role, RawText, true);
        }

        if (LastTranscriptRole == TEXT("Activity") && ActiveActivityBodyBox.IsValid())
        {
            if (!ActiveReasoningStartedSeconds.IsValid() || *ActiveReasoningStartedSeconds <= 0.0)
            {
                ActiveReasoningStartedSeconds = MakeShared<double>(FPlatformTime::Seconds());
                ActiveReasoningEndSeconds = MakeShared<double>(0.0);
            }
            if (bReasoningEntry)
            {
                if (ActiveActivityHasReasoning.IsValid())
                {
                    *ActiveActivityHasReasoning = true;
                }
            }
            if (ActiveActivityUpdateCount.IsValid())
            {
                ++(*ActiveActivityUpdateCount);
            }

            AppendActivityEntryToActive(Role, RawText);
            TranscriptScrollBox->ScrollToEnd();
            return;
        }

        ResetActiveActivityState();
        ActiveReasoningStartedSeconds = MakeShared<double>(FPlatformTime::Seconds());
        ActiveReasoningEndSeconds = MakeShared<double>(0.0);
        ActiveActivityHasReasoning = MakeShared<bool>(bReasoningEntry);
        ActiveActivityUpdateCount = MakeShared<int32>(1);

        TranscriptScrollBox->AddSlot()
        .Padding(FMargin(0.0f, 0.0f, 0.0f, 10.0f))
        [
            SAssignNew(EntryWidget, SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .HAlign(HAlign_Fill)
            [
                SNew(SBox)
                .MaxDesiredWidth(TranscriptEntryWidth)
                [
                    SNew(SExpandableArea)
                    .Tag(FName(TEXT("UnrealAgent.Transcript.Working")))
                    .InitiallyCollapsed(true)
                    .AllowAnimatedTransition(false)
                    .BorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
                    .BodyBorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
                    .BorderBackgroundColor(FStyleColors::Transparent)
                    .BodyBorderBackgroundColor(FStyleColors::Transparent)
                    .HeaderPadding(FMargin(0.0f))
                    .Padding(FMargin(18.0f, 6.0f, 0.0f, 0.0f))
                    .HeaderContent()
                    [
                        SNew(STextBlock)
                        .Tag(FName(TEXT("UnrealAgent.Transcript.Working.Header")))
                        .Text_Lambda([StartedAtSeconds = ActiveReasoningStartedSeconds, EndSeconds = ActiveReasoningEndSeconds, bHasReasoning = ActiveActivityHasReasoning, UpdateCount = ActiveActivityUpdateCount]()
                        {
                            return ActivityTitleText(StartedAtSeconds, EndSeconds, bHasReasoning, UpdateCount);
                        })
                        .Font(FAppStyle::Get().GetFontStyle("SmallFont"))
                        .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                    ]
                    .BodyContent()
                    [
                        SAssignNew(ActiveActivityBodyBox, SVerticalBox)
                        .Tag(FName(TEXT("UnrealAgent.Transcript.Working.Body")))
                    ]
                ]
            ]
        ];

        TranscriptEntryWidgets.Add(EntryWidget);
        LastTranscriptRole = TEXT("Activity");
        AppendActivityEntryToActive(Role, RawText);
        TrimTranscriptHistory();
        TranscriptScrollBox->ScrollToEnd();
        return;
    }

    bHasConversationContent = true;

    if (LastTranscriptRole == TEXT("Activity"))
    {
        FinalizeActiveReasoning();
    }

    if (ShouldAppendToLastTranscriptEntry(Role))
    {
        LastTranscriptText = ClampTranscriptText(LastTranscriptText + Text);
        LastTranscriptTextBlock->SetText(FText::FromString(RenderTranscriptText(Role, LastTranscriptText)));
        if (!bRestoringChatHistory)
        {
            UpdateActiveChatSummary(Role, LastTranscriptText, false);
        }
        TranscriptScrollBox->ScrollToEnd();
        return;
    }

    if (!bRestoringChatHistory)
    {
        UpdateActiveChatSummary(Role, RawText, true);
    }

    const FName EntryBorderTag = bUserEntry ? FName(TEXT("UnrealAgent.Transcript.UserBubble")) : FName();
    const FName EntryTextTag = bUserEntry
        ? FName(TEXT("UnrealAgent.Transcript.UserText"))
        : Role == TEXT("OpenCode")
            ? FName(TEXT("UnrealAgent.Transcript.AssistantText"))
            : FName(TEXT("UnrealAgent.Transcript.Text"));

    TranscriptScrollBox->AddSlot()
    .Padding(FMargin(0.0f, 0.0f, 0.0f, 14.0f))
    [
        SAssignNew(EntryWidget, SHorizontalBox)
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .HAlign(EntryAlignment)
        [
            SNew(SBox)
            .MaxDesiredWidth(bUserEntry ? UserTranscriptMaxWidth : TranscriptEntryWidth)
            [
                SNew(SBorder)
                .Tag(EntryBorderTag)
                .BorderImage(bUserEntry ? GetUserTranscriptBrush() : FCoreStyle::Get().GetBrush("NoBrush"))
                .Padding(bUserEntry ? FMargin(12.0f, 8.0f) : FMargin(0.0f))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SBox)
                        .Visibility(bShowRoleLabel ? EVisibility::Visible : EVisibility::Collapsed)
                        [
                            SNew(STextBlock)
                            .Text(RoleLabel)
                            .Font(FAppStyle::Get().GetFontStyle("SmallBoldFont"))
                            .ColorAndOpacity(AccentColor)
                            .Justification(TextJustification)
                        ]
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(bShowRoleLabel ? FMargin(0.0f, 4.0f, 0.0f, 0.0f) : FMargin(0.0f))
                    [
                        SAssignNew(EntryTextBlock, STextBlock)
                        .Tag(EntryTextTag)
                        .Text(FText::FromString(DisplayText))
                        .ColorAndOpacity(FSlateColor::UseForeground())
                        .AutoWrapText(true)
                        .WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)
                        .Justification(TextJustification)
                    ]
                ]
            ]
        ]
    ];

    TranscriptEntryWidgets.Add(EntryWidget);
    LastTranscriptTextBlock = EntryTextBlock;
    LastTranscriptRole = Role;
    LastTranscriptText = RawText;
    TrimTranscriptHistory();
    TranscriptScrollBox->ScrollToEnd();
}

void SUnrealAgentPanel::AppendActivityEntryToActive(const FString& Role, const FString& RawText)
{
    if (!ActiveActivityBodyBox.IsValid())
    {
        return;
    }

    if (Role == TEXT("Tool"))
    {
        AppendToolActivityGroup(RawText);
        LastActivityTranscriptRole = Role;
        return;
    }

    AppendActivityTextRow(Role, RawText);
    LastActivityTranscriptRole = Role;
}

void SUnrealAgentPanel::AppendActivityTextRow(const FString& Role, const FString& RawText)
{
    const bool bAppendToPreviousText = Role == TEXT("Thought")
        && LastActivityTranscriptRole == Role
        && LastActivityTextBlock.IsValid();

    if (bAppendToPreviousText)
    {
        LastTranscriptText = ClampTranscriptText(LastTranscriptText + RawText);
        LastActivityTextBlock->SetText(FText::FromString(LastTranscriptText));
        return;
    }

    const FString EntryText = FormatActivityTranscriptText(Role, RawText);
    if (EntryText.IsEmpty())
    {
        return;
    }

    ActiveActivityBodyBox->AddSlot()
    .AutoHeight()
    .Padding(FMargin(0.0f, 2.0f, 0.0f, 6.0f))
    [
        SAssignNew(LastActivityTextBlock, STextBlock)
        .Tag(FName(TEXT("UnrealAgent.Transcript.Working.Text")))
        .Text(FText::FromString(EntryText))
        .ColorAndOpacity(FSlateColor::UseSubduedForeground())
        .AutoWrapText(true)
        .WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)
    ];

    LastTranscriptText = EntryText;
}

void SUnrealAgentPanel::AppendToolActivityGroup(const FString& RawText)
{
    const FToolActivityDisplay Display = ParseToolActivityDisplay(RawText);
    if (!Display.bShouldShow || !ActiveActivityBodyBox.IsValid())
    {
        return;
    }

    FToolActivityGroup* ExistingGroup = ActiveToolActivityGroups.FindByPredicate([&Display](const FToolActivityGroup& Group)
    {
        return Group.Key == Display.Key;
    });

    if (ExistingGroup == nullptr)
    {
        const int32 GroupIndex = ActiveToolActivityGroups.AddDefaulted();
        ExistingGroup = &ActiveToolActivityGroups[GroupIndex];
        ExistingGroup->Key = Display.Key;
        ExistingGroup->Title = Display.Title;

        TSharedPtr<STextBlock> HeaderTextBlock;
        TSharedPtr<STextBlock> BodyTextBlock;

        ActiveActivityBodyBox->AddSlot()
        .AutoHeight()
        .Padding(FMargin(0.0f, 4.0f, 0.0f, 4.0f))
        [
            SNew(SExpandableArea)
            .Tag(FName(TEXT("UnrealAgent.Transcript.ToolGroup")))
            .InitiallyCollapsed(true)
            .AllowAnimatedTransition(false)
            .BorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
            .BodyBorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
            .BorderBackgroundColor(FStyleColors::Transparent)
            .BodyBorderBackgroundColor(FStyleColors::Transparent)
            .HeaderPadding(FMargin(0.0f))
            .Padding(FMargin(12.0f, 3.0f, 0.0f, 0.0f))
            .HeaderContent()
            [
                SAssignNew(HeaderTextBlock, STextBlock)
                .Tag(FName(TEXT("UnrealAgent.Transcript.ToolGroup.Header")))
                .Text(FText::FromString(Display.Title))
                .Font(FAppStyle::Get().GetFontStyle("SmallBoldFont"))
                .ColorAndOpacity(FSlateColor::UseForeground())
            ]
            .BodyContent()
            [
                SAssignNew(BodyTextBlock, STextBlock)
                .Tag(FName(TEXT("UnrealAgent.Transcript.ToolGroup.Body")))
                .Text(FText::FromString(Display.Detail))
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                .AutoWrapText(true)
                .WrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping)
            ]
        ];

        ExistingGroup->HeaderTextBlock = HeaderTextBlock;
        ExistingGroup->BodyTextBlock = BodyTextBlock;
    }

    if (!ExistingGroup->Details.Contains(Display.Detail))
    {
        ExistingGroup->Details.Add(Display.Detail);
    }

    if (ExistingGroup->HeaderTextBlock.IsValid())
    {
        ExistingGroup->HeaderTextBlock->SetText(FText::FromString(MakeToolGroupHeaderText(ExistingGroup->Title, ExistingGroup->Details.Num())));
    }
    if (ExistingGroup->BodyTextBlock.IsValid())
    {
        ExistingGroup->BodyTextBlock->SetText(FText::FromString(MakeToolGroupBodyText(ExistingGroup->Details)));
    }

    LastActivityTextBlock.Reset();
    LastTranscriptText.Reset();
}

void SUnrealAgentPanel::ResetActiveActivityState()
{
    ActiveActivityBodyBox.Reset();
    LastActivityTextBlock.Reset();
    ActiveToolActivityGroups.Reset();
}

void SUnrealAgentPanel::FlushPendingTranscript(bool bForce)
{
    if (PendingTranscriptText.IsEmpty())
    {
        return;
    }

    const double Now = FPlatformTime::Seconds();
    if (!bForce && Now - LastTranscriptFlushTime < TranscriptFlushIntervalSeconds)
    {
        return;
    }

    AddTranscriptEntryImmediately(PendingTranscriptRole, PendingTranscriptText);
    PendingTranscriptText.Reset();
    LastTranscriptFlushTime = Now;
}

bool SUnrealAgentPanel::ShouldAppendToLastTranscriptEntry(const FString& Role) const
{
    return LastTranscriptTextBlock.IsValid()
        && IsStreamTranscriptRole(Role)
        && Role == LastTranscriptRole;
}

FString SUnrealAgentPanel::ClampTranscriptText(const FString& Text) const
{
    return Text.Len() <= MaxTranscriptEntryChars
        ? Text
        : Text.Left(MaxTranscriptEntryChars) + TEXT("\n[truncated]");
}

void SUnrealAgentPanel::TrimTranscriptHistory()
{
    while (TranscriptEntryWidgets.Num() > MaxTranscriptEntries)
    {
        TSharedPtr<SWidget> OldestEntry = TranscriptEntryWidgets[0];
        TranscriptEntryWidgets.RemoveAt(0);
        if (TranscriptScrollBox.IsValid() && OldestEntry.IsValid())
        {
            TranscriptScrollBox->RemoveSlot(OldestEntry.ToSharedRef());
        }
    }
}

void SUnrealAgentPanel::FinalizeActiveReasoning()
{
    if (ActiveReasoningEndSeconds.IsValid() && *ActiveReasoningEndSeconds == 0.0)
    {
        *ActiveReasoningEndSeconds = FPlatformTime::Seconds();
    }
    ActiveReasoningStartedSeconds.Reset();
    ActiveReasoningEndSeconds.Reset();
    ActiveActivityHasReasoning.Reset();
    ActiveActivityUpdateCount.Reset();
    ResetActiveActivityState();
    LastActivityTranscriptRole.Reset();
    if (LastTranscriptRole == TEXT("Activity"))
    {
        LastTranscriptRole.Reset();
    }
}

void SUnrealAgentPanel::HandlePermissionRequest(const FString& Description)
{
    bHasPendingPermission = true;
    PendingPermissionDescription = Description;
    SetStatus(TEXT("OpenCode requested permission."));
}

void SUnrealAgentPanel::HandleClientStopped()
{
    FlushPendingTranscript(true);
    FinalizeActiveReasoning();
    bHasPendingPermission = false;
    PendingPermissionDescription.Reset();
    PendingTranscriptText.Reset();
    PendingTranscriptRole.Reset();
    LastTranscriptTextBlock.Reset();
    ResetActiveActivityState();
    ActiveReasoningStartedSeconds.Reset();
    ActiveReasoningEndSeconds.Reset();
    ActiveActivityHasReasoning.Reset();
    ActiveActivityUpdateCount.Reset();
    LastTranscriptRole.Reset();
    LastActivityTranscriptRole.Reset();
    LastTranscriptText.Reset();
    RefreshModelOptions();
}

void SUnrealAgentPanel::RefreshModelOptions()
{
    StoreActiveContextWindowUsage();

    ModelOptions.Reset();
    SelectedModelOption.Reset();
    SelectedThinkingOption.Reset();
    SelectedAgentOption.Reset();

    if (AcpClient.IsValid())
    {
        const FString& CurrentModel = AcpClient->GetCurrentModel();
        for (const FOpenCodeAcpModelOption& ModelOption : AcpClient->GetModelOptions())
        {
            TSharedPtr<FOpenCodeAcpModelOption> SharedOption = MakeShared<FOpenCodeAcpModelOption>(ModelOption);
            if (SharedOption->Id == CurrentModel)
            {
                SelectedModelOption = SharedOption;
            }
            ModelOptions.Add(SharedOption);
        }

        const FString& CurrentThinking = AcpClient->GetCurrentThinking();
        for (const FOpenCodeAcpThinkingOption& ThinkingOption : AcpClient->GetThinkingOptions())
        {
            if (ThinkingOption.Id == CurrentThinking)
            {
                SelectedThinkingOption = MakeShared<FOpenCodeAcpThinkingOption>(ThinkingOption);
                break;
            }
        }

        const FString& CurrentAgent = AcpClient->GetCurrentAgent();
        for (const FOpenCodeAcpAgentOption& AgentOption : AcpClient->GetAgentOptions())
        {
            if (AgentOption.Id == CurrentAgent)
            {
                SelectedAgentOption = MakeShared<FOpenCodeAcpAgentOption>(AgentOption);
                break;
            }
        }
    }

    ModelOptions.Sort([](const TSharedPtr<FOpenCodeAcpModelOption>& Left, const TSharedPtr<FOpenCodeAcpModelOption>& Right)
    {
        if (!Left.IsValid() || !Right.IsValid())
        {
            return Left.IsValid();
        }

        const int32 ProviderCompare = Left->GetProviderName().Compare(Right->GetProviderName(), ESearchCase::IgnoreCase);
        if (ProviderCompare != 0)
        {
            return ProviderCompare < 0;
        }

        return Left->GetDisplayName().Compare(Right->GetDisplayName(), ESearchCase::IgnoreCase) < 0;
    });

    RebuildFilteredModelOptions();
}

#undef LOCTEXT_NAMESPACE
