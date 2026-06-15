#include "Acp/StudioKit/UnrealAgentStudioKitPrivate.h"

namespace UnrealAgentStudioKit
{
    const TCHAR* SensitiveMarkers[] = {
        TEXT("x-mcp-capability-token"),
        TEXT("capabilitytoken"),
        TEXT("capability token"),
        TEXT("capability_token"),
        TEXT("capability-token"),
        TEXT("authorization:"),
        TEXT("bearer "),
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

    bool IsSensitiveLine(const FString& Line)
    {
        const FString LowerLine = Line.ToLower();
        for (const TCHAR* Marker : SensitiveMarkers)
        {
            if (LowerLine.Contains(Marker))
            {
                return true;
            }
        }
        return false;
    }

    FString FindPendingSensitiveMarkerPrefix(const FString& Line)
    {
        const FString LowerLine = Line.ToLower();
        FString BestMatch;
        for (const TCHAR* MarkerText : SensitiveMarkers)
        {
            const FString Marker(MarkerText);
            const int32 MaxPrefixLength = FMath::Min(LowerLine.Len(), Marker.Len() - 1);
            for (int32 PrefixLength = MaxPrefixLength; PrefixLength > BestMatch.Len(); --PrefixLength)
            {
                if (LowerLine.EndsWith(Marker.Left(PrefixLength)))
                {
                    BestMatch = LowerLine.Right(PrefixLength);
                    break;
                }
            }
        }
        return BestMatch;
    }

    FString RedactLine(const FString& Line)
    {
        int32 SeparatorIndex = INDEX_NONE;
        if (Line.FindChar(TEXT(':'), SeparatorIndex) || Line.FindChar(TEXT('='), SeparatorIndex))
        {
            return Line.Left(SeparatorIndex + 1) + TEXT(" [REDACTED]");
        }
        return TEXT("[REDACTED]");
    }

    bool LooksLikeTrustedBoundaryField(const FString& Line)
    {
        int32 SeparatorIndex = INDEX_NONE;
        if ((!Line.FindChar(TEXT(':'), SeparatorIndex)
                && !Line.FindChar(TEXT('='), SeparatorIndex))
            || SeparatorIndex == INDEX_NONE)
        {
            return false;
        }
        const FString Key = Line.Left(SeparatorIndex).TrimStartAndEnd().ToLower();
        return Key == TEXT("safe")
            || Key == TEXT("error")
            || Key == TEXT("warning")
            || Key == TEXT("info")
            || Key == TEXT("status")
            || Key == TEXT("message")
            || Key == TEXT("path")
            || Key == TEXT("file")
            || Key == TEXT("line");
    }

    bool IsMultilineScalarMarker(const FString& Value)
    {
        const FString Trimmed = Value.TrimStartAndEnd();
        return Trimmed.StartsWith(TEXT("|")) || Trimmed.StartsWith(TEXT(">"));
    }
}

FString FUnrealAgentStudioKit::RedactSensitiveText(const FString& Text)
{
    FUnrealAgentRedactionState State;
    return RedactSensitiveText(Text, State);
}

FString FUnrealAgentStudioKit::RedactSensitiveText(
    const FString& Text,
    FUnrealAgentRedactionState& State)
{
    TArray<FString> Lines;
    Text.ParseIntoArrayLines(Lines, false);
    if (Lines.IsEmpty() && !Text.IsEmpty())
    {
        Lines.Add(Text);
    }

    FString Redacted;
    const bool bTextEndsWithLineTerminator =
        Text.EndsWith(TEXT("\n")) || Text.EndsWith(TEXT("\r"));
    for (int32 Index = 0; Index < Lines.Num(); ++Index)
    {
        if (Index > 0)
        {
            Redacted += LINE_TERMINATOR;
        }

        const bool bLineComplete =
            Index + 1 < Lines.Num() || bTextEndsWithLineTerminator;
        const FString DetectionLine =
            State.PendingSensitiveMarkerPrefix + Lines[Index];
        State.PendingSensitiveMarkerPrefix.Reset();
        const bool bSensitive =
            UnrealAgentStudioKit::IsSensitiveLine(DetectionLine);
        const bool bTrustedBoundary =
            UnrealAgentStudioKit::LooksLikeTrustedBoundaryField(Lines[Index]);
        const bool bWasIncompleteValue = State.bRedactIncompleteValue;
        const bool bWasMultilineScalar = State.bRedactMultilineScalar;
        const bool bRedactLine =
            bSensitive
            || (bWasIncompleteValue && !bTrustedBoundary)
            || (bWasMultilineScalar && !bTrustedBoundary);
        if (bSensitive)
        {
            Redacted += UnrealAgentStudioKit::RedactLine(Lines[Index]);
        }
        else if (bRedactLine)
        {
            Redacted += TEXT("[REDACTED]");
        }
        else
        {
            Redacted += Lines[Index];
        }

        if (bWasIncompleteValue && bLineComplete)
        {
            State.bRedactIncompleteValue = false;
        }
        if (bWasMultilineScalar && bTrustedBoundary)
        {
            State.bRedactMultilineScalar = false;
        }
        if (bSensitive)
        {
            int32 SeparatorIndex = INDEX_NONE;
            const bool bHasSeparator =
                DetectionLine.FindChar(TEXT(':'), SeparatorIndex)
                || DetectionLine.FindChar(TEXT('='), SeparatorIndex);
            const FString Value = bHasSeparator
                ? DetectionLine.Mid(SeparatorIndex + 1)
                : FString();
            State.bRedactMultilineScalar =
                bHasSeparator
                && (Value.TrimStartAndEnd().IsEmpty()
                    || UnrealAgentStudioKit::IsMultilineScalarMarker(Value));
            State.bRedactIncompleteValue =
                !State.bRedactMultilineScalar && !bLineComplete;
        }
        else if (!bRedactLine && !bLineComplete)
        {
            State.PendingSensitiveMarkerPrefix =
                UnrealAgentStudioKit::FindPendingSensitiveMarkerPrefix(
                    DetectionLine);
        }
        else if (bRedactLine && !bLineComplete)
        {
            if (bWasIncompleteValue)
            {
                State.bRedactIncompleteValue = true;
            }
        }
    }
    if (Text.EndsWith(TEXT("\n")) && !Redacted.EndsWith(TEXT("\n")))
    {
        Redacted += LINE_TERMINATOR;
    }
    return Redacted;
}

bool FUnrealAgentStudioKit::IsManagedFileText(const FString& Text)
{
    return Text.Contains(UnrealAgentStudioKit::StudioKitVersionMarker);
}
