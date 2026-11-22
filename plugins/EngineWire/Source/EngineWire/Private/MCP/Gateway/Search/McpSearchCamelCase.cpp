// McpSearchCamelCase.cpp — see header.

#include "MCP/Gateway/Search/McpSearchCamelCase.h"

#include "MCP/Gateway/McpNativeGatewaySearchMatch.h"

namespace
{
bool IsUpper(TCHAR Ch)
{
    return Ch >= TEXT('A') && Ch <= TEXT('Z');
}

bool IsLower(TCHAR Ch)
{
    return Ch >= TEXT('a') && Ch <= TEXT('z');
}

bool IsRunChar(TCHAR Ch)
{
    return IsUpper(Ch) || IsLower(Ch) || (Ch >= TEXT('0') && Ch <= TEXT('9'));
}

// Whether every character of Run in [From, To) is lower case (bAll), or any one is (!bAll).
bool LowerIn(const FString& Run, int32 From, int32 To, bool bAll)
{
    for (int32 Index = From; Index < To; ++Index)
    {
        if (IsLower(Run[Index]) != bAll)
        {
            return !bAll;
        }
    }
    return bAll;
}
}

TArray<FString> McpSplitCamelCaseWord(const FString& Run)
{
    TArray<FString> Parts;
    int32 Start = 0;
    for (int32 At = 1; At < Run.Len(); ++At)
    {
        if (!IsUpper(Run[At]))
        {
            continue;
        }
        const TCHAR Prev = Run[At - 1];
        const TCHAR Next = At + 1 < Run.Len() ? Run[At + 1] : TEXT('\0');
        const bool bSplit = IsLower(Prev)
            ? !(LowerIn(Run, 0, At, true) && !LowerIn(Run, At, Run.Len(), false))
            : IsUpper(Prev) && IsLower(Next) && At - Start > 1 && !(Next == TEXT('s') && At + 2 == Run.Len());
        if (bSplit)
        {
            Parts.Add(Run.Mid(Start, At - Start));
            Start = At;
        }
    }
    Parts.Add(Run.Mid(Start));
    return Parts;
}

FString McpSplitCamelCase(const FString& Text)
{
    FString Out;
    Out.Reserve(Text.Len() + Text.Len() / 4);
    int32 Index = 0;
    while (Index < Text.Len())
    {
        int32 End = Index;
        while (End < Text.Len() && IsRunChar(Text[End]))
        {
            ++End;
        }
        if (End == Index)
        {
            Out.AppendChar(Text[Index++]);
            continue;
        }
        Out += FString::Join(McpSplitCamelCaseWord(Text.Mid(Index, End - Index)), TEXT(" "));
        Index = End;
    }
    return Out;
}

TArray<FMcpJoinedSpan> McpSearchJoinedSpans(const FString& Query, const TArray<FString>& ContentWords)
{
    // The query's words as typed, and folded: joining folded words would read "directories to scan" as directorytoscan.
    TArray<FString> Runs;
    FString Run;
    for (const TCHAR Ch : Query + TEXT(" "))
    {
        if (IsRunChar(Ch))
        {
            Run.AppendChar(Ch);
        }
        else if (!Run.IsEmpty())
        {
            Runs.Add(Run);
            Run.Reset();
        }
    }
    const auto Fold = [](const FString& Text)
    {
        TArray<FString> Words;
        McpSearchWords(Text, Words);
        return Words.Num() == 1 ? Words[0] : Text;
    };
    TArray<FString> Folded;
    for (const FString& Each : Runs)
    {
        Folded.Add(Fold(Each));
    }
    TArray<FMcpJoinedSpan> Spans;
    for (int32 From = 0; From < Runs.Num(); ++From)
    {
        FString Joined = Runs[From];
        for (int32 To = From + 1; To < FMath::Min(Runs.Num(), From + McpSearchJoinedSpanWords); ++To)
        {
            Joined += Runs[To];
            FMcpJoinedSpan& Span = Spans.AddDefaulted_GetRef();
            Span.Word = Fold(Joined);
            for (int32 Index = From; Index <= To; ++Index)
            {
                if (ContentWords.Contains(Folded[Index]))
                {
                    Span.Covers.AddUnique(Folded[Index]);
                }
            }
        }
    }
    return Spans;
}

bool McpSearchFieldHolds(const FString& Text, const FString& Word, const TArray<FMcpJoinedSpan>& Spans, bool bNamesOnly)
{
    TArray<FString> Words;
    McpSearchWords(Text, Words);
    if (Words.Contains(Word))
    {
        return true;
    }
    if (!Spans.ContainsByPredicate([&Word](const FMcpJoinedSpan& Span) { return Span.Covers.Contains(Word); }))
    {
        return false;
    }
    TArray<FString> Whole;
    if (bNamesOnly)
    {
        // The runs that open with a capital, folded like any word.
        for (int32 Index = 0; Index < Text.Len();)
        {
            int32 End = Index;
            while (End < Text.Len() && IsRunChar(Text[End]))
            {
                ++End;
            }
            if (End > Index && IsUpper(Text[Index]))
            {
                McpSearchWords(Text.Mid(Index, End - Index), Whole);
            }
            Index = FMath::Max(End, Index + 1);
        }
    }
    const TArray<FString>& Holds = bNamesOnly ? Whole : Words;
    for (const FMcpJoinedSpan& Span : Spans)
    {
        if (Span.Covers.Contains(Word) && Holds.Contains(Span.Word))
        {
            return true;
        }
    }
    return false;
}
