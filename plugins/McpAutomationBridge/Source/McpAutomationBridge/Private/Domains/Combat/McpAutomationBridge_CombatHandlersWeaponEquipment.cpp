#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Combat/McpAutomationBridge_CombatHandlersPrivate.h"

namespace McpCombatHandlers
{
bool FCombatActionContext::HandleWeaponEquipment() const
{
    if (SubAction == TEXT("setup_attachment_system"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found."), TEXT("NOT_FOUND"));
            return true;
        }

        // Parse attachment slots and create actual SceneComponent attach points
        const TArray<TSharedPtr<FJsonValue>>* AttachmentSlotsArray;
        TArray<FString> SlotNames;
        TArray<FString> CreatedComponents;
        TArray<TSharedPtr<FJsonValue>> SlotObjects;

        if (Payload->TryGetArrayField(TEXT("attachmentSlots"), AttachmentSlotsArray))
        {
            USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
            if (SCS)
            {
                for (const auto& SlotValue : *AttachmentSlotsArray)
                {
                    if (SlotValue->Type == EJson::Object)
                    {
                        auto SlotObj = SlotValue->AsObject();
                        FString SlotName = GetJsonStringField(SlotObj, TEXT("slotName"));
                        FString SlotType = GetJsonStringField(SlotObj, TEXT("slotType"), TEXT("Optic"));

                        if (!SlotName.IsEmpty())
                        {
                            SlotNames.Add(SlotName);
                            TSharedPtr<FJsonObject> SlotOut = MakeShared<FJsonObject>();
                            SlotOut->SetStringField(TEXT("slotName"), SlotName);
                            SlotOut->SetStringField(TEXT("socketName"), GetJsonStringField(SlotObj, TEXT("socketName"), SlotName + TEXT("Socket")));
                            const TArray<TSharedPtr<FJsonValue>>* AllowedTypes = nullptr;
                            TArray<TSharedPtr<FJsonValue>> AllowedOut;
                            if (SlotObj->TryGetArrayField(TEXT("allowedTypes"), AllowedTypes) && AllowedTypes)
                            {
                                AllowedOut = *AllowedTypes;
                            }
                            else
                            {
                                AllowedOut.Add(MakeShared<FJsonValueString>(SlotType));
                            }
                            SlotOut->SetArrayField(TEXT("allowedTypes"), AllowedOut);
                            SlotObjects.Add(MakeShared<FJsonValueObject>(SlotOut));

                            FString ComponentName = FString::Printf(TEXT("AttachPoint_%s"), *SlotName);
                            USceneComponent* AttachPoint = GetOrCreateSCSComponent<USceneComponent>(Blueprint, ComponentName, TEXT("WeaponMesh"));
                            if (AttachPoint)
                            {
                                CreatedComponents.Add(ComponentName);
                            }
                        }
                    }
                    else if (SlotValue->Type == EJson::String)
                    {
                        // Simple string slot name
                        FString SlotName = SlotValue->AsString();
                        if (!SlotName.IsEmpty())
                        {
                            SlotNames.Add(SlotName);
                            TSharedPtr<FJsonObject> SlotOut = MakeShared<FJsonObject>();
                            SlotOut->SetStringField(TEXT("slotName"), SlotName);
                            SlotOut->SetStringField(TEXT("socketName"), SlotName + TEXT("Socket"));
                            SlotOut->SetArrayField(TEXT("allowedTypes"), TArray<TSharedPtr<FJsonValue>>());
                            SlotObjects.Add(MakeShared<FJsonValueObject>(SlotOut));

                            FString ComponentName = FString::Printf(TEXT("AttachPoint_%s"), *SlotName);
                            USceneComponent* AttachPoint = GetOrCreateSCSComponent<USceneComponent>(Blueprint, ComponentName, TEXT("WeaponMesh"));
                            if (AttachPoint)
                            {
                                CreatedComponents.Add(ComponentName);
                            }
                        }
                    }
                }
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);
        if (!McpSafeAssetSave(Blueprint))
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("%s was changed in the editor but could not be saved to disk."), *Blueprint->GetPathName()), TEXT("SAVE_FAILED"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        // Echo slot objects in the contract's shape (slotName/socketName/allowedTypes);
        // bare strings failed the output schema after the Blueprint had been changed.
        Result->SetArrayField(TEXT("attachmentSlots"), SlotObjects);

        TArray<TSharedPtr<FJsonValue>> ComponentsJsonArray;
        for (const FString& Comp : CreatedComponents)
        {
            ComponentsJsonArray.Add(MakeShared<FJsonValueString>(Comp));
        }
        Result->SetArrayField(TEXT("componentsCreated"), ComponentsJsonArray);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Attachment system configured with SceneComponent attach points."), Result);
        return true;
    }
    return false;
}
}
