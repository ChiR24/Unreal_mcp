#include "Domains/AI/McpAIContext.h"

#include "Domains/AI/SmartObjects/McpAISmartObjectsFeature.h"

#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"

namespace McpAIHandlers
{
static bool IsSmartObjectsModuleAvailable()
{
    if (FModuleManager::Get().IsModuleLoaded(TEXT("SmartObjectsModule")))
    {
        return true;
    }
    if (FModuleManager::Get().ModuleExists(TEXT("SmartObjectsModule")))
    {
        return FModuleManager::Get().LoadModule(TEXT("SmartObjectsModule")) != nullptr;
    }
    return false;
}

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
    // Runtime check: Verify SmartObjects module is actually loaded
    if (!IsSmartObjectsModuleAvailable())
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
    USmartObjectDefinition* Definition = CreateAIAssetInPackage<USmartObjectDefinition>(Path, Name, TEXT("Smart Object Definition"), Error, Code);
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

    // Load the SmartObjectDefinition
    USmartObjectDefinition* Definition = LoadObject<USmartObjectDefinition>(nullptr, *DefinitionPath);
    if (!Definition)
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("SmartObjectDefinition not found: %s"), *DefinitionPath), TEXT("NOT_FOUND"));
        return true;
    }

    // Create and add a new slot using reflection to access private Slots array
    FSmartObjectSlotDefinition NewSlot;
#if ENGINE_MINOR_VERSION >= 3
    // UE 5.3+ uses FVector3f/FRotator3f and has bEnabled/ID members
    NewSlot.Offset = FVector3f(Offset);
    NewSlot.Rotation = FRotator3f(Rotation);
    NewSlot.bEnabled = bEnabled;
    NewSlot.ID = FGuid::NewGuid();
#else
    // UE 5.0-5.2 uses FVector/FRotator
    NewSlot.Offset = Offset;
    NewSlot.Rotation = Rotation;
#endif
    // Access slots via reflection
    FProperty* SlotsProp = Definition->GetClass()->FindPropertyByName(TEXT("Slots"));
    FArrayProperty* ArrayProp = CastField<FArrayProperty>(SlotsProp);
    FStructProperty* InnerStruct = ArrayProp ? CastField<FStructProperty>(ArrayProp->Inner) : nullptr;
    if (!InnerStruct)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("This engine's Smart Object definition has no Slots array to add to."), TEXT("INVALID_STATE"));
        return true;
    }
    Definition->Modify();
    FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Definition));
    const int32 SlotIndex = ArrayHelper.AddValue();
    InnerStruct->Struct->CopyScriptStruct(ArrayHelper.GetRawPtr(SlotIndex), &NewSlot);
    NotifySlotAdded(Definition, ArrayProp, SlotIndex);

    Result->SetNumberField(TEXT("slotIndex"), SlotIndex);
    Result->SetStringField(TEXT("definitionPath"), DefinitionPath);
    Result->SetBoolField(TEXT("saved"), McpSafeAssetSave(Definition));
    McpHandlerUtils::AddVerification(Result, Definition);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Slot added"), Result);
    return true;
}
}
