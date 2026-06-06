#include "UnrealAgentStudioKitPrivate.h"

namespace UnrealAgentStudioKit
{
    bool IsSensitiveLine(const FString& Line)
    {
        const FString LowerLine = Line.ToLower();
        return LowerLine.Contains(TEXT("x-mcp-capability-token"))
            || LowerLine.Contains(TEXT("capabilitytoken"))
            || LowerLine.Contains(TEXT("capability token"))
            || LowerLine.Contains(TEXT("authorization:"))
            || LowerLine.Contains(TEXT("bearer "))
            || LowerLine.Contains(TEXT("api_key"))
            || LowerLine.Contains(TEXT("api-key"))
            || LowerLine.Contains(TEXT("apikey"))
            || LowerLine.Contains(TEXT("access_token"))
            || LowerLine.Contains(TEXT("refresh_token"))
            || LowerLine.Contains(TEXT("password"))
            || LowerLine.Contains(TEXT("secret"));
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
}

FString FUnrealAgentStudioKit::RedactSensitiveText(const FString& Text)
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

        Redacted += UnrealAgentStudioKit::IsSensitiveLine(Lines[Index]) ? UnrealAgentStudioKit::RedactLine(Lines[Index]) : Lines[Index];
    }
    return Redacted;
}

bool FUnrealAgentStudioKit::IsManagedFileText(const FString& Text)
{
    return Text.Contains(UnrealAgentStudioKit::StudioKitVersionMarker);
}
