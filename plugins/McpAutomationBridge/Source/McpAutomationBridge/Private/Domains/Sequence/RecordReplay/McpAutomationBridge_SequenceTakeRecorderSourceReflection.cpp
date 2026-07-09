#include "Domains/Sequence/RecordReplay/McpAutomationBridge_SequenceTakeRecorderInternal.h"

#if MCP_SEQUENCE_HAS_TAKE_RECORDER_API
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#endif

namespace McpSequenceRecordReplay
{
#if MCP_SEQUENCE_HAS_TAKE_RECORDER_API
UClass* ResolveTakeRecorderActorSourceClass(FString& OutError)
{
    UClass* SourceClass = LoadClass<UTakeRecorderSource>(
        nullptr, TEXT("/Script/TakeRecorderSources.TakeRecorderActorSource"));
    if (!SourceClass ||
        !SourceClass->FindFunctionByName(TEXT("AddSourceForActor")))
    {
        OutError = TEXT(
            "Take Recorder actor source reflection API is unavailable");
        return nullptr;
    }
    return SourceClass;
}

bool ValidateTakeActorSourceOptions(
    UClass* ActorSourceClass,
    const TSharedPtr<FJsonObject>& Payload,
    FString& OutError)
{
    for (const TPair<const TCHAR*, const TCHAR*> Field : {
             TPair<const TCHAR*, const TCHAR*>(TEXT("reduceKeys"), TEXT("bReduceKeys")),
             TPair<const TCHAR*, const TCHAR*>(TEXT("recordParentHierarchy"), TEXT("bRecordParentHierarchy"))})
    {
        if (!Payload->HasField(Field.Key)) continue;
        bool Value = false;
        if (!Payload->TryGetBoolField(Field.Key, Value) ||
            !FindFProperty<FBoolProperty>(ActorSourceClass, Field.Value))
        {
            OutError = FString::Printf(
                TEXT("%s is unsupported or invalid"), Field.Key);
            return false;
        }
    }
    if (!Payload->HasField(TEXT("recordType"))) return true;
    FString RecordType;
    const FEnumProperty* Property =
        FindFProperty<FEnumProperty>(ActorSourceClass, TEXT("RecordType"));
    if (!Payload->TryGetStringField(TEXT("recordType"), RecordType) ||
        !Property ||
        (RecordType.ToLower() != TEXT("possessable") &&
         RecordType.ToLower() != TEXT("spawnable") &&
         RecordType.ToLower() != TEXT("projectdefault")))
    {
        OutError =
            TEXT("recordType must be possessable, spawnable, or projectDefault");
        return false;
    }
    return true;
}

UTakeRecorderSource* AddTakeActorSource(
    UClass* SourceClass,
    AActor* Actor,
    UTakeRecorderSources* Sources)
{
    UFunction* Function =
        SourceClass ? SourceClass->FindFunctionByName(TEXT("AddSourceForActor")) : nullptr;
    UObject* DefaultObject = SourceClass ? SourceClass->GetDefaultObject() : nullptr;
    if (!Function || !DefaultObject) return nullptr;
    FStructOnScope Params(Function);
    FObjectPropertyBase* ReturnProperty = nullptr;
    for (TFieldIterator<FProperty> It(Function); It; ++It)
    {
        FObjectPropertyBase* Property = CastField<FObjectPropertyBase>(*It);
        if (!Property || !Property->HasAnyPropertyFlags(CPF_Parm)) continue;
        if (Property->HasAnyPropertyFlags(CPF_ReturnParm))
            ReturnProperty = Property;
        else if (Property->PropertyClass->IsChildOf(AActor::StaticClass()))
            Property->SetObjectPropertyValue_InContainer(
                Params.GetStructMemory(), Actor);
        else if (Property->PropertyClass->IsChildOf(
                     UTakeRecorderSources::StaticClass()))
            Property->SetObjectPropertyValue_InContainer(
                Params.GetStructMemory(), Sources);
    }
    DefaultObject->ProcessEvent(Function, Params.GetStructMemory());
    return ReturnProperty
        ? Cast<UTakeRecorderSource>(
              ReturnProperty->GetObjectPropertyValue_InContainer(
                  Params.GetStructMemory()))
        : nullptr;
}

namespace
{
bool SetEnumValue(UObject* Object, const TCHAR* PropertyName, const FString& Name)
{
    FEnumProperty* Property =
        Object ? FindFProperty<FEnumProperty>(Object->GetClass(), PropertyName) : nullptr;
    UEnum* Enum = Property ? Property->GetEnum() : nullptr;
    const int64 Value = Enum ? Enum->GetValueByNameString(Name) : INDEX_NONE;
    if (!Property || Value == INDEX_NONE) return false;
    Property->GetUnderlyingProperty()->SetIntPropertyValue(
        Property->ContainerPtrToValuePtr<void>(Object), Value);
    return true;
}
}

bool ConfigureActorSource(
    UTakeRecorderSource* Source,
    const TSharedPtr<FJsonObject>& Payload,
    int32& OutAppliedOptions,
    FString& OutError)
{
    if (!Source) return false;
    for (const TPair<const TCHAR*, const TCHAR*> Field : {
             TPair<const TCHAR*, const TCHAR*>(TEXT("reduceKeys"), TEXT("bReduceKeys")),
             TPair<const TCHAR*, const TCHAR*>(TEXT("recordParentHierarchy"), TEXT("bRecordParentHierarchy"))})
    {
        if (!Payload->HasField(Field.Key)) continue;
        bool Value = false;
        FBoolProperty* Property =
            FindFProperty<FBoolProperty>(Source->GetClass(), Field.Value);
        if (!Payload->TryGetBoolField(Field.Key, Value) || !Property)
        {
            OutError = FString::Printf(
                TEXT("%s could not be applied"), Field.Key);
            return false;
        }
        Property->SetPropertyValue_InContainer(Source, Value);
        ++OutAppliedOptions;
    }
    FString RecordType;
    if (Payload->TryGetStringField(TEXT("recordType"), RecordType))
    {
        FString EnumName = RecordType.ToLower();
        if (EnumName == TEXT("projectdefault")) EnumName = TEXT("ProjectDefault");
        else if (EnumName == TEXT("possessable")) EnumName = TEXT("Possessable");
        else EnumName = TEXT("Spawnable");
        if (!SetEnumValue(Source, TEXT("RecordType"), EnumName))
        {
            OutError = TEXT("recordType could not be applied");
            return false;
        }
        ++OutAppliedOptions;
    }
    return true;
}

FTakeRecorderActorSourceState CaptureActorSourceOptions(
    UTakeRecorderSource* Source,
    const TSharedPtr<FJsonObject>& Payload)
{
    FTakeRecorderActorSourceState State;
    State.Source = Source;
    State.bEnabled = Source->bEnabled;
    if (Payload->HasField(TEXT("reduceKeys")))
    {
        State.bHasReduceKeys = true;
        if (const FBoolProperty* Property =
                FindFProperty<FBoolProperty>(
                    Source->GetClass(), TEXT("bReduceKeys")))
            State.bReduceKeys =
                Property->GetPropertyValue_InContainer(Source);
    }
    if (Payload->HasField(TEXT("recordParentHierarchy")))
    {
        State.bHasRecordParentHierarchy = true;
        if (const FBoolProperty* Property =
                FindFProperty<FBoolProperty>(
                    Source->GetClass(), TEXT("bRecordParentHierarchy")))
            State.bRecordParentHierarchy =
                Property->GetPropertyValue_InContainer(Source);
    }
    if (Payload->HasField(TEXT("recordType")))
    {
        State.bHasRecordType = true;
        if (const FEnumProperty* Property =
                FindFProperty<FEnumProperty>(
                    Source->GetClass(), TEXT("RecordType")))
            State.RecordTypeValue =
                Property->GetUnderlyingProperty()
                    ->GetSignedIntPropertyValue(
                        Property->ContainerPtrToValuePtr<void>(Source));
    }
    return State;
}

void RestoreActorSourceOptions(
    const TArray<FTakeRecorderActorSourceState>& States)
{
    for (const FTakeRecorderActorSourceState& State : States)
    {
        UTakeRecorderSource* Source = State.Source.Get();
        if (!Source) continue;
        Source->bEnabled = State.bEnabled;
        if (State.bHasReduceKeys)
            if (FBoolProperty* Property =
                    FindFProperty<FBoolProperty>(
                        Source->GetClass(), TEXT("bReduceKeys")))
                Property->SetPropertyValue_InContainer(
                    Source, State.bReduceKeys);
        if (State.bHasRecordParentHierarchy)
            if (FBoolProperty* Property =
                    FindFProperty<FBoolProperty>(
                        Source->GetClass(), TEXT("bRecordParentHierarchy")))
                Property->SetPropertyValue_InContainer(
                    Source, State.bRecordParentHierarchy);
        if (State.bHasRecordType)
            if (FEnumProperty* Property =
                    FindFProperty<FEnumProperty>(
                        Source->GetClass(), TEXT("RecordType")))
                Property->GetUnderlyingProperty()->SetIntPropertyValue(
                    Property->ContainerPtrToValuePtr<void>(Source),
                    State.RecordTypeValue);
    }
}

#endif
}
