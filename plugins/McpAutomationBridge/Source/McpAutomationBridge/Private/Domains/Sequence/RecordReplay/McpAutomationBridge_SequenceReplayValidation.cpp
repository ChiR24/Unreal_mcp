#include "Domains/Sequence/RecordReplay/McpAutomationBridge_SequenceReplayInternal.h"

#include "Dom/JsonObject.h"

namespace McpSequenceRecordReplay
{
namespace
{
constexpr int32 MaxReplayNameLength = 128;
constexpr double MaxCheckpointSaveMsPerFrame = 1000.0;
constexpr double MaxReplayRecordTimeSeconds = 86400.0;
constexpr double MaxReplayPlaybackSpeed = 16.0;
constexpr double MaxReplaySeekTimeSeconds = 86400.0;
constexpr double MaxKillcamDurationSeconds = 600.0;

bool IsAsciiAlphaNumeric(TCHAR Character)
{
    return (Character >= TEXT('A') && Character <= TEXT('Z')) ||
        (Character >= TEXT('a') && Character <= TEXT('z')) ||
        (Character >= TEXT('0') && Character <= TEXT('9'));
}

bool ValidateOptionalNameField(
    const TSharedPtr<FJsonObject>& Payload,
    const TCHAR* FieldName,
    FString& OutError)
{
    if (!Payload.IsValid() || !Payload->HasField(FieldName))
    {
        return true;
    }

    FString Name;
    if (!Payload->TryGetStringField(FieldName, Name) || !IsSafeReplayName(Name))
    {
        OutError = FString::Printf(
            TEXT("%s must be a simple replay identifier using only ASCII letters, numbers, underscores, and hyphens"),
            FieldName);
        return false;
    }
    return true;
}

bool ValidateOptionalNumberField(
    const TSharedPtr<FJsonObject>& Payload,
    const TCHAR* FieldName,
    bool bAllowZero,
    double Maximum,
    FString& OutError)
{
    if (!Payload.IsValid() || !Payload->HasField(FieldName)) return true;
    double Value = 0.0;
    if (!Payload->TryGetNumberField(FieldName, Value) ||
        !FMath::IsFinite(Value) ||
        (bAllowZero ? Value < 0.0 : Value <= 0.0) ||
        Value > Maximum)
    {
        OutError = FString::Printf(
            TEXT("%s must be a %s number no greater than %.3f"),
            FieldName, bAllowZero ? TEXT("non-negative") : TEXT("positive"),
            Maximum);
        return false;
    }
    return true;
}

bool ValidateOptionalBoolField(
    const TSharedPtr<FJsonObject>& Payload,
    const TCHAR* FieldName,
    FString& OutError)
{
    if (!Payload.IsValid() || !Payload->HasField(FieldName)) return true;
    bool Value = false;
    if (Payload->TryGetBoolField(FieldName, Value)) return true;
    OutError = FString::Printf(TEXT("%s must be a boolean"), FieldName);
    return false;
}
}

bool IsSafeReplayName(const FString& Name)
{
    if (Name.IsEmpty() || Name.Len() > MaxReplayNameLength ||
        !IsAsciiAlphaNumeric(Name[0]))
    {
        return false;
    }

    for (int32 Index = 1; Index < Name.Len(); ++Index)
    {
        const TCHAR Character = Name[Index];
        if (!IsAsciiAlphaNumeric(Character) &&
            Character != TEXT('_') && Character != TEXT('-'))
        {
            return false;
        }
    }
    return true;
}

bool ValidateReplayOptions(const TArray<FString>& Options, FString& OutError)
{
    for (const FString& Option : Options)
    {
        if (Option != TEXT("ReplayStreamerOverride=Null"))
        {
            OutError = TEXT(
                "additionalOptions contains an unsupported replay option; "
                "only ReplayStreamerOverride=Null is allowed");
            return false;
        }
    }
    return true;
}

bool ValidateReplayRequest(
    const TSharedPtr<FJsonObject>& Payload,
    FString& OutError)
{
    if (!ValidateOptionalNameField(Payload, TEXT("replayName"), OutError) ||
        !ValidateOptionalNameField(Payload, TEXT("demoName"), OutError) ||
        !ValidateOptionalNameField(Payload, TEXT("name"), OutError) ||
        !ValidateOptionalNumberField(
            Payload, TEXT("checkpointSaveMaxMSPerFrame"), false,
            MaxCheckpointSaveMsPerFrame, OutError) ||
        !ValidateOptionalNumberField(
            Payload, TEXT("maxRecordTimeSeconds"), false,
            MaxReplayRecordTimeSeconds, OutError) ||
        !ValidateOptionalNumberField(
            Payload, TEXT("playbackSpeed"), false,
            MaxReplayPlaybackSpeed, OutError) ||
        !ValidateOptionalNumberField(
            Payload, TEXT("speed"), false, MaxReplayPlaybackSpeed, OutError) ||
        !ValidateOptionalNumberField(
            Payload, TEXT("timeSeconds"), true,
            MaxReplaySeekTimeSeconds, OutError) ||
        !ValidateOptionalNumberField(
            Payload, TEXT("seconds"), true,
            MaxReplaySeekTimeSeconds, OutError) ||
        !ValidateOptionalNumberField(
            Payload, TEXT("durationSeconds"), false,
            MaxKillcamDurationSeconds, OutError) ||
        !ValidateOptionalBoolField(
            Payload, TEXT("prioritizeActors"), OutError) ||
        !ValidateOptionalBoolField(
            Payload, TEXT("loadDefaultMapOnStop"), OutError))
    {
        return false;
    }

    if (!Payload.IsValid() || !Payload->HasField(TEXT("additionalOptions")))
    {
        return true;
    }

    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Payload->TryGetArrayField(TEXT("additionalOptions"), Values) || !Values)
    {
        OutError = TEXT("additionalOptions must be an array of supported replay options");
        return false;
    }

    TArray<FString> Options;
    for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        FString Option;
        if (!Value.IsValid() || !Value->TryGetString(Option) || Option.IsEmpty())
        {
            OutError = TEXT("additionalOptions must contain non-empty strings");
            return false;
        }
        Options.Add(MoveTemp(Option));
    }
    return ValidateReplayOptions(Options, OutError);
}
}
