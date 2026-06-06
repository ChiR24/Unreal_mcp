#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_CombatHandlersPrivate.h"

namespace McpCombatHandlers
{
#if WITH_EDITOR
bool FCombatActionContext::HandleDamageExecution() const
{
    if (SubAction == TEXT("configure_damage_execution"))
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

        double DamageImpulse = GetNumberFieldCombat(Payload, TEXT("damageImpulse"), 500.0);
        double CriticalMultiplier = GetNumberFieldCombat(Payload, TEXT("criticalMultiplier"), 2.0);
        double HeadshotMultiplier = GetNumberFieldCombat(Payload, TEXT("headshotMultiplier"), 2.5);

        // Add damage-related variables
        AddBlueprintVariableCombat(Blueprint, TEXT("DamageImpulse"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("CriticalMultiplier"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("HeadshotMultiplier"), MakeFloatPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        // Set values
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FDoubleProperty* ImpulseProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("DamageImpulse")))
                {
                    ImpulseProp->SetPropertyValue_InContainer(CDO, DamageImpulse);
                }
                if (FDoubleProperty* CritProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("CriticalMultiplier")))
                {
                    CritProp->SetPropertyValue_InContainer(CDO, CriticalMultiplier);
                }
                if (FDoubleProperty* HeadProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("HeadshotMultiplier")))
                {
                    HeadProp->SetPropertyValue_InContainer(CDO, HeadshotMultiplier);
                }
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetNumberField(TEXT("damageImpulse"), DamageImpulse);
        Result->SetNumberField(TEXT("criticalMultiplier"), CriticalMultiplier);
        Result->SetNumberField(TEXT("headshotMultiplier"), HeadshotMultiplier);

        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Damage execution configured."), Result);
        return true;
    }

    // setup_hitbox_component

    if (SubAction == TEXT("setup_hitbox_component"))
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

        FString HitboxType = GetStringFieldCombat(Payload, TEXT("hitboxType"), TEXT("Capsule"));
        FString BoneName = GetStringFieldCombat(Payload, TEXT("hitboxBoneName"), TEXT(""));
        bool bIsDamageZoneHead = GetBoolFieldCombat(Payload, TEXT("isDamageZoneHead"), false);
        double DamageMultiplier = GetNumberFieldCombat(Payload, TEXT("damageMultiplier"), 1.0);
        TSharedPtr<FJsonObject> AppliedHitboxSize = MakeShared<FJsonObject>();

        // Create appropriate collision component based on type
        if (HitboxType == TEXT("Capsule"))
        {
            UCapsuleComponent* Hitbox = GetOrCreateSCSComponent<UCapsuleComponent>(Blueprint, TEXT("HitboxCapsule"));
            if (Hitbox)
            {
                auto HitboxSizeObj = Payload->GetObjectField(TEXT("hitboxSize"));
                if (HitboxSizeObj.IsValid())
                {
                    double Radius = GetNumberFieldCombat(HitboxSizeObj, TEXT("radius"), 34.0);
                    double HalfHeight = GetNumberFieldCombat(HitboxSizeObj, TEXT("halfHeight"), 88.0);
                    Hitbox->SetCapsuleRadius(static_cast<float>(Radius));
                    Hitbox->SetCapsuleHalfHeight(static_cast<float>(HalfHeight));
                    AppliedHitboxSize->SetNumberField(TEXT("radius"), Radius);
                    AppliedHitboxSize->SetNumberField(TEXT("halfHeight"), HalfHeight);
                }
            }
        }
        else if (HitboxType == TEXT("Box"))
        {
            UBoxComponent* Hitbox = GetOrCreateSCSComponent<UBoxComponent>(Blueprint, TEXT("HitboxBox"));
            if (Hitbox)
            {
                auto HitboxSizeObj = Payload->GetObjectField(TEXT("hitboxSize"));
                if (HitboxSizeObj.IsValid())
                {
                    auto ExtentObj = HitboxSizeObj->GetObjectField(TEXT("extent"));
                    if (ExtentObj.IsValid())
                    {
                        FVector Extent = GetVectorFromJsonCombat(ExtentObj);
                        Hitbox->SetBoxExtent(Extent);
                        TSharedPtr<FJsonObject> ExtentResult = MakeShared<FJsonObject>();
                        ExtentResult->SetNumberField(TEXT("x"), Extent.X);
                        ExtentResult->SetNumberField(TEXT("y"), Extent.Y);
                        ExtentResult->SetNumberField(TEXT("z"), Extent.Z);
                        AppliedHitboxSize->SetObjectField(TEXT("extent"), ExtentResult);
                    }
                }
            }
        }
        else if (HitboxType == TEXT("Sphere"))
        {
            USphereComponent* Hitbox = GetOrCreateSCSComponent<USphereComponent>(Blueprint, TEXT("HitboxSphere"));
            if (Hitbox)
            {
                auto HitboxSizeObj = Payload->GetObjectField(TEXT("hitboxSize"));
                if (HitboxSizeObj.IsValid())
                {
                    double Radius = GetNumberFieldCombat(HitboxSizeObj, TEXT("radius"), 50.0);
                    Hitbox->SetSphereRadius(static_cast<float>(Radius));
                    AppliedHitboxSize->SetNumberField(TEXT("radius"), Radius);
                }
            }
        }

        // Add hitbox metadata variables
        AddBlueprintVariableCombat(Blueprint, TEXT("bIsHeadshotZone"), MakeBoolPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("HitboxDamageMultiplier"), MakeFloatPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        // Set values
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FBoolProperty* HeadProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bIsHeadshotZone")))
                {
                    HeadProp->SetPropertyValue_InContainer(CDO, bIsDamageZoneHead);
                }
                if (FDoubleProperty* MultProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("HitboxDamageMultiplier")))
                {
                    MultProp->SetPropertyValue_InContainer(CDO, DamageMultiplier);
                }
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("hitboxType"), HitboxType);
        Result->SetObjectField(TEXT("hitboxSize"), AppliedHitboxSize);
        Result->SetBoolField(TEXT("isDamageZoneHead"), bIsDamageZoneHead);
        Result->SetNumberField(TEXT("damageMultiplier"), DamageMultiplier);

        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Hitbox component configured."), Result);
        return true;
    }

    // ============================================================
    // 15.5 WEAPON FEATURES
    // ============================================================

    // setup_reload_system

    if (SubAction == TEXT("configure_hit_detection"))
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

        FString HitboxType = GetStringFieldCombat(Payload, TEXT("hitboxType"), TEXT("Capsule"));
        double DamageMultiplier = GetNumberFieldCombat(Payload, TEXT("damageMultiplier"), 1.0);

        // Create collision component based on type
        if (HitboxType == TEXT("Capsule"))
        {
            GetOrCreateSCSComponent<UCapsuleComponent>(Blueprint, TEXT("HitboxCapsule"));
        }
        else if (HitboxType == TEXT("Box"))
        {
            GetOrCreateSCSComponent<UBoxComponent>(Blueprint, TEXT("HitboxBox"));
        }
        else
        {
            GetOrCreateSCSComponent<USphereComponent>(Blueprint, TEXT("HitboxSphere"));
        }

        AddBlueprintVariableCombat(Blueprint, TEXT("HitboxDamageMultiplier"), MakeFloatPinType());
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("hitboxType"), HitboxType);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Hit detection configured."), Result);
        return true;
    }

    // get_combat_stats -> alias for get_combat_info

    return false;
}
#endif
}
