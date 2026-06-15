#include "Acp/StudioKit/UnrealAgentStudioKitPrivate.h"

namespace UnrealAgentStudioKit
{
namespace
{
bool IsSensitivePromptFieldKey(const FString& Key)
{
    FString Normalized;
    for (const TCHAR Character : Key.ToLower())
    {
        if (FChar::IsAlnum(Character))
        {
            Normalized.AppendChar(Character);
        }
    }
    return Normalized == TEXT("xmcpcapabilitytoken")
        || Normalized == TEXT("capabilitytoken")
        || Normalized == TEXT("authorization")
        || Normalized == TEXT("apikey")
        || Normalized == TEXT("accesstoken")
        || Normalized == TEXT("refreshtoken")
        || Normalized == TEXT("password")
        || Normalized == TEXT("secret");
}

int32 FindEmbeddedSensitiveAssignmentSeparator(const FString& Line)
{
    const TCHAR* Markers[] = {
        TEXT("x-mcp-capability-token"),
        TEXT("capability_token"),
        TEXT("capability-token"),
        TEXT("capability token"),
        TEXT("capabilitytoken"),
        TEXT("authorization"),
        TEXT("api_key"),
        TEXT("api-key"),
        TEXT("apikey"),
        TEXT("access_token"),
        TEXT("accesstoken"),
        TEXT("refresh_token"),
        TEXT("refreshtoken"),
        TEXT("password"),
        TEXT("secret")
    };
    for (const TCHAR* Marker : Markers)
    {
        int32 SearchFrom = 0;
        while (SearchFrom < Line.Len())
        {
            const int32 MarkerIndex = Line.Find(
                Marker,
                ESearchCase::IgnoreCase,
                ESearchDir::FromStart,
                SearchFrom);
            if (MarkerIndex == INDEX_NONE)
            {
                break;
            }
            const bool bHasLeftBoundary = MarkerIndex == 0
                || !FChar::IsAlnum(Line[MarkerIndex - 1]);
            int32 Cursor = MarkerIndex + FCString::Strlen(Marker);
            while (Cursor < Line.Len()
                && (FChar::IsWhitespace(Line[Cursor])
                    || Line[Cursor] == TEXT('"')
                    || Line[Cursor] == TEXT('\'')))
            {
                ++Cursor;
            }
            if (bHasLeftBoundary
                && Cursor < Line.Len()
                && (Line[Cursor] == TEXT(':') || Line[Cursor] == TEXT('=')))
            {
                return Cursor;
            }
            SearchFrom = MarkerIndex + 1;
        }
    }
    return INDEX_NONE;
}

FString RedactPromptLine(const FString& Line)
{
    int32 ColonIndex = INDEX_NONE;
    int32 EqualsIndex = INDEX_NONE;
    Line.FindChar(TEXT(':'), ColonIndex);
    Line.FindChar(TEXT('='), EqualsIndex);
    int32 SeparatorIndex = ColonIndex;
    if (SeparatorIndex == INDEX_NONE
        || (EqualsIndex != INDEX_NONE && EqualsIndex < SeparatorIndex))
    {
        SeparatorIndex = EqualsIndex;
    }
    if (SeparatorIndex != INDEX_NONE
        && IsSensitivePromptFieldKey(Line.Left(SeparatorIndex)))
    {
        return Line.Left(SeparatorIndex + 1) + TEXT(" [REDACTED]");
    }
    const int32 EmbeddedSeparator =
        FindEmbeddedSensitiveAssignmentSeparator(Line);
    if (EmbeddedSeparator != INDEX_NONE)
    {
        return Line.Left(EmbeddedSeparator + 1) + TEXT(" [REDACTED]");
    }

    const int32 BearerIndex = Line.Find(
        TEXT("bearer "),
        ESearchCase::IgnoreCase);
    return BearerIndex == INDEX_NONE
        ? Line
        : Line.Left(BearerIndex + 7) + TEXT("[REDACTED]");
}
}
}

FString FUnrealAgentStudioKit::RedactPromptSensitiveText(const FString& Text)
{
    TArray<FString> Lines;
    Text.ParseIntoArrayLines(Lines, false);
    if (Lines.IsEmpty() && !Text.IsEmpty())
    {
        Lines.Add(Text);
    }

    FString Redacted;
    for (int32 Index = 0; Index < Lines.Num(); ++Index)
    {
        if (Index > 0)
        {
            Redacted += LINE_TERMINATOR;
        }
        Redacted += UnrealAgentStudioKit::RedactPromptLine(Lines[Index]);
    }
    if (Text.EndsWith(TEXT("\n")) && !Redacted.EndsWith(TEXT("\n")))
    {
        Redacted += LINE_TERMINATOR;
    }
    return Redacted;
}
