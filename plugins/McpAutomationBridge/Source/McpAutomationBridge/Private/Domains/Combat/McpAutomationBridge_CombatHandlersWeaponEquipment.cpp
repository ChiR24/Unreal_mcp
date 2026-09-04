#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Combat/McpAutomationBridge_CombatHandlersPrivate.h"

namespace McpCombatHandlers
{
#if WITH_EDITOR
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
        McpSafeAssetSave(Blueprint);

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
    if (SubAction == TEXT("setup_weapon_switching"))
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

        double SwitchInTime = GetJsonNumberField(Payload, TEXT("switchInTime"), 0.3);
        double SwitchOutTime = GetJsonNumberField(Payload, TEXT("switchOutTime"), 0.2);
        FString EquipAnimPath = GetJsonStringField(Payload, TEXT("equipAnimationPath"));
        FString UnequipAnimPath = GetJsonStringField(Payload, TEXT("unequipAnimationPath"));

        AddBlueprintVariableCombat(Blueprint, TEXT("SwitchInTime"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("SwitchOutTime"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("bIsSwitching"), MakeBoolPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("bIsEquipped"), MakeBoolPinType());

        // Add animation references if provided
        bool bEquipAnimLoaded = false;
        bool bUnequipAnimLoaded = false;
        if (!EquipAnimPath.IsEmpty())
        {
            UAnimMontage* EquipAnim = LoadObject<UAnimMontage>(nullptr, *EquipAnimPath);
            if (EquipAnim)
            {
                AddBlueprintVariableCombat(Blueprint, TEXT("EquipAnimation"), MakeObjectPinType(UAnimMontage::StaticClass()));
                bEquipAnimLoaded = true;
            }
        }
        if (!UnequipAnimPath.IsEmpty())
        {
            UAnimMontage* UnequipAnim = LoadObject<UAnimMontage>(nullptr, *UnequipAnimPath);
            if (UnequipAnim)
            {
                AddBlueprintVariableCombat(Blueprint, TEXT("UnequipAnimation"), MakeObjectPinType(UAnimMontage::StaticClass()));
                bUnequipAnimLoaded = true;
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FDoubleProperty* InProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("SwitchInTime")))
                {
                    InProp->SetPropertyValue_InContainer(CDO, SwitchInTime);
                }
                if (FDoubleProperty* OutProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("SwitchOutTime")))
                {
                    OutProp->SetPropertyValue_InContainer(CDO, SwitchOutTime);
                }
                if (FBoolProperty* SwitchingProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bIsSwitching")))
                {
                    SwitchingProp->SetPropertyValue_InContainer(CDO, false);
                }
                if (FBoolProperty* EquippedProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bIsEquipped")))
                {
                    EquippedProp->SetPropertyValue_InContainer(CDO, false);
                }
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetNumberField(TEXT("switchInTime"), SwitchInTime);
        Result->SetNumberField(TEXT("switchOutTime"), SwitchOutTime);
        Result->SetStringField(TEXT("equipAnimationPath"), EquipAnimPath);
        Result->SetStringField(TEXT("unequipAnimationPath"), UnequipAnimPath);
        Result->SetBoolField(TEXT("equipAnimationLoaded"), bEquipAnimLoaded);
        Result->SetBoolField(TEXT("unequipAnimationLoaded"), bUnequipAnimLoaded);

        TArray<TSharedPtr<FJsonValue>> VarsAdded;
        VarsAdded.Add(MakeShared<FJsonValueString>(TEXT("SwitchInTime")));
        VarsAdded.Add(MakeShared<FJsonValueString>(TEXT("SwitchOutTime")));
        VarsAdded.Add(MakeShared<FJsonValueString>(TEXT("bIsSwitching")));
        VarsAdded.Add(MakeShared<FJsonValueString>(TEXT("bIsEquipped")));
        if (bEquipAnimLoaded) VarsAdded.Add(MakeShared<FJsonValueString>(TEXT("EquipAnimation")));
        if (bUnequipAnimLoaded) VarsAdded.Add(MakeShared<FJsonValueString>(TEXT("UnequipAnimation")));
        Result->SetArrayField(TEXT("variablesAdded"), VarsAdded);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Weapon switching configured with Blueprint variables."), Result);
        return true;
    }

    // ============================================================
    // 15.6 EFFECTS
    // ============================================================

    // configure_muzzle_flash

    return false;
}
#endif
}
