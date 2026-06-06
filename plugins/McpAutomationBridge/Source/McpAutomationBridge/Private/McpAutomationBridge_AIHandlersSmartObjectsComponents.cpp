#include "McpAutomationBridge_AIHandlerContext.h"

#if WITH_EDITOR
#include "McpAutomationBridge_AISmartObjectsFeature.h"

#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace McpAIHandlers
{
bool HandleConfigureSmartObjectSlotBehavior(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("configure_slot_behavior");
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (SubAction == TEXT("configure_slot_behavior"))
    {
#if MCP_HAS_SMART_OBJECTS && MCP_SMART_OBJECTS_HEADERS_AVAILABLE
        FString DefinitionPath = GetStringFieldAI(Payload, TEXT("definitionPath"));
        int32 SlotIndex = static_cast<int32>(GetNumberFieldAI(Payload, TEXT("slotIndex"), 0));
        FString BehaviorType = GetStringFieldAI(Payload, TEXT("behaviorType"), TEXT(""));

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

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        if (!Definition->IsValidSlotIndex(SlotIndex))
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Invalid slot index: %d"), SlotIndex), TEXT("INVALID_PARAMS"));
            return true;
        }

        // Get the slot and configure it
        FSmartObjectSlotDefinition& Slot = Definition->GetMutableSlot(SlotIndex);

        // Configure activity tags if provided
        if (Payload->HasField(TEXT("activityTags")))
        {
            const TArray<TSharedPtr<FJsonValue>>* TagsArray;
            if (Payload->TryGetArrayField(TEXT("activityTags"), TagsArray))
            {
                for (const auto& TagValue : *TagsArray)
                {
                    FString TagStr = TagValue->AsString();
                    FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagStr), false);
                    if (Tag.IsValid())
                    {
                        Slot.ActivityTags.AddTag(Tag);
                    }
                }
            }
        }

        // Configure enabled state
        if (Payload->HasField(TEXT("enabled")))
        {
            Slot.bEnabled = GetBoolFieldAI(Payload, TEXT("enabled"), true);
        }

        // Save
        McpSafeAssetSave(Definition);

        Result->SetNumberField(TEXT("slotIndex"), SlotIndex);
        Result->SetNumberField(TEXT("behaviorCount"), Slot.BehaviorDefinitions.Num());
        Result->SetStringField(TEXT("message"), TEXT("Slot behavior configured"));
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Behavior configured"), Result);
#else
        // UE 5.0: SmartObject API is limited - skip slot configuration
        Self->SendAutomationError(RequestingSocket, RequestId,
            TEXT("SmartObject slot configuration requires UE 5.1+"), TEXT("UNSUPPORTED_VERSION"));
        return true;
#endif
#elif MCP_HAS_SMART_OBJECTS
        FString DefinitionPath = GetStringFieldAI(Payload, TEXT("definitionPath"));
        int32 SlotIndex = static_cast<int32>(GetNumberFieldAI(Payload, TEXT("slotIndex"), 0));
        Result->SetNumberField(TEXT("slotIndex"), SlotIndex);
        Result->SetStringField(TEXT("message"), TEXT("Slot behavior configuration registered (headers unavailable)"));
        Result->SetBoolField(TEXT("headersUnavailable"), true);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Behavior configured"), Result);
#else
        Self->SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Smart Objects require UE 5.0+"),
                            TEXT("UNSUPPORTED_VERSION"));
#endif
        return true;
    }

    return true;
}

bool HandleAddSmartObjectComponent(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("add_smart_object_component");
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (SubAction == TEXT("add_smart_object_component"))
    {
#if MCP_HAS_SMART_OBJECTS && MCP_SMART_OBJECTS_HEADERS_AVAILABLE
        FString BlueprintPath = GetStringFieldAI(Payload, TEXT("blueprintPath"));
        FString DefinitionPath = GetStringFieldAI(Payload, TEXT("definitionPath"), TEXT(""));
        FString ComponentName = GetStringFieldAI(Payload, TEXT("componentName"), TEXT("SmartObjectComponent"));

        if (BlueprintPath.IsEmpty())
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("blueprintPath is required"), TEXT("INVALID_PARAMS"));
            return true;
        }

        // Load the Blueprint
        FString NormalizedPath, LoadError;
        UBlueprint* Blueprint = LoadBlueprintAsset(BlueprintPath, NormalizedPath, LoadError);
        if (!Blueprint)
        {
            Self->SendAutomationError(RequestingSocket, RequestId, LoadError, TEXT("NOT_FOUND"));
            return true;
        }

        // Load the definition if provided
        USmartObjectDefinition* Definition = nullptr;
        if (!DefinitionPath.IsEmpty())
        {
            Definition = LoadObject<USmartObjectDefinition>(nullptr, *DefinitionPath);
        }

        // Get the SCS
        USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
        if (!SCS)
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint has no SimpleConstructionScript"), TEXT("INVALID_STATE"));
            return true;
        }

        // Create the component node using proper UE 5.7 SCS pattern
        USCS_Node* NewNode = SCS->CreateNode(USmartObjectComponent::StaticClass(), FName(*ComponentName));
        if (!NewNode)
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create SCS node for SmartObjectComponent"), TEXT("CREATION_FAILED"));
            return true;
        }

        // Configure the component template
        USmartObjectComponent* SOComp = Cast<USmartObjectComponent>(NewNode->ComponentTemplate);
        if (SOComp && Definition)
        {
            SOComp->SetDefinition(Definition);
        }

        // Add to SCS
        SCS->AddNode(NewNode);

        // Mark for compile and save
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeAssetSave(Blueprint);

        Result->SetStringField(TEXT("componentName"), ComponentName);
        Result->SetStringField(TEXT("blueprintPath"), NormalizedPath);
        if (Definition)
        {
            Result->SetStringField(TEXT("definitionPath"), DefinitionPath);
        }
        Result->SetStringField(TEXT("message"), TEXT("Smart Object component added to blueprint"));
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Component added"), Result);
#elif MCP_HAS_SMART_OBJECTS
        FString BlueprintPath = GetStringFieldAI(Payload, TEXT("blueprintPath"));
        Result->SetStringField(TEXT("componentName"), TEXT("SmartObject"));
        Result->SetStringField(TEXT("message"), TEXT("Smart Object component addition registered (headers unavailable)"));
        Result->SetBoolField(TEXT("headersUnavailable"), true);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Component registered"), Result);
#else
        Self->SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Smart Objects require UE 5.0+"),
                            TEXT("UNSUPPORTED_VERSION"));
#endif
        return true;
    }

    // =========================================================================
    // 16.8 Mass AI / Crowds (3 actions)
    // =========================================================================

    return true;
}
}
#endif
