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
    return AfterMount == Value.Len() || Value[AfterMount] == '/';
}

inline bool IsAllowedUnrealMountPath(const FString& Value, int32 Index)
{
    return IsAllowedUnrealMountPrefixAt(Value, Index, TEXT("/Game")) ||
           IsAllowedUnrealMountPrefixAt(Value, Index, TEXT("/Engine")) ||
           IsAllowedUnrealMountPrefixAt(Value, Index, TEXT("/Script")) ||
           IsAllowedUnrealMountPrefixAt(Value, Index, TEXT("/Temp")) ||
           IsAllowedUnrealMountPrefixAt(Value, Index, TEXT("/Niagara"));
}

inline bool IsResponsePathChar(TCHAR Character)
{
    return FChar::IsAlnum(Character) || Character == '/' || Character == '\\' ||
           Character == '.' || Character == '_' || Character == '-' ||
           Character == ':' || Character == '(' || Character == ')' ||
           Character == '+' || Character == '~' || Character == ' ';
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
        const bool bUnixPath = Input[Index] == '/' && !bAllowedUnrealPath;
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
            while (Index < Input.Len() && IsResponsePathChar(Input[Index]))
            {
                ++Index;
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
