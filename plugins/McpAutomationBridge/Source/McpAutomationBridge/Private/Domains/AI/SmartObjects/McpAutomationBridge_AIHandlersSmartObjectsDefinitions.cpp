#include "Domains/AI/McpAutomationBridge_AIHandlerContext.h"

#include "Domains/AI/SmartObjects/McpAutomationBridge_AISmartObjectsFeature.h"

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

// Implements the "create_smart_object_definition" action.
bool HandleCreateSmartObjectDefinition(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
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

    // Create the package and asset
    FString FullPath = Path / Name;
    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Failed to create package: %s"), *FullPath), TEXT("CREATION_FAILED"));
        return true;
    }

    USmartObjectDefinition* Definition = NewObject<USmartObjectDefinition>(Package, *Name, RF_Public | RF_Standalone);
    if (!Definition)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create SmartObjectDefinition asset"), TEXT("CREATION_FAILED"));
        return true;
    }

    // Save the asset
    McpSafeAssetSave(Definition);

    Result->SetStringField(TEXT("definitionPath"), FullPath);
    Result->SetNumberField(TEXT("slotCount"), 0);
    Result->SetStringField(TEXT("message"), TEXT("Smart Object Definition created"));
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Definition created"), Result);
    return true;
}

// Implements the "add_smart_object_slot" action.
bool HandleAddSmartObjectSlot(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
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
    int32 SlotIndex = -1;
    if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(SlotsProp))
    {
        FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Definition));
        SlotIndex = ArrayHelper.AddValue();
        if (FStructProperty* InnerStruct = CastField<FStructProperty>(ArrayProp->Inner))
        {
            InnerStruct->Struct->CopyScriptStruct(ArrayHelper.GetRawPtr(SlotIndex), &NewSlot);
        }
    }

    // Save
    McpSafeAssetSave(Definition);

    Result->SetNumberField(TEXT("slotIndex"), SlotIndex);
    Result->SetStringField(TEXT("definitionPath"), DefinitionPath);
    Result->SetStringField(TEXT("message"), TEXT("Slot added to Smart Object Definition"));
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Slot added"), Result);
    return true;
}
}
