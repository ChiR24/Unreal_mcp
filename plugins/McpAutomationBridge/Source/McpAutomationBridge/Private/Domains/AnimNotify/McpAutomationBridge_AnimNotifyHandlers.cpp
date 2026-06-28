// McpAutomationBridge_AnimNotifyHandlers.cpp
// Animation Notify management on UAnimSequenceBase (AnimSequence/AnimMontage):
// - add_notify: Add a UAnimNotify at a specific time
// - add_notify_state: Add a UAnimNotifyState with begin/end times
// - remove_notify: Remove a notify by index
// - list_notifies: List all notifies on an animation asset
// - set_notify_properties: Set properties on an existing notify
// - list_notify_classes: List available notify classes

#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "UObject/UObjectIterator.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogMcpAnimNotifyHandlers, Log, All);

#if WITH_EDITOR

// Helper: Load an animation asset (AnimSequence or AnimMontage)
static UAnimSequenceBase* LoadAnimAsset(const FString& AssetPath)
{
    FString SanitizedPath = SanitizeProjectRelativePath(AssetPath);
    if (SanitizedPath.IsEmpty()) return nullptr;

    // Try AnimSequence first, then AnimMontage
    UObject* Asset = StaticLoadObject(UAnimSequenceBase::StaticClass(), nullptr, *SanitizedPath);
    return Cast<UAnimSequenceBase>(Asset);
}

// Helper: Find a UClass by name for notify types
static UClass* FindNotifyClass(const FString& ClassName, bool bIsState)
{
    UClass* BaseClass = bIsState ? UAnimNotifyState::StaticClass() : UAnimNotify::StaticClass();

    // Try exact path first
    UClass* FoundClass = FindObject<UClass>(nullptr, *ClassName);
    if (FoundClass && FoundClass->IsChildOf(BaseClass)) return FoundClass;

    // Try loading
    FoundClass = LoadObject<UClass>(nullptr, *ClassName);
    if (FoundClass && FoundClass->IsChildOf(BaseClass)) return FoundClass;

    // Try common prefixes
    TArray<FString> Prefixes = {
        FString::Printf(TEXT("/Script/Engine.%s"), *ClassName),
        FString::Printf(TEXT("/Script/Engine.AnimNotify_%s"), *ClassName),
        FString::Printf(TEXT("/Script/Engine.AnimNotifyState_%s"), *ClassName)
    };
    for (const FString& Prefix : Prefixes)
    {
        FoundClass = FindObject<UClass>(nullptr, *Prefix);
        if (!FoundClass) FoundClass = LoadObject<UClass>(nullptr, *Prefix);
        if (FoundClass && FoundClass->IsChildOf(BaseClass)) return FoundClass;
    }

    return nullptr;
}

// Helper: Serialize a notify event to JSON
static TSharedPtr<FJsonObject> NotifyToJson(const FAnimNotifyEvent& Event, int32 Index)
{
    TSharedPtr<FJsonObject> Json = MakeShareable(new FJsonObject());
    Json->SetNumberField(TEXT("index"), Index);
    Json->SetStringField(TEXT("notifyName"), Event.NotifyName.ToString());
    Json->SetNumberField(TEXT("triggerTime"), Event.GetTriggerTime());
    Json->SetNumberField(TEXT("duration"), Event.GetDuration());
    Json->SetNumberField(TEXT("trackIndex"), Event.TrackIndex);

    if (Event.Notify)
    {
        Json->SetStringField(TEXT("notifyClass"), Event.Notify->GetClass()->GetName());
        Json->SetStringField(TEXT("type"), TEXT("AnimNotify"));
    }
    else if (Event.NotifyStateClass)
    {
        Json->SetStringField(TEXT("notifyClass"), Event.NotifyStateClass->GetClass()->GetName());
        Json->SetStringField(TEXT("type"), TEXT("AnimNotifyState"));
        Json->SetNumberField(TEXT("endTime"), Event.GetTriggerTime() + Event.GetDuration());
    }
    else
    {
        Json->SetStringField(TEXT("type"), TEXT("Custom"));
    }

    return Json;
}

// ----------------------------------------------------------------------------
// add_notify
// ----------------------------------------------------------------------------
static bool HandleAddNotify(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"), TEXT(""));
    FString NotifyClassName = GetJsonStringField(Payload, TEXT("notifyClass"), TEXT("AnimNotify"));
    FString NotifyName = GetJsonStringField(Payload, TEXT("notifyName"), TEXT(""));
    float Time = GetJsonNumberField(Payload, TEXT("time"), 0.0f);
    int32 TrackIndex = static_cast<int32>(GetJsonNumberField(Payload, TEXT("trackIndex"), 0.0f));

    if (AssetPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("assetPath is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    UAnimSequenceBase* AnimAsset = LoadAnimAsset(AssetPath);
    if (!AnimAsset)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Animation asset not found: %s"), *AssetPath),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    UClass* NotifyClass = FindNotifyClass(NotifyClassName, false);
    if (!NotifyClass)
    {
        // Use base UAnimNotify if class not found
        NotifyClass = UAnimNotify::StaticClass();
    }

    // Create the notify object
    UAnimNotify* NewNotify = NewObject<UAnimNotify>(AnimAsset, NotifyClass, NAME_None, RF_Transactional);
    if (!NewNotify)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to create notify object"), nullptr);
        return true;
    }

    // Add the notify event
    FAnimNotifyEvent& NewEvent = AnimAsset->Notifies.AddDefaulted_GetRef();
    NewEvent.Notify = NewNotify;
    NewEvent.NotifyName = NotifyName.IsEmpty() ? FName(*NotifyClass->GetName()) : FName(*NotifyName);
    NewEvent.SetTime(Time);
    NewEvent.TrackIndex = TrackIndex;
    NewEvent.NotifyFilterType = ENotifyFilterType::NoFiltering;

    AnimAsset->MarkPackageDirty();
    McpSafeAssetSave(AnimAsset);

    int32 NotifyIndex = AnimAsset->Notifies.Num() - 1;

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetNumberField(TEXT("notifyIndex"), NotifyIndex);
    ResultJson->SetStringField(TEXT("notifyClass"), NotifyClass->GetName());
    ResultJson->SetStringField(TEXT("notifyName"), NewEvent.NotifyName.ToString());
    ResultJson->SetNumberField(TEXT("time"), Time);
    ResultJson->SetStringField(TEXT("assetPath"), AnimAsset->GetPathName());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Added notify '%s' at %.2fs"), *NewEvent.NotifyName.ToString(), Time), ResultJson);
    return true;
}

// ----------------------------------------------------------------------------
// add_notify_state
// ----------------------------------------------------------------------------
static bool HandleAddNotifyState(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"), TEXT(""));
    FString NotifyClassName = GetJsonStringField(Payload, TEXT("notifyClass"), TEXT("AnimNotifyState"));
    FString NotifyName = GetJsonStringField(Payload, TEXT("notifyName"), TEXT(""));
    float BeginTime = GetJsonNumberField(Payload, TEXT("beginTime"), 0.0f);
    float EndTime = GetJsonNumberField(Payload, TEXT("endTime"), 1.0f);
    int32 TrackIndex = static_cast<int32>(GetJsonNumberField(Payload, TEXT("trackIndex"), 0.0f));

    if (AssetPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("assetPath is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    if (EndTime <= BeginTime)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("endTime must be greater than beginTime"), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UAnimSequenceBase* AnimAsset = LoadAnimAsset(AssetPath);
    if (!AnimAsset)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Animation asset not found: %s"), *AssetPath),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    UClass* NotifyClass = FindNotifyClass(NotifyClassName, true);
    if (!NotifyClass)
    {
        NotifyClass = UAnimNotifyState::StaticClass();
    }

    UAnimNotifyState* NewNotifyState = NewObject<UAnimNotifyState>(AnimAsset, NotifyClass, NAME_None, RF_Transactional);
    if (!NewNotifyState)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to create notify state object"), nullptr);
        return true;
    }

    FAnimNotifyEvent& NewEvent = AnimAsset->Notifies.AddDefaulted_GetRef();
    NewEvent.NotifyStateClass = NewNotifyState;
    NewEvent.NotifyName = NotifyName.IsEmpty() ? FName(*NotifyClass->GetName()) : FName(*NotifyName);
    NewEvent.SetTime(BeginTime);
    NewEvent.SetDuration(EndTime - BeginTime);
    NewEvent.TrackIndex = TrackIndex;
    NewEvent.NotifyFilterType = ENotifyFilterType::NoFiltering;

    AnimAsset->MarkPackageDirty();
    McpSafeAssetSave(AnimAsset);

    int32 NotifyIndex = AnimAsset->Notifies.Num() - 1;

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetNumberField(TEXT("notifyIndex"), NotifyIndex);
    ResultJson->SetStringField(TEXT("notifyClass"), NotifyClass->GetName());
    ResultJson->SetStringField(TEXT("notifyName"), NewEvent.NotifyName.ToString());
    ResultJson->SetNumberField(TEXT("beginTime"), BeginTime);
    ResultJson->SetNumberField(TEXT("endTime"), EndTime);
    ResultJson->SetNumberField(TEXT("duration"), EndTime - BeginTime);
    ResultJson->SetStringField(TEXT("assetPath"), AnimAsset->GetPathName());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Added notify state '%s' at %.2fs-%.2fs"),
            *NewEvent.NotifyName.ToString(), BeginTime, EndTime), ResultJson);
    return true;
}

// ----------------------------------------------------------------------------
// remove_notify
// ----------------------------------------------------------------------------
static bool HandleRemoveNotify(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"), TEXT(""));
    int32 NotifyIndex = static_cast<int32>(GetJsonNumberField(Payload, TEXT("notifyIndex"), -1.0f));

    if (AssetPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("assetPath is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    UAnimSequenceBase* AnimAsset = LoadAnimAsset(AssetPath);
    if (!AnimAsset)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Animation asset not found: %s"), *AssetPath),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    if (NotifyIndex < 0 || NotifyIndex >= AnimAsset->Notifies.Num())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Invalid notify index %d (asset has %d notifies)"),
                NotifyIndex, AnimAsset->Notifies.Num()),
            nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString RemovedName = AnimAsset->Notifies[NotifyIndex].NotifyName.ToString();
    AnimAsset->Notifies.RemoveAt(NotifyIndex);
    AnimAsset->MarkPackageDirty();
    McpSafeAssetSave(AnimAsset);

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetStringField(TEXT("removedNotify"), RemovedName);
    ResultJson->SetNumberField(TEXT("removedIndex"), NotifyIndex);
    ResultJson->SetNumberField(TEXT("remainingNotifies"), AnimAsset->Notifies.Num());
    ResultJson->SetStringField(TEXT("assetPath"), AnimAsset->GetPathName());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Removed notify '%s' at index %d"), *RemovedName, NotifyIndex), ResultJson);
    return true;
}

// ----------------------------------------------------------------------------
// list_notifies
// ----------------------------------------------------------------------------
static bool HandleListNotifies(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"), TEXT(""));
    if (AssetPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("assetPath is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    UAnimSequenceBase* AnimAsset = LoadAnimAsset(AssetPath);
    if (!AnimAsset)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Animation asset not found: %s"), *AssetPath),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    TArray<TSharedPtr<FJsonValue>> NotifiesArray;
    for (int32 i = 0; i < AnimAsset->Notifies.Num(); i++)
    {
        TSharedPtr<FJsonObject> NotifyJson = NotifyToJson(AnimAsset->Notifies[i], i);
        NotifiesArray.Add(MakeShareable(new FJsonValueObject(NotifyJson)));
    }

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetArrayField(TEXT("notifies"), NotifiesArray);
    ResultJson->SetNumberField(TEXT("count"), NotifiesArray.Num());
    ResultJson->SetStringField(TEXT("assetPath"), AnimAsset->GetPathName());
    ResultJson->SetStringField(TEXT("assetClass"), AnimAsset->GetClass()->GetName());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Found %d notifies on %s"), NotifiesArray.Num(), *AnimAsset->GetName()), ResultJson);
    return true;
}

// ----------------------------------------------------------------------------
// set_notify_properties
// ----------------------------------------------------------------------------
static bool HandleSetNotifyProperties(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString AssetPath = GetJsonStringField(Payload, TEXT("assetPath"), TEXT(""));
    int32 NotifyIndex = static_cast<int32>(GetJsonNumberField(Payload, TEXT("notifyIndex"), -1.0f));

    if (AssetPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("assetPath is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    UAnimSequenceBase* AnimAsset = LoadAnimAsset(AssetPath);
    if (!AnimAsset)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Animation asset not found: %s"), *AssetPath),
            nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    if (NotifyIndex < 0 || NotifyIndex >= AnimAsset->Notifies.Num())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Invalid notify index %d"), NotifyIndex),
            nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FAnimNotifyEvent& Event = AnimAsset->Notifies[NotifyIndex];
    UObject* NotifyObj = Event.Notify ? static_cast<UObject*>(Event.Notify) : static_cast<UObject*>(Event.NotifyStateClass);

    if (!NotifyObj)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Notify at index has no notify object"), nullptr);
        return true;
    }

    const TSharedPtr<FJsonObject>* PropsObj = nullptr;
    if (!Payload->TryGetObjectField(TEXT("properties"), PropsObj) || !PropsObj || !(*PropsObj).IsValid())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("properties object is required"), nullptr, TEXT("MISSING_PARAMETER"));
        return true;
    }

    int32 PropsSet = 0;
    for (const auto& Pair : (*PropsObj)->Values)
    {
        FProperty* Property = NotifyObj->GetClass()->FindPropertyByName(FName(*Pair.Key));
        if (!Property) continue;

        void* ValuePtr = Property->ContainerPtrToValuePtr<void>(NotifyObj);
        FString ValueStr = Pair.Value->Type == EJson::String ? Pair.Value->AsString()
            : Pair.Value->Type == EJson::Number ? FString::SanitizeFloat(Pair.Value->AsNumber())
            : Pair.Value->Type == EJson::Boolean ? (Pair.Value->AsBool() ? TEXT("True") : TEXT("False"))
            : TEXT("");

        if (!ValueStr.IsEmpty())
        {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            if (Property->ImportText_Direct(*ValueStr, ValuePtr, NotifyObj, PPF_None))
#else
            if (Property->ImportText(*ValueStr, ValuePtr, PPF_None, NotifyObj))
#endif
            {
                PropsSet++;
            }
        }
    }

    AnimAsset->MarkPackageDirty();
    McpSafeAssetSave(AnimAsset);

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetNumberField(TEXT("propertiesSet"), PropsSet);
    ResultJson->SetNumberField(TEXT("notifyIndex"), NotifyIndex);
    ResultJson->SetStringField(TEXT("assetPath"), AnimAsset->GetPathName());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Set %d properties on notify at index %d"), PropsSet, NotifyIndex), ResultJson);
    return true;
}

// ----------------------------------------------------------------------------
// list_notify_classes
// ----------------------------------------------------------------------------
static bool HandleListNotifyClasses(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    TArray<TSharedPtr<FJsonValue>> NotifyClasses;
    TArray<TSharedPtr<FJsonValue>> StateClasses;

    for (TObjectIterator<UClass> It; It; ++It)
    {
        UClass* Class = *It;
        if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated)) continue;

        if (Class->IsChildOf(UAnimNotify::StaticClass()) && Class != UAnimNotify::StaticClass())
        {
            TSharedPtr<FJsonObject> ClassJson = MakeShareable(new FJsonObject());
            ClassJson->SetStringField(TEXT("className"), Class->GetName());
            ClassJson->SetStringField(TEXT("classPath"), Class->GetPathName());
            NotifyClasses.Add(MakeShareable(new FJsonValueObject(ClassJson)));
        }
        else if (Class->IsChildOf(UAnimNotifyState::StaticClass()) && Class != UAnimNotifyState::StaticClass())
        {
            TSharedPtr<FJsonObject> ClassJson = MakeShareable(new FJsonObject());
            ClassJson->SetStringField(TEXT("className"), Class->GetName());
            ClassJson->SetStringField(TEXT("classPath"), Class->GetPathName());
            StateClasses.Add(MakeShareable(new FJsonValueObject(ClassJson)));
        }
    }

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());
    ResultJson->SetArrayField(TEXT("notifyClasses"), NotifyClasses);
    ResultJson->SetArrayField(TEXT("stateClasses"), StateClasses);
    ResultJson->SetNumberField(TEXT("notifyCount"), NotifyClasses.Num());
    ResultJson->SetNumberField(TEXT("stateCount"), StateClasses.Num());

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Found %d notify classes and %d state classes"),
            NotifyClasses.Num(), StateClasses.Num()), ResultJson);
    return true;
}

#endif // WITH_EDITOR

// ============================================================================
// Main Dispatcher
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleManageAnimNotifyAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString SubAction = GetJsonStringField(Payload, TEXT("subAction"), TEXT(""));

    UE_LOG(LogMcpAnimNotifyHandlers, Verbose, TEXT("HandleManageAnimNotifyAction: SubAction=%s"), *SubAction);

    if (SubAction == TEXT("add_notify")) return HandleAddNotify(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("add_notify_state")) return HandleAddNotifyState(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("remove_notify")) return HandleRemoveNotify(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("list_notifies")) return HandleListNotifies(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("set_notify_properties")) return HandleSetNotifyProperties(this, RequestId, Payload, Socket);
    if (SubAction == TEXT("list_notify_classes")) return HandleListNotifyClasses(this, RequestId, Payload, Socket);

    SendAutomationResponse(Socket, RequestId, false,
        FString::Printf(TEXT("Unknown anim_notify subAction: %s"), *SubAction), nullptr, TEXT("UNKNOWN_ACTION"));
    return true;

#else
    SendAutomationResponse(Socket, RequestId, false,
        TEXT("Animation notify operations require editor build"), nullptr, TEXT("EDITOR_ONLY"));
    return true;
#endif
}
