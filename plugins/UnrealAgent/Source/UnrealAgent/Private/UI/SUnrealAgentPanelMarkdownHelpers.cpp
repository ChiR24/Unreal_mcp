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
}

#undef LOCTEXT_NAMESPACE
