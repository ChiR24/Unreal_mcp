#include "Domains/AI/McpAIContext.h"

#include "Domains/AI/SmartObjects/McpAISmartObjectsFeature.h"

#include "Components/ActorComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace McpAIHandlers
{
static const TCHAR* const SmartObjectsDisabledMessage =
    TEXT("SmartObjects plugin is not enabled in this project. Enable the SmartObjects plugin to use Smart Object features.");

// Points a Smart Object component at Definition. From 5.4 SetDefinition is a UFUNCTION that also syncs the
// definition's parameters into DefinitionRef; before, it was an inline setter of the DefinitionAsset property.
// Answers whether the component now holds Definition, read back from where this engine stores it.
static bool SetSmartObjectComponentDefinition(UObject* Component, UObject* Definition)
{
    UClass* Class = Component->GetClass();
    FObjectProperty* AssetProp = CastField<FObjectProperty>(Class->FindPropertyByName(TEXT("DefinitionAsset")));
    if (UFunction* Setter = Component->FindFunction(TEXT("SetDefinition")))
    {
        struct FSetDefinitionParams { UObject* Value; } Params{ Definition };
        Component->ProcessEvent(Setter, &Params);
    }
    else if (AssetProp)
    {
        AssetProp->SetObjectPropertyValue_InContainer(Component, Definition);
    }
    if (AssetProp)
    {
        return AssetProp->GetObjectPropertyValue_InContainer(Component) == Definition;
    }
    const FStructProperty* RefProp = CastField<FStructProperty>(Class->FindPropertyByName(TEXT("DefinitionRef")));
    const FObjectProperty* RefAsset = RefProp ? CastField<FObjectProperty>(RefProp->Struct->FindPropertyByName(TEXT("SmartObjectDefinition"))) : nullptr;
    return RefAsset && RefAsset->GetObjectPropertyValue_InContainer(RefProp->ContainerPtrToValuePtr<void>(Component)) == Definition;
}

// Implements the "configure_slot_behavior" action.
bool HandleConfigureSmartObjectSlotBehavior(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    FString DefinitionPath = GetJsonStringField(Payload, TEXT("definitionPath"));
    int32 SlotIndex = static_cast<int32>(GetJsonNumberField(Payload, TEXT("slotIndex"), 0));

    if (DefinitionPath.IsEmpty())
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("definitionPath is required"), TEXT("INVALID_PARAMS"));
        return true;
    }

    UClass* DefinitionClass = FindSmartObjectClass(TEXT("SmartObjectDefinition"));
    if (!DefinitionClass)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, SmartObjectsDisabledMessage, TEXT("SMARTOBJECTS_PLUGIN_NOT_ENABLED"));
        return true;
    }
    UObject* Definition = LoadObject<UObject>(nullptr, *DefinitionPath);
    if (!Definition || !Definition->IsA(DefinitionClass))
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("SmartObjectDefinition not found: %s"), *DefinitionPath), TEXT("NOT_FOUND"));
        return true;
    }

    FStructProperty* SlotProp = nullptr;
    FArrayProperty* SlotsProp = FindSmartObjectSlots(Definition, SlotProp);
    if (!SlotsProp)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("This engine's Smart Object definition has no Slots array."), TEXT("INVALID_STATE"));
        return true;
    }
    FScriptArrayHelper Slots(SlotsProp, SlotsProp->ContainerPtrToValuePtr<void>(Definition));
    if (!Slots.IsValidIndex(SlotIndex))
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Invalid slot index: %d"), SlotIndex), TEXT("INVALID_PARAMS"));
        return true;
    }

    // A call with neither field used to save and answer success.
    const TArray<TSharedPtr<FJsonValue>>* TagsArray = nullptr;
    const bool bHasTags = Payload->TryGetArrayField(TEXT("activityTags"), TagsArray) && TagsArray;
    const bool bHasEnabled = Payload->HasField(TEXT("enabled"));
    if (!bHasTags && !bHasEnabled)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Nothing to set: send activityTags or enabled."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Slots carry ActivityTags from 5.1 and bEnabled from 5.2; asking for one this engine lacks changes nothing.
    const FStructProperty* TagsProp = CastField<FStructProperty>(SlotProp->Struct->FindPropertyByName(TEXT("ActivityTags")));
    FBoolProperty* EnabledProp = CastField<FBoolProperty>(SlotProp->Struct->FindPropertyByName(TEXT("bEnabled")));
    const bool bNoTags = bHasTags && !(TagsProp && TagsProp->Struct == FGameplayTagContainer::StaticStruct());
    if (bNoTags || (bHasEnabled && !EnabledProp))
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Smart Object slots on this engine have no %s (activity tags arrive in UE 5.1, the enabled flag in 5.2). Nothing was changed."),
                bNoTags ? TEXT("activity tags") : TEXT("enabled flag")),
            TEXT("UNSUPPORTED_VERSION"));
        return true;
    }

    // Resolve every tag first: an unregistered one used to be dropped while the call still reported success.
    FGameplayTagContainer NewTags;
    if (bHasTags)
    {
        TArray<FString> UnknownTags;
        for (const auto& TagValue : *TagsArray)
        {
            const FString TagStr = TagValue.IsValid() ? TagValue->AsString() : FString();
            const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagStr), false);
            if (Tag.IsValid()) { NewTags.AddTag(Tag); } else { UnknownTags.Add(TagStr); }
        }
        if (UnknownTags.Num() > 0)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Gameplay tag(s) not registered in this project: %s. Register them in the project's GameplayTags settings first, then retry. Nothing was changed."), *FString::Join(UnknownTags, TEXT(", "))),
                TEXT("GAMEPLAY_TAG_NOT_REGISTERED"));
            return true;
        }
    }

    Definition->Modify();
    uint8* Slot = Slots.GetRawPtr(SlotIndex);
    if (bHasTags)
    {
        TagsProp->ContainerPtrToValuePtr<FGameplayTagContainer>(Slot)->AppendTags(NewTags);
    }
    if (bHasEnabled)
    {
        EnabledProp->SetPropertyValue_InContainer(Slot, GetJsonBoolField(Payload, TEXT("enabled"), true));
    }

    const FArrayProperty* BehaviorsProp = CastField<FArrayProperty>(SlotProp->Struct->FindPropertyByName(TEXT("BehaviorDefinitions")));
    Result->SetNumberField(TEXT("slotIndex"), SlotIndex);
    Result->SetNumberField(TEXT("behaviorCount"), BehaviorsProp ? FScriptArrayHelper(BehaviorsProp, BehaviorsProp->ContainerPtrToValuePtr<void>(Slot)).Num() : 0);
    Result->SetBoolField(TEXT("saved"), McpSafeAssetSave(Definition));
    McpHandlerUtils::AddVerification(Result, Definition);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Behavior configured"), Result);
    return true;
}

// Implements the "add_smart_object_component" action.
bool HandleAddSmartObjectComponent(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    FString DefinitionPath = GetJsonStringField(Payload, TEXT("definitionPath"), TEXT(""));
    FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"), TEXT("SmartObjectComponent"));

    if (BlueprintPath.IsEmpty())
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("blueprintPath is required"), TEXT("INVALID_PARAMS"));
        return true;
    }

    UClass* ComponentClass = FindSmartObjectClass(TEXT("SmartObjectComponent"));
    UClass* DefinitionClass = FindSmartObjectClass(TEXT("SmartObjectDefinition"));
    if (!ComponentClass || !DefinitionClass)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, SmartObjectsDisabledMessage, TEXT("SMARTOBJECTS_PLUGIN_NOT_ENABLED"));
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

    // A definition that did not load was dropped, leaving a component with nothing to offer.
    UObject* Definition = DefinitionPath.IsEmpty() ? nullptr : McpLoadAsset(DefinitionPath);
    if (!DefinitionPath.IsEmpty() && !(Definition && Definition->IsA(DefinitionClass)))
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Smart Object definition not found: %s. Nothing was added."), *DefinitionPath), TEXT("NOT_FOUND"));
        return true;
    }

    // Get the SCS
    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (!SCS)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint has no SimpleConstructionScript"), TEXT("INVALID_STATE"));
        return true;
    }
    // A taken name made CreateNode pick another while the reply named the one asked for.
    for (const USCS_Node* Node : SCS->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString().Equals(ComponentName, ESearchCase::IgnoreCase))
        {
            Self->SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Component '%s' already exists on %s; pick another "
                "componentName. Nothing was added."), *ComponentName, *Blueprint->GetName()), TEXT("ALREADY_EXISTS"));
            return true;
        }
    }

    // Create the component node using proper UE 5.7 SCS pattern
    USCS_Node* NewNode = SCS->CreateNode(ComponentClass, FName(*ComponentName));
    if (!NewNode || !NewNode->ComponentTemplate)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create SCS node for SmartObjectComponent"), TEXT("CREATION_FAILED"));
        return true;
    }

    // The node is not in the construction script yet, so a definition that did not take leaves the Blueprint as it was.
    if (Definition && !SetSmartObjectComponentDefinition(NewNode->ComponentTemplate, Definition))
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("The Smart Object component did not accept %s as its definition. Nothing was added."), *DefinitionPath), TEXT("SET_DEFINITION_FAILED"));
        return true;
    }

    // Add to SCS
    SCS->AddNode(NewNode);

    // Mark for compile and save
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    Result->SetStringField(TEXT("componentName"), NewNode->GetVariableName().ToString());
    Result->SetStringField(TEXT("blueprintPath"), NormalizedPath);
    if (Definition)
    {
        Result->SetStringField(TEXT("definitionPath"), Definition->GetPathName());
    }
    Result->SetBoolField(TEXT("saved"), McpSafeAssetSave(Blueprint));
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Component added"), Result);
    return true;
}
}
