#include "Acp/Client/McpOpenCodeAcpClientPermissionBinaryGlob.h"

namespace UnrealAgent::OpenCodeAcp::PermissionBinaryGlob
{
namespace
{
bool CharacterClassMatches(
    const FString& Pattern,
    const int32 OpenIndex,
    const TCHAR Literal,
    int32& OutNextIndex)
{
    const int32 CloseIndex = Pattern.Find(
        TEXT("]"),
        ESearchCase::CaseSensitive,
        ESearchDir::FromStart,
        OpenIndex + 1);
    if (CloseIndex == INDEX_NONE)
    {
        OutNextIndex = OpenIndex + 1;
        return Pattern[OpenIndex] == Literal;
    }
    OutNextIndex = CloseIndex + 1;
    int32 Cursor = OpenIndex + 1;
    bool bNegated = false;
    if (Cursor < CloseIndex
        && (Pattern[Cursor] == TEXT('!') || Pattern[Cursor] == TEXT('^')))
    {
        bNegated = true;
        ++Cursor;
    }
    bool bMatched = false;
    while (Cursor < CloseIndex)
    {
        if (Cursor + 2 < CloseIndex && Pattern[Cursor + 1] == TEXT('-'))
        {
            bMatched |= Literal >= Pattern[Cursor]
                && Literal <= Pattern[Cursor + 2];
            Cursor += 3;
        }
        else
        {
            bMatched |= Pattern[Cursor] == Literal;
            ++Cursor;
        }
    }
    return bNegated ? !bMatched : bMatched;
}

bool WildcardPatternMatchesLiteral(
    const FString& Pattern,
    const FString& Literal)
{
    if (Pattern.Len() > 512)
    {
        return true;
    }

    struct FMatchState
    {
        int32 PatternIndex;
        int32 LiteralIndex;
    };
    TArray<FMatchState> Pending;
    TSet<uint64> Visited;
    Pending.Add({0, 0});
    while (!Pending.IsEmpty())
    {
        const FMatchState State = Pending.Pop(EAllowShrinking::No);
        const uint64 Key =
            (static_cast<uint64>(State.PatternIndex) << 32)
            | static_cast<uint32>(State.LiteralIndex);
        if (Visited.Contains(Key))
        {
            continue;
        }
        Visited.Add(Key);
        if (State.PatternIndex == Pattern.Len())
        {
            if (State.LiteralIndex == Literal.Len())
            {
                return true;
            }
            continue;
        }

        const TCHAR PatternCharacter = Pattern[State.PatternIndex];
        if (PatternCharacter == TEXT('*'))
        {
            Pending.Add({State.PatternIndex + 1, State.LiteralIndex});
            if (State.LiteralIndex < Literal.Len())
            {
                Pending.Add({State.PatternIndex, State.LiteralIndex + 1});
            }
            continue;
        }
        if (State.LiteralIndex >= Literal.Len())
        {
            continue;
        }
        if (PatternCharacter == TEXT('?'))
        {
            Pending.Add({State.PatternIndex + 1, State.LiteralIndex + 1});
            continue;
        }
        if (PatternCharacter == TEXT('['))
        {
            int32 NextPatternIndex = State.PatternIndex + 1;
            if (CharacterClassMatches(
                    Pattern,
                    State.PatternIndex,
                    Literal[State.LiteralIndex],
                    NextPatternIndex))
            {
                Pending.Add({NextPatternIndex, State.LiteralIndex + 1});
            }
            continue;
        }
        if (PatternCharacter == Literal[State.LiteralIndex])
        {
            Pending.Add({State.PatternIndex + 1, State.LiteralIndex + 1});
        }
    }
    return false;
}
}

bool ExpandBracePatterns(
    const FString& Pattern,
    TArray<FString>& OutPatterns)
{
    OutPatterns = {Pattern};
    for (int32 Pass = 0; Pass < 16; ++Pass)
    {
        bool bExpandedAny = false;
        TArray<FString> NextPatterns;
        for (const FString& Candidate : OutPatterns)
        {
            int32 OpenIndex = Candidate.Find(TEXT("{"));
            int32 CloseIndex = INDEX_NONE;
            FString Body;
            while (OpenIndex != INDEX_NONE)
            {
                CloseIndex = Candidate.Find(
                    TEXT("}"),
                    ESearchCase::CaseSensitive,
                    ESearchDir::FromStart,
                    OpenIndex + 1);
                if (CloseIndex == INDEX_NONE)
                {
                    OpenIndex = INDEX_NONE;
                    break;
                }
                Body = Candidate.Mid(
                    OpenIndex + 1,
                    CloseIndex - OpenIndex - 1);
                if (Body.Contains(TEXT(",")))
                {
                    break;
                }
                if (Body.Contains(TEXT("..")))
                {
                    return false;
                }
                OpenIndex = Candidate.Find(
                    TEXT("{"),
                    ESearchCase::CaseSensitive,
                    ESearchDir::FromStart,
                    CloseIndex + 1);
            }
            if (OpenIndex == INDEX_NONE)
            {
                NextPatterns.Add(Candidate);
                continue;
            }
            if (Body.StartsWith(TEXT(","))
                || Body.EndsWith(TEXT(","))
                || Body.Contains(TEXT(",,")))
            {
                return false;
            }
            TArray<FString> Alternatives;
            Body.ParseIntoArray(Alternatives, TEXT(","), false);
            if (NextPatterns.Num() + Alternatives.Num() > 64)
            {
                return false;
            }
            for (const FString& Alternative : Alternatives)
            {
                const FString Expanded =
                    Candidate.Left(OpenIndex)
                    + Alternative
                    + Candidate.Mid(CloseIndex + 1);
                if (Expanded.Len() > 512)
                {
                    return false;
                }
                NextPatterns.Add(Expanded);
            }
            bExpandedAny = true;
        }
        OutPatterns = MoveTemp(NextPatterns);
        if (!bExpandedAny)
        {
            return true;
        }
    }
    return false;
}

bool ShellPatternMayMatchLiteral(
    const FString& Pattern,
    const FString& Literal)
{
    TArray<FString> ExpandedPatterns;
    if (!ExpandBracePatterns(Pattern, ExpandedPatterns))
    {
        return true;
    }
    for (const FString& ExpandedPattern : ExpandedPatterns)
    {
        if (WildcardPatternMatchesLiteral(ExpandedPattern, Literal))
        {
            return true;
        }
    }
    return false;
}
}
