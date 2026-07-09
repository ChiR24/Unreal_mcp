#include "Domains/Sequence/RecordReplay/McpAutomationBridge_SequenceTakeRecorderInternal.h"

#include "Engine/Engine.h"

namespace McpSequenceRecordReplay
{
#if MCP_SEQUENCE_HAS_TAKE_RECORDER_API
TArray<FString> GetTakeRecorderStringArray(
    const TSharedPtr<FJsonObject>& Payload,
    const TCHAR* FieldName)
{
    TArray<FString> Result;
    FString Ignored;
    ReadTakeRecorderStringArray(Payload, FieldName, Result, Ignored);
    return Result;
}

bool ConfigureSources(
    UTakeRecorderPanel* Panel,
    const TSharedPtr<FJsonObject>& Payload,
    int32& OutAdded,
    int32& OutRequested,
    FString& OutErrorCode,
    FString& OutError)
{
    OutAdded = 0;
    OutRequested = 0;
    UTakeRecorderSources* Sources = Panel ? Panel->GetSources() : nullptr;
    if (!Sources)
    {
        OutErrorCode = TEXT("NOT_AVAILABLE");
        OutError = TEXT("Take Recorder sources are unavailable");
        return false;
    }
    if (UTakeRecorderBlueprintLibrary::GetActiveRecorder())
    {
        OutErrorCode = TEXT("RECORDING_ACTIVE");
        OutError =
            TEXT("Take Recorder sources cannot change during an active recording");
        return false;
    }
    FPreparedTakeRecorderSources Prepared;
    if (!PrepareTakeRecorderSources(
            Sources, Payload, Prepared, OutErrorCode, OutError))
        return false;
    OutRequested = Prepared.Actors.Num() + Prepared.Classes.Num();
    if (OutRequested == 0 && !Prepared.bClearSources)
    {
        OutErrorCode = TEXT("INVALID_ARGUMENT");
        OutError =
            TEXT("configure_take_sources requires actors, sourceClasses, or clearSources");
        return false;
    }

    const TArray<UTakeRecorderSource*> OriginalSources =
        Sources->GetSourcesCopy();
    TSet<UTakeRecorderSource*> OriginalSourceSet;
    for (UTakeRecorderSource* Source : OriginalSources)
        OriginalSourceSet.Add(Source);

    Sources->Modify();
    if (Prepared.bClearSources)
        for (UTakeRecorderSource* Source : OriginalSources)
            Sources->RemoveSource(Source);

    TArray<FTakeRecorderActorSourceState> ModifiedActorSources;
    auto RollBack = [&]()
    {
        RestoreActorSourceOptions(ModifiedActorSources);
        for (UTakeRecorderSource* Source : Sources->GetSourcesCopy())
            if (!OriginalSourceSet.Contains(Source))
                Sources->RemoveSource(Source);
        if (Prepared.bClearSources)
        {
            for (UTakeRecorderSource* Original : OriginalSources)
            {
                UTakeRecorderSource* Restored =
                    Original
                        ? Sources->AddSource(Original->GetClass())
                        : nullptr;
                if (!Restored)
                {
                    OutError += TEXT(" (source rollback failed)");
                    break;
                }
                UEngine::CopyPropertiesForUnrelatedObjects(
                    Original, Restored);
            }
        }
        OutAdded = 0;
        return false;
    };
    for (AActor* Actor : Prepared.Actors)
    {
        UTakeRecorderSource* Source = AddTakeActorSource(
            Prepared.ActorSourceClass, Actor, Sources);
        if (!Source)
        {
            OutErrorCode = TEXT("SOURCE_ADD_FAILED");
            OutError = TEXT("Failed to add an actor Take Recorder source");
            return RollBack();
        }
        if (OriginalSourceSet.Contains(Source))
            ModifiedActorSources.Add(
                CaptureActorSourceOptions(Source, Payload));
        Source->Modify();
        Source->bEnabled = true;
        int32 AppliedOptions = 0;
        if (!ConfigureActorSource(Source, Payload, AppliedOptions, OutError))
        {
            OutErrorCode = TEXT("SOURCE_CONFIGURATION_FAILED");
            return RollBack();
        }
        ++OutAdded;
    }
    for (UClass* SourceClass : Prepared.Classes)
    {
        if (!Sources->AddSource(SourceClass))
        {
            OutErrorCode = TEXT("SOURCE_ADD_FAILED");
            OutError = FString::Printf(
                TEXT("Failed to add Take Recorder source: %s"),
                *SourceClass->GetPathName());
            return RollBack();
        }
        ++OutAdded;
    }
    return true;
}

#endif
}
