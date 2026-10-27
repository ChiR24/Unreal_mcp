#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Combat/McpCombatPrivate.h"

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

        // Every slot is read before anything changes. A call with no slot, or only nameless ones, used to compile,
        // save and answer success with nothing made; socketName and allowedTypes were echoed and applied nowhere.
        struct FSlot
        {
            FString Name;
            FString Socket;
            TArray<FString> Types;
        };
        TArray<FSlot> Slots;
        const TArray<TSharedPtr<FJsonValue>>* AttachmentSlotsArray = nullptr;
        if (Payload->TryGetArrayField(TEXT("attachmentSlots"), AttachmentSlotsArray) && AttachmentSlotsArray)
        {
            for (int32 Index = 0; Index < AttachmentSlotsArray->Num(); ++Index)
            {
                const TSharedPtr<FJsonValue>& SlotValue = (*AttachmentSlotsArray)[Index];
                FSlot Slot;
                if (SlotValue.IsValid() && SlotValue->Type == EJson::Object)
                {
                    const TSharedPtr<FJsonObject> SlotObj = SlotValue->AsObject();
                    Slot.Name = GetJsonStringField(SlotObj, TEXT("slotName")).TrimStartAndEnd();
                    Slot.Socket = GetJsonStringField(SlotObj, TEXT("socketName")).TrimStartAndEnd();
                    const TArray<TSharedPtr<FJsonValue>>* Types = nullptr;
                    if (SlotObj->TryGetArrayField(TEXT("allowedTypes"), Types) && Types)
                    {
                        for (const TSharedPtr<FJsonValue>& Type : *Types)
                        {
                            Slot.Types.Add(Type->AsString());
                        }
                    }
                }
                else if (SlotValue.IsValid() && SlotValue->Type == EJson::String)
                {
                    Slot.Name = SlotValue->AsString().TrimStartAndEnd();
                }
                if (Slot.Name.IsEmpty())
                {
                    SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("attachmentSlots[%d] has no slotName; nothing was changed."), Index), TEXT("INVALID_ARGUMENT"));
                    return true;
                }
                Slots.Add(Slot);
            }
        }
        USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
        if (Slots.Num() == 0 || !SCS)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("attachmentSlots needs at least one slot, each {slotName, socketName, allowedTypes}; nothing was changed."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        const USCS_Node* MeshNode = SCS->FindSCSNode(TEXT("WeaponMesh"));
        const UStaticMeshComponent* MeshTemplate = MeshNode ? Cast<UStaticMeshComponent>(MeshNode->ComponentTemplate) : nullptr;
        const UStaticMesh* WeaponMeshAsset = MeshTemplate ? MeshTemplate->GetStaticMesh() : nullptr;
        TArray<FString> CreatedComponents;
        TArray<TSharedPtr<FJsonValue>> SlotObjects;
        for (const FSlot& Slot : Slots)
        {
            // An attach point under WeaponMesh, on socketName when one is given. allowedTypes are added to its component
            // tags, which the game reads with ComponentHasTag or GetComponentsByTag.
            const FString ComponentName = FString::Printf(TEXT("AttachPoint_%s"), *Slot.Name);
            USceneComponent* AttachPoint = GetOrCreateSCSComponent<USceneComponent>(Blueprint, ComponentName, TEXT("WeaponMesh"));
            USCS_Node* PointNode = SCS->FindSCSNode(FName(*ComponentName));
            if (!AttachPoint || !PointNode)
            {
                continue;
            }
            if (!Slot.Socket.IsEmpty())
            {
                PointNode->AttachToName = FName(*Slot.Socket);
            }
            TArray<TSharedPtr<FJsonValue>> TypesOut;
            for (const FString& Type : Slot.Types)
            {
                AttachPoint->ComponentTags.AddUnique(FName(*Type));
                TypesOut.Add(MakeShared<FJsonValueString>(Type));
            }
            CreatedComponents.Add(ComponentName);
            TSharedPtr<FJsonObject> SlotOut = MakeShared<FJsonObject>();
            SlotOut->SetStringField(TEXT("slotName"), Slot.Name);
            SlotOut->SetStringField(TEXT("socketName"), Slot.Socket);
            if (!Slot.Socket.IsEmpty())
            {
                SlotOut->SetBoolField(TEXT("socketFound"), WeaponMeshAsset && WeaponMeshAsset->FindSocket(FName(*Slot.Socket)));
            }
            SlotOut->SetArrayField(TEXT("allowedTypes"), TypesOut);
            SlotObjects.Add(MakeShared<FJsonValueObject>(SlotOut));
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

        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Attachment system configured with SceneComponent attach points."), Result);
        return true;
    }
    return false;
}
}
