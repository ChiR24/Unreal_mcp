#include "Domains/Sequence/RecordReplay/McpAutomationBridge_SequenceTakeRecorderInternal.h"

#include "McpAutomationBridgeSettings.h"

namespace McpSequenceRecordReplay
{
#if MCP_SEQUENCE_HAS_TAKE_RECORDER_API
bool ReadTakeRecorderStringArray(
    const TSharedPtr<FJsonObject>& Payload,
    const TCHAR* FieldName,
    TArray<FString>& OutValues,
    FString& OutError)
{
    if (!Payload.IsValid() || !Payload->HasField(FieldName)) return true;
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Payload->TryGetArrayField(FieldName, Values) || !Values)
    {
        OutError = FString::Printf(
            TEXT("%s must be an array of strings"), FieldName);
        return false;
    }
    const UMcpAutomationBridgeSettings* Settings =
        GetDefault<UMcpAutomationBridgeSettings>();
    const int32 MaxItems =
        Settings ? FMath::Max(1, Settings->MaxTakeRecorderSourceItems) : 64;
    const int32 MaxStringLength =
        Settings ? FMath::Max(1, Settings->MaxTakeRecorderStringLength) : 1024;
    if (Values->Num() > MaxItems)
    {
        OutError = FString::Printf(
            TEXT("%s exceeds the configured item limit"), FieldName);
        return false;
    }
    for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        FString Text;
        if (!Value.IsValid() || !Value->TryGetString(Text) ||
            Text.TrimStartAndEnd().IsEmpty() ||
            Text.TrimStartAndEnd().Len() > MaxStringLength)
        {
            OutError = FString::Printf(
                TEXT("%s must contain non-empty strings"), FieldName);
            return false;
        }
        OutValues.AddUnique(Text.TrimStartAndEnd());
    }
    return true;
}

bool PrepareTakeRecorderSources(
    UTakeRecorderSources* Sources,
    const TSharedPtr<FJsonObject>& Payload,
    FPreparedTakeRecorderSources& OutPrepared,
    FString& OutCode,
    FString& OutError)
{
    TArray<FString> ActorNames;
    TArray<FString> ClassPaths;
    if (!ReadTakeRecorderStringArray(
            Payload, TEXT("actorNames"), ActorNames, OutError) ||
        !ReadTakeRecorderStringArray(
            Payload, TEXT("actors"), ActorNames, OutError) ||
        !ReadTakeRecorderStringArray(
            Payload, TEXT("sourceActors"), ActorNames, OutError) ||
        !ReadTakeRecorderStringArray(
            Payload, TEXT("sourceClasses"), ClassPaths, OutError))
    {
        OutCode = TEXT("INVALID_ARGUMENT");
        return false;
    }
    if (Payload->HasField(TEXT("actorName")))
    {
        FString ActorName;
        const UMcpAutomationBridgeSettings* Settings =
            GetDefault<UMcpAutomationBridgeSettings>();
        const int32 MaxStringLength =
            Settings
                ? FMath::Max(1, Settings->MaxTakeRecorderStringLength)
                : 1024;
        if (!Payload->TryGetStringField(TEXT("actorName"), ActorName) ||
            ActorName.TrimStartAndEnd().IsEmpty() ||
            ActorName.TrimStartAndEnd().Len() > MaxStringLength)
        {
            OutCode = TEXT("INVALID_ARGUMENT");
            OutError = TEXT("actorName must be a non-empty string");
            return false;
        }
        ActorNames.AddUnique(ActorName.TrimStartAndEnd());
    }
    if (Payload->HasField(TEXT("clearSources")) &&
        !Payload->TryGetBoolField(
            TEXT("clearSources"), OutPrepared.bClearSources))
    {
        OutCode = TEXT("INVALID_ARGUMENT");
        OutError = TEXT("clearSources must be a boolean");
        return false;
    }
    for (const FString& ActorName : ActorNames)
    {
        AActor* Actor = FindTakeRecorderActor(ActorName);
        if (!Actor)
        {
            OutCode = TEXT("ACTOR_NOT_FOUND");
            OutError = FString::Printf(
                TEXT("Take Recorder actor not found: %s"), *ActorName);
            return false;
        }
        OutPrepared.Actors.AddUnique(Actor);
    }
    for (const FString& ClassPath : ClassPaths)
    {
        const UMcpAutomationBridgeSettings* Settings =
            GetDefault<UMcpAutomationBridgeSettings>();
        const bool bAllowlisted =
            Settings &&
            Settings->TakeRecorderSourceClassAllowlist.ContainsByPredicate(
                [&ClassPath](const FString& AllowedClass)
                {
                    return ClassPath.Equals(
                        AllowedClass.TrimStartAndEnd(),
                        ESearchCase::CaseSensitive);
                });
        if (!bAllowlisted)
        {
            OutCode = TEXT("INVALID_SOURCE_CLASS");
            OutError = FString::Printf(
                TEXT("Take Recorder source class is not allowlisted: %s"),
                *ClassPath);
            return false;
        }
        UClass* SourceClass =
            LoadClass<UTakeRecorderSource>(nullptr, *ClassPath);
        const UTakeRecorderSource* DefaultSource =
            SourceClass
                ? SourceClass->GetDefaultObject<UTakeRecorderSource>()
                : nullptr;
        if (!SourceClass || !DefaultSource ||
            !DefaultSource->CanAddSource(Sources))
        {
            OutCode = TEXT("INVALID_SOURCE_CLASS");
            OutError = FString::Printf(
                TEXT("Take Recorder source class is invalid or cannot be added: %s"),
                *ClassPath);
            return false;
        }
        OutPrepared.Classes.AddUnique(SourceClass);
    }
    if (OutPrepared.Actors.Num() == 0) return true;
    OutPrepared.ActorSourceClass =
        ResolveTakeRecorderActorSourceClass(OutError);
    if (!OutPrepared.ActorSourceClass)
    {
        OutCode = TEXT("TAKE_ACTOR_SOURCE_UNAVAILABLE");
        return false;
    }
    if (!ValidateTakeActorSourceOptions(
            OutPrepared.ActorSourceClass, Payload, OutError))
    {
        OutCode = TEXT("INVALID_ARGUMENT");
        return false;
    }
    return true;
}
#endif
}
