#pragma once

#include "CoreMinimal.h"

namespace McpAutomationBridgeSubsystemResponse
{
inline FString SanitizeForLog(const FString& In)
{
    if (In.IsEmpty())
    {
        return FString();
    }

    FString Out;
    Out.Reserve(FMath::Min<int32>(In.Len(), 1024));
    for (int32 Index = 0; Index < In.Len(); ++Index)
    {
        const TCHAR Character = In[Index];
        Out.AppendChar(Character >= 32 && Character != 127 ? Character : TEXT('?'));
    }

    if (Out.Len() > 512)
    {
        Out = Out.Left(512) + TEXT("[TRUNCATED]");
    }
    return Out;
}

inline bool IsAllowedUnrealMountPrefixAt(
    const FString& Value,
    int32 Index,
    const TCHAR* Mount)
{
    const int32 MountLen = FCString::Strlen(Mount);
    if (Index + MountLen > Value.Len())
    {
        return false;
    }

    if (!Value.Mid(Index, MountLen).Equals(Mount, ESearchCase::CaseSensitive))
    {
        return false;
    }

    const int32 AfterMount = Index + MountLen;
    if (AfterMount == Value.Len() || Value[AfterMount] == '/')
    {
        return true;
    }

    // A mount root NAMED in prose is still the public mount name, not a host
    // path. Requiring a trailing '/' meant the reflection-boundary refusal
    // ("Objects in /Script packages are not addressable...") had its own
    // subject rewritten to "[path redacted]", deleting the only actionable
    // fact in the message. A following character that cannot continue a path
    // segment ends the token, so `/Script packages` is kept while a genuine
    // host path like `/Scripts/secret` still fails the prefix test and is
    // redacted as before.
    const TCHAR Next = Value[AfterMount];
    return !(FChar::IsAlnum(Next) || Next == TEXT('_') || Next == TEXT('-')
             || Next == TEXT('.') || Next == TEXT('\\'));
}

inline bool IsAllowedUnrealMountPath(const FString& Value, int32 Index)
{
    return IsAllowedUnrealMountPrefixAt(Value, Index, TEXT("/Game")) ||
           IsAllowedUnrealMountPrefixAt(Value, Index, TEXT("/Engine")) ||
           IsAllowedUnrealMountPrefixAt(Value, Index, TEXT("/Script")) ||
           IsAllowedUnrealMountPrefixAt(Value, Index, TEXT("/Temp")) ||
           IsAllowedUnrealMountPrefixAt(Value, Index, TEXT("/Niagara"));
}

// Characters that may appear INSIDE a bare path segment. Space, '(' and ')'
// are not included here: they are handled by IsPathContinuationChar below,
// which only lets them extend a run that is already unambiguously a path.
inline bool IsResponsePathChar(TCHAR Character)
{
    return FChar::IsAlnum(Character) || Character == '/' || Character == '\\' ||
           Character == '.' || Character == '_' || Character == '-' ||
           Character == ':' || Character == '+' || Character == '~';
}

// A path may only START after a separator when the very next character is a
// plausible path character. "/ 'path'" and "a / b" are prose separators, not
// path roots.
inline bool IsPathStartChar(TCHAR Character)
{
    return FChar::IsAlnum(Character) || Character == '.' || Character == '_' ||
           Character == '~' || Character == '/' || Character == '\\';
}

// Space and parentheses continue an unambiguous path ("C:\Program Files
// (x86)\...", "/home/My Project/x"). They are only consulted once the run is
// known to be a path, so a lone prose '/' ("read/write or create/delete",
// "('a' / 'b')") still cannot swallow the words around it.
inline bool IsPathContinuationChar(TCHAR Character)
{
    return IsResponsePathChar(Character) || Character == ' ' ||
           Character == '(' || Character == ')';
}

inline bool IsUnrealMountPathChar(TCHAR Character)
{
    return FChar::IsAlnum(Character) || Character == '/' || Character == '.' ||
           Character == '_' || Character == '-' || Character == ':';
}

inline FString RedactFilesystemPathsForResponse(const FString& Input)
{
    FString Output;
    Output.Reserve(Input.Len());

    for (int32 Index = 0; Index < Input.Len();)
    {
        const bool bAllowedUnrealPath =
            Input[Index] == '/' && IsAllowedUnrealMountPath(Input, Index);
        // A lone '/' in prose is a separator, not a path root: require a
        // plausible path character next, then either a second separator or a
        // known host root as the first segment. Spaces and parentheses only
        // extend the run once a second separator has been seen, so a real
        // spaced path is redacted whole while "read/write or create/delete"
        // stays prose.
        bool bUnixPath = false;
        int32 UnixEnd = Index + 1;
        if (Input[Index] == '/' && !bAllowedUnrealPath &&
            Index + 1 < Input.Len() && IsPathStartChar(Input[Index + 1]))
        {
            int32 Scan = Index + 1;
            int32 Separators = 1;
            bool bAllowSpaces = false;
            while (Scan < Input.Len())
            {
                const TCHAR ScanChar = Input[Scan];
                if (ScanChar == ' ' || ScanChar == '(' || ScanChar == ')')
                {
                    if (!bAllowSpaces) break;
                }
                else if (!IsResponsePathChar(ScanChar))
                {
                    break;
                }
                if (ScanChar == '/')
                {
                    ++Separators;
                    bAllowSpaces = Separators >= 2;
                }
                ++Scan;
            }
            // Single-segment token: a path only when standalone, so `/etc` is
            // redacted and the embedded `and/or` stays prose.
            const bool bStandaloneToken =
                Index == 0 || !FChar::IsAlnum(Input[Index - 1]);
            bUnixPath = Separators >= 2 || bStandaloneToken;
            UnixEnd = Scan;
        }
        const bool bWindowsPath =
            Index + 2 < Input.Len() && FChar::IsAlpha(Input[Index]) &&
            Input[Index + 1] == ':' &&
            (Input[Index + 2] == '/' || Input[Index + 2] == '\\');
        const bool bUncPath =
            Index + 1 < Input.Len() && Input[Index] == '\\' &&
            Input[Index + 1] == '\\';

        if (bAllowedUnrealPath)
        {
            while (Index < Input.Len() && IsUnrealMountPathChar(Input[Index]))
            {
                Output.AppendChar(Input[Index]);
                ++Index;
            }
            continue;
        }

        if (bUnixPath || bWindowsPath || bUncPath)
        {
            Output += TEXT("[path redacted]");
            if (bUnixPath)
            {
                Index = UnixEnd;
            }
            else
            {
                // A drive or UNC root is unambiguous from its first character,
                // so its run consumes spaces/parentheses as well ("Program
                // Files (x86)").
                while (Index < Input.Len() && IsPathContinuationChar(Input[Index]))
                {
                    ++Index;
                }
            }
            continue;
        }

        Output.AppendChar(Input[Index]);
        ++Index;
    }

    return Output;
}

inline void RedactFollowingValueForResponse(FString& Text, const FString& Marker)
{
    const auto IsValueDelimiter = [](TCHAR Character)
    {
        return Character == ';' || Character == '&' || Character == ',' ||
               Character == '\r' || Character == '\n';
    };
    const auto IsHeaderDelimiter = [](TCHAR Character)
    {
        return Character == ';' || Character == '&' || Character == '\r' ||
               Character == '\n';
    };

    int32 SearchStart = 0;
    while (SearchStart < Text.Len())
    {
        const int32 MarkerIndex = Text.Find(
            Marker, ESearchCase::IgnoreCase, ESearchDir::FromStart, SearchStart);
        if (MarkerIndex == INDEX_NONE)
        {
            return;
        }

        const int32 ValueStart = MarkerIndex + Marker.Len();
        const bool bAllowWhitespaceInValue = Marker.EndsWith(TEXT(":"));
        int32 ValueEnd = ValueStart;
        if (bAllowWhitespaceInValue)
        {
            while (ValueEnd < Text.Len() && FChar::IsWhitespace(Text[ValueEnd]) &&
                   Text[ValueEnd] != '\r' && Text[ValueEnd] != '\n')
            {
                ++ValueEnd;
            }
            while (ValueEnd < Text.Len() && !IsHeaderDelimiter(Text[ValueEnd]))
            {
                ++ValueEnd;
            }
        }
        else
        {
            while (ValueEnd < Text.Len() && !IsValueDelimiter(Text[ValueEnd]) &&
                   !FChar::IsWhitespace(Text[ValueEnd]))
            {
                ++ValueEnd;
            }
        }

        Text = Text.Left(ValueStart) + TEXT("[redacted]") + Text.Mid(ValueEnd);
        SearchStart = ValueStart + 10;
    }
}

inline FString SanitizeEngineErrorForResponse(const FString& In)
{
    FString Out = RedactFilesystemPathsForResponse(SanitizeForLog(In));
    RedactFollowingValueForResponse(Out, TEXT("token="));
    RedactFollowingValueForResponse(Out, TEXT("capabilitytoken="));
    RedactFollowingValueForResponse(Out, TEXT("password="));
    RedactFollowingValueForResponse(Out, TEXT("secret="));
    RedactFollowingValueForResponse(Out, TEXT("api_key="));
    RedactFollowingValueForResponse(Out, TEXT("apikey="));
    RedactFollowingValueForResponse(Out, TEXT("authorization:"));
    RedactFollowingValueForResponse(Out, TEXT("bearer "));

    if (Out.Len() > 512)
    {
        Out = Out.Left(512) + TEXT("[TRUNCATED]");
    }
    return Out;
}
}
