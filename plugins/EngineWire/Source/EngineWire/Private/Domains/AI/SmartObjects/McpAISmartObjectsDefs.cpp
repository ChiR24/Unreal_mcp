#include "Domains/AI/McpAIContext.h"

#include "Domains/AI/SmartObjects/McpAISmartObjectsFeature.h"

namespace McpAIHandlers
{
// Tells the definition a slot was added the way the details panel does, so each engine version gives it what the
// editor gives a new slot (a fresh ID, the definition's world condition schema: without it every save warned
// "missing schema").
static void NotifySlotAdded(UObject* Definition, FArrayProperty* SlotsProp, int32 SlotIndex)
{
    FEditPropertyChain Chain;
    Chain.AddHead(SlotsProp);
    Chain.SetActivePropertyNode(SlotsProp);
    Chain.SetActiveMemberPropertyNode(SlotsProp);
    TArray<TMap<FString, int32>> Indices;
    Indices.AddDefaulted_GetRef().Add(SlotsProp->GetName(), SlotIndex);
    FPropertyChangedEvent Added(SlotsProp, EPropertyChangeType::ArrayAdd);
    Added.SetArrayIndexPerObject(Indices);
    Added.ObjectIteratorIndex = 0;
    FPropertyChangedChainEvent Event(Chain, Added);
    Definition->PostEditChangeChainProperty(Event);
}

// Implements the "create_smart_object_definition" action.
bool HandleCreateSmartObjectDefinition(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    UClass* DefinitionClass = FindSmartObjectClass(TEXT("SmartObjectDefinition"));
    if (!DefinitionClass)
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            TEXT("SmartObjects plugin is not enabled in this project. Enable the SmartObjects plugin to use Smart Object features."),
            TEXT("SMARTOBJECTS_PLUGIN_NOT_ENABLED"));
        return true;
    }

    FString Name = GetJsonStringField(Payload, TEXT("name"));
    FString Path = GetJsonStringField(Payload, TEXT("path"), TEXT("/Game/AI/SmartObjects"));

    if (Name.IsEmpty())
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Smart Object Definition name is required"), TEXT("INVALID_PARAMS"));
        return true;
    }

    // CreatePackage and NewObject rebuilt an asset already at the path, or crashed on another class.
    FString Error;
    FString Code;
    UObject* Definition = CreateAIAssetOfClass(DefinitionClass, Path, Name, TEXT("Smart Object Definition"), Error, Code);
    if (!Definition)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, Error, Code);
        return true;
    }

    Result->SetStringField(TEXT("definitionPath"), Definition->GetOutermost()->GetName());
    Result->SetNumberField(TEXT("slotCount"), 0);
    Result->SetBoolField(TEXT("saved"), McpSafeAssetSave(Definition));
    McpHandlerUtils::AddVerification(Result, Definition);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Definition created"), Result);
    return true;
}

// Implements the "add_smart_object_slot" action.
bool HandleAddSmartObjectSlot(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    FString DefinitionPath = GetJsonStringField(Payload, TEXT("definitionPath"));
    FVector Offset = ExtractVectorField(Payload, TEXT("offset"), FVector::ZeroVector);
    FRotator Rotation = ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    bool bEnabled = GetJsonBoolField(Payload, TEXT("enabled"), true);

    if (DefinitionPath.IsEmpty())
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("definitionPath is required"), TEXT("INVALID_PARAMS"));
        return true;
    }

    UClass* DefinitionClass = FindSmartObjectClass(TEXT("SmartObjectDefinition"));
    if (!DefinitionClass)
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            TEXT("SmartObjects plugin is not enabled in this project. Enable the SmartObjects plugin to use Smart Object features."),
            TEXT("SMARTOBJECTS_PLUGIN_NOT_ENABLED"));
        return true;
    }
    UObject* Definition = LoadObject<UObject>(nullptr, *DefinitionPath);
    if (!Definition || !Definition->IsA(DefinitionClass))
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("SmartObjectDefinition not found: %s"), *DefinitionPath), TEXT("NOT_FOUND"));
        return true;
    }

    // Every field is found before the slot is added, so a refusal changes nothing. Offset and Rotation are doubles
    // (Vector, Rotator) before 5.3 and floats (Vector3f, Rotator3f) from it; bEnabled exists from 5.2.
    FStructProperty* SlotProp = nullptr;
    FArrayProperty* SlotsProp = FindSmartObjectSlots(Definition, SlotProp);
    const FStructProperty* OffsetProp = SlotProp ? CastField<FStructProperty>(SlotProp->Struct->FindPropertyByName(TEXT("Offset"))) : nullptr;
    const FStructProperty* RotationProp = SlotProp ? CastField<FStructProperty>(SlotProp->Struct->FindPropertyByName(TEXT("Rotation"))) : nullptr;
    FBoolProperty* EnabledProp = SlotProp ? CastField<FBoolProperty>(SlotProp->Struct->FindPropertyByName(TEXT("bEnabled"))) : nullptr;
    const FName OffsetType = OffsetProp ? OffsetProp->Struct->GetFName() : NAME_None;
    const FName RotationType = RotationProp ? RotationProp->Struct->GetFName() : NAME_None;
    const bool bFloat = OffsetType == FName(TEXT("Vector3f")) && RotationType == FName(TEXT("Rotator3f"));
    const bool bDouble = OffsetType == FName(TEXT("Vector")) && RotationType == FName(TEXT("Rotator"));
    if (!SlotsProp || (!bFloat && !bDouble))
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("This engine's Smart Object definition has no Slots array with an offset and rotation to add to."), TEXT("INVALID_STATE"));
        return true;
    }
    if (!bEnabled && !EnabledProp)
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            TEXT("Smart Object slots on this engine have no enabled flag (it arrives in UE 5.2), so a disabled slot cannot be added. Nothing was added."),
            TEXT("UNSUPPORTED_VERSION"));
        return true;
    }

    Definition->Modify();
    FScriptArrayHelper ArrayHelper(SlotsProp, SlotsProp->ContainerPtrToValuePtr<void>(Definition));
    const int32 SlotIndex = ArrayHelper.AddValue();
    uint8* NewSlot = ArrayHelper.GetRawPtr(SlotIndex);
    if (bFloat)
    {
        *OffsetProp->ContainerPtrToValuePtr<FVector3f>(NewSlot) = FVector3f(Offset);
        *RotationProp->ContainerPtrToValuePtr<FRotator3f>(NewSlot) = FRotator3f(Rotation);
    }
    else
    {
        *OffsetProp->ContainerPtrToValuePtr<FVector>(NewSlot) = Offset;
        *RotationProp->ContainerPtrToValuePtr<FRotator>(NewSlot) = Rotation;
    }
    if (EnabledProp)
    {
        EnabledProp->SetPropertyValue_InContainer(NewSlot, bEnabled);
    }
    NotifySlotAdded(Definition, SlotsProp, SlotIndex);

    Result->SetNumberField(TEXT("slotIndex"), SlotIndex);
    Result->SetStringField(TEXT("definitionPath"), DefinitionPath);
    Result->SetBoolField(TEXT("saved"), McpSafeAssetSave(Definition));
    McpHandlerUtils::AddVerification(Result, Definition);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Slot added"), Result);
    return true;
}
}
