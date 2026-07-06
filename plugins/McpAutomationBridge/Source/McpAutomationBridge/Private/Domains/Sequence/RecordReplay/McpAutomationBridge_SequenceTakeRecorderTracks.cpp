#include "Domains/Sequence/RecordReplay/McpAutomationBridge_SequenceTakeRecorderInternal.h"

#if MCP_SEQUENCE_HAS_TAKE_RECORDER_API
#include "UObject/UnrealType.h"
#endif

namespace McpSequenceRecordReplay {
#if MCP_SEQUENCE_HAS_TAKE_RECORDER_API
namespace {
struct FMapState {
    UObject* Map = nullptr; FBoolProperty* Property = nullptr; void* Entry = nullptr;
    bool bBefore = false; bool bAfter = false;
}; struct FSourceState {
    UTakeRecorderSource* Source = nullptr;
    FBoolProperty* Reduce = nullptr; FBoolProperty* Parent = nullptr;
    FEnumProperty* RecordType = nullptr;
    bool bReduce = false; bool bParent = false; int64 RecordTypeValue = 0;
};

bool ReadNames(const TSharedPtr<FJsonObject>& Payload, const TCHAR* Field,
    TSet<FString>& OutNames, FString& OutError) {
    if (!Payload->HasField(Field)) return true;
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Payload->TryGetArrayField(Field, Values) || !Values) {
        OutError = FString::Printf(TEXT("%s must be an array of strings"), Field);
        return false;
    }
    for (const TSharedPtr<FJsonValue>& Value : *Values) {
        FString Name;
        if (!Value.IsValid() || !Value->TryGetString(Name) ||
            Name.TrimStartAndEnd().IsEmpty()) {
            OutError = FString::Printf(TEXT("%s must contain non-empty strings"), Field);
            return false;
        }
        OutNames.Add(Name.TrimStartAndEnd().ToLower());
    }
    return true; }

bool ReadBool(const TSharedPtr<FJsonObject>& Payload, const TCHAR* Field,
    bool DefaultValue, bool& OutValue, FString& OutError) {
    OutValue = DefaultValue;
    if (!Payload->HasField(Field) ||
        Payload->TryGetBoolField(Field, OutValue)) return true;
    OutError = FString::Printf(TEXT("%s must be a boolean"), Field);
    return false; }

UObject* GetRecordedMap(UTakeRecorderSource* Source) {
    FObjectPropertyBase* Property = Source
        ? FindFProperty<FObjectPropertyBase>(
            Source->GetClass(), TEXT("RecordedProperties")) : nullptr;
    return Property ? Property->GetObjectPropertyValue_InContainer(Source) : nullptr; }

AActor* GetSourceActor(UTakeRecorderSource* Source) {
    FSoftObjectProperty* Property = Source
        ? FindFProperty<FSoftObjectProperty>(Source->GetClass(), TEXT("Target")) : nullptr;
    return Property
        ? Cast<AActor>(Property->GetPropertyValue_InContainer(Source).Get())
        : nullptr; }

void CollectMapStates(UObject* Map, const TSet<FString>& Names,
    bool bEnabled, bool bDisableOthers, TArray<FMapState>& OutStates,
    int32& OutMatched) {
    if (!Map) return;
    FArrayProperty* Array =
        FindFProperty<FArrayProperty>(Map->GetClass(), TEXT("Properties"));
    FStructProperty* EntryType =
        Array ? CastField<FStructProperty>(Array->Inner) : nullptr;
    FNameProperty* Name = EntryType
        ? FindFProperty<FNameProperty>(EntryType->Struct, TEXT("PropertyName"))
        : nullptr;
    FBoolProperty* Enabled = EntryType
        ? FindFProperty<FBoolProperty>(EntryType->Struct, TEXT("bEnabled"))
        : nullptr;
    if (Array && Name && Enabled) {
        FScriptArrayHelper Helper(
            Array, Array->ContainerPtrToValuePtr<void>(Map));
        for (int32 Index = 0; Index < Helper.Num(); ++Index) {
            void* Entry = Helper.GetRawPtr(Index);
            const bool bMatch = Names.Contains(
                Name->GetPropertyValue_InContainer(Entry).ToString().ToLower());
            if (bMatch) ++OutMatched;
            if (bMatch || bDisableOthers)
                OutStates.Add({Map, Enabled, Entry,
                    Enabled->GetPropertyValue_InContainer(Entry),
                    bMatch ? bEnabled : false});
        }
    }
    FArrayProperty* Children =
        FindFProperty<FArrayProperty>(Map->GetClass(), TEXT("Children"));
    FObjectPropertyBase* ChildType =
        Children ? CastField<FObjectPropertyBase>(Children->Inner) : nullptr;
    if (!Children || !ChildType) return;
    FScriptArrayHelper Helper(
        Children, Children->ContainerPtrToValuePtr<void>(Map));
    for (int32 Index = 0; Index < Helper.Num(); ++Index)
        CollectMapStates(
            ChildType->GetObjectPropertyValue(Helper.GetRawPtr(Index)),
            Names, bEnabled, bDisableOthers, OutStates, OutMatched); }

FSourceState CaptureSource(UTakeRecorderSource* Source,
    const TSharedPtr<FJsonObject>& Payload) {
    FSourceState State; State.Source = Source;
    if (Payload->HasField(TEXT("reduceKeys"))) {
        State.Reduce = FindFProperty<FBoolProperty>(
            Source->GetClass(), TEXT("bReduceKeys"));
        if (State.Reduce)
            State.bReduce = State.Reduce->GetPropertyValue_InContainer(Source);
    }
    if (Payload->HasField(TEXT("recordParentHierarchy"))) {
        State.Parent = FindFProperty<FBoolProperty>(
            Source->GetClass(), TEXT("bRecordParentHierarchy"));
        if (State.Parent)
            State.bParent = State.Parent->GetPropertyValue_InContainer(Source);
    }
    if (Payload->HasField(TEXT("recordType"))) {
        State.RecordType = FindFProperty<FEnumProperty>(
            Source->GetClass(), TEXT("RecordType"));
        if (State.RecordType) State.RecordTypeValue =
            State.RecordType->GetUnderlyingProperty()->GetSignedIntPropertyValue(
                State.RecordType->ContainerPtrToValuePtr<void>(Source));
    }
    return State; }

void RestoreSources(const TArray<FSourceState>& States) {
    for (const FSourceState& State : States) {
        if (State.Reduce) State.Reduce->SetPropertyValue_InContainer(State.Source, State.bReduce);
        if (State.Parent) State.Parent->SetPropertyValue_InContainer(State.Source, State.bParent);
        if (State.RecordType)
            State.RecordType->GetUnderlyingProperty()->SetIntPropertyValue(
                State.RecordType->ContainerPtrToValuePtr<void>(State.Source),
                State.RecordTypeValue);
    } }

void RollBackAdded(UTakeRecorderSources* Sources,
    const TSet<UTakeRecorderSource*>& Before) {
    for (UTakeRecorderSource* Source : Sources->GetSourcesCopy())
        if (!Before.Contains(Source)) Sources->RemoveSource(Source); }
}

bool HandleConfigureRecordedTracks(UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket, UTakeRecorderPanel* Panel,
    bool& OutSucceeded) {
    OutSucceeded = false;
    auto Fail = [&](const FString& Message, const TCHAR* Code) {
        Subsystem->SendAutomationError(Socket, RequestId, Message, Code);
        return true; };
    if (UTakeRecorderBlueprintLibrary::GetActiveRecorder()) return Fail(
        TEXT("Recorded tracks cannot change during an active recording"),
        TEXT("RECORDING_ACTIVE"));
    UTakeRecorderSources* Sources = Panel ? Panel->GetSources() : nullptr;
    if (!Sources) return Fail(
        TEXT("Take Recorder sources are unavailable"), TEXT("NOT_AVAILABLE"));

    FString Error; TSet<FString> Names;
    for (const TCHAR* Field : {
        TEXT("properties"), TEXT("trackNames"), TEXT("tracks")})
        if (!ReadNames(Payload, Field, Names, Error))
            return Fail(Error, TEXT("INVALID_ARGUMENT"));
    bool bEnabled = true, bDisableOthers = false;
    if (!ReadBool(Payload, TEXT("enabled"), true, bEnabled, Error) ||
        !ReadBool(Payload, TEXT("disableOthers"), false, bDisableOthers, Error))
        return Fail(Error, TEXT("INVALID_ARGUMENT"));

    FString ClassError;
    UClass* ActorClass = ResolveTakeRecorderActorSourceClass(ClassError);
    if (!ActorClass) return Fail(ClassError, TEXT("TAKE_ACTOR_SOURCE_UNAVAILABLE"));
    if (!ValidateTakeActorSourceOptions(ActorClass, Payload, Error))
        return Fail(Error, TEXT("INVALID_ARGUMENT"));
    const bool bHasOptions = Payload->HasField(TEXT("reduceKeys")) ||
        Payload->HasField(TEXT("recordParentHierarchy")) ||
        Payload->HasField(TEXT("recordType"));
    if (Names.Num() == 0 && !bDisableOthers && !bHasOptions) return Fail(
        TEXT("No recorded properties or source options were requested"),
        TEXT("INVALID_ARGUMENT"));

    TSet<FString> RequestedNames;
    for (const TCHAR* Field : {
        TEXT("actorNames"), TEXT("actors"), TEXT("sourceActors")})
        if (!ReadNames(Payload, Field, RequestedNames, Error))
            return Fail(Error, TEXT("INVALID_ARGUMENT"));
    if (Payload->HasField(TEXT("actorName"))) {
        FString Name;
        if (!Payload->TryGetStringField(TEXT("actorName"), Name) ||
            Name.TrimStartAndEnd().IsEmpty())
            return Fail(TEXT("actorName must be a non-empty string"),
                TEXT("INVALID_ARGUMENT"));
        RequestedNames.Add(Name.TrimStartAndEnd().ToLower());
    }
    TSet<AActor*> RequestedActors;
    for (const FString& Name : RequestedNames) {
        AActor* Actor = FindTakeRecorderActor(Name);
        if (!Actor) return Fail(
            FString::Printf(TEXT("Take Recorder actor not found: %s"), *Name),
            TEXT("ACTOR_NOT_FOUND"));
        RequestedActors.Add(Actor); }

    TSet<UTakeRecorderSource*> Before;
    TSet<AActor*> ExistingActors;
    for (UTakeRecorderSource* Source : Sources->GetSourcesCopy()) {
        Before.Add(Source);
        if (Source && Source->IsA(ActorClass)) ExistingActors.Add(GetSourceActor(Source)); }
    TArray<FString> MissingActors;
    for (AActor* Actor : RequestedActors)
        if (!ExistingActors.Contains(Actor)) MissingActors.Add(Actor->GetName());
    if (MissingActors.Num() > 0) {
        TSharedPtr<FJsonObject> AddPayload = MakeShared<FJsonObject>();
        TArray<TSharedPtr<FJsonValue>> Values;
        for (const FString& Name : MissingActors) Values.Add(MakeShared<FJsonValueString>(Name));
        AddPayload->SetArrayField(TEXT("actorNames"), Values);
        for (const TCHAR* Field : {
            TEXT("reduceKeys"), TEXT("recordParentHierarchy"), TEXT("recordType")})
            if (const TSharedPtr<FJsonValue>* Value = Payload->Values.Find(Field))
                AddPayload->SetField(Field, *Value);
        int32 Added = 0, Requested = 0; FString Code;
        Sources->Modify();
        if (!ConfigureSources(
            Panel, AddPayload, Added, Requested, Code, Error)) {
            RollBackAdded(Sources, Before);
            return Fail(Error, *Code); }
    }

    TArray<UTakeRecorderSource*> Targets;
    for (UTakeRecorderSource* Source : Sources->GetSourcesCopy())
        if (Source && Source->IsA(ActorClass) &&
            (RequestedActors.Num() == 0 ||
                RequestedActors.Contains(GetSourceActor(Source))))
            Targets.Add(Source);
    if (Targets.Num() == 0) {
        RollBackAdded(Sources, Before);
        return Fail(TEXT("No matching actor Take Recorder sources are configured"),
            TEXT("NO_ACTOR_SOURCES")); }

    TArray<FMapState> MapStates; TArray<UObject*> RootMaps; int32 Matched = 0;
    for (UTakeRecorderSource* Source : Targets) {
        UObject* Root = GetRecordedMap(Source); RootMaps.Add(Root);
        CollectMapStates(
            Root, Names, bEnabled, bDisableOthers, MapStates, Matched); }
    if (Names.Num() > 0 && Matched == 0) {
        RollBackAdded(Sources, Before);
        return Fail(TEXT("None of the requested recorded properties were found"),
            TEXT("RECORDED_PROPERTY_NOT_FOUND")); }

    TArray<FSourceState> SourceStates;
    for (UTakeRecorderSource* Source : Targets) SourceStates.Add(CaptureSource(Source, Payload));
    int32 AppliedOptions = 0;
    for (UTakeRecorderSource* Source : Targets) {
        Source->Modify();
        if (!ConfigureActorSource(Source, Payload, AppliedOptions, Error)) {
            RestoreSources(SourceStates); RollBackAdded(Sources, Before);
            return Fail(Error, TEXT("SOURCE_CONFIGURATION_FAILED")); }
    }
    int32 Changed = 0; TSet<UObject*> ModifiedMaps;
    for (const FMapState& State : MapStates)
        if (State.bBefore != State.bAfter) {
            if (!ModifiedMaps.Contains(State.Map)) {
                State.Map->Modify(); ModifiedMaps.Add(State.Map); }
            State.Property->SetPropertyValue_InContainer(
                State.Entry, State.bAfter);
            ++Changed;
        }
    for (UObject* Root : RootMaps) if (Root) Root->PostEditChange();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("actorSourceCount"), Targets.Num());
    Result->SetNumberField(TEXT("matchedProperties"), Matched);
    Result->SetNumberField(TEXT("changedProperties"), Changed);
    Result->SetNumberField(TEXT("appliedSourceOptions"), AppliedOptions);
    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        TEXT("Recorded track settings configured"), Result);
    OutSucceeded = true;
    return true; }
#endif
}
