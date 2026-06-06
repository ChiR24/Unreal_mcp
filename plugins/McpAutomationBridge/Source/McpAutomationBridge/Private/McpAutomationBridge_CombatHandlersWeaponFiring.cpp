#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_CombatHandlersPrivate.h"

namespace McpCombatHandlers
{
#if WITH_EDITOR
bool FCombatActionContext::HandleWeaponFiring() const
{
    if (SubAction == TEXT("configure_hitscan"))
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

        bool bHitscanEnabled = GetBoolFieldCombat(Payload, TEXT("hitscanEnabled"), true);
        FString TraceChannel = GetStringFieldCombat(Payload, TEXT("traceChannel"), TEXT("Visibility"));
        double Range = GetNumberFieldCombat(Payload, TEXT("range"), 10000.0);

        // Add variables
        AddBlueprintVariableCombat(Blueprint, TEXT("bIsHitscan"), MakeBoolPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("TraceChannel"), MakeNamePinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("HitscanRange"), MakeFloatPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        // Set values
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FBoolProperty* HitscanProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bIsHitscan")))
                {
                    HitscanProp->SetPropertyValue_InContainer(CDO, bHitscanEnabled);
                }
                if (FNameProperty* ChannelProp = FindFProperty<FNameProperty>(BPGC, TEXT("TraceChannel")))
                {
                    ChannelProp->SetPropertyValue_InContainer(CDO, FName(*TraceChannel));
                }
                if (FDoubleProperty* RangeProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("HitscanRange")))
                {
                    RangeProp->SetPropertyValue_InContainer(CDO, Range);
                }
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetBoolField(TEXT("hitscanEnabled"), bHitscanEnabled);
        Result->SetStringField(TEXT("traceChannel"), TraceChannel);
        Result->SetNumberField(TEXT("range"), Range);

        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Hitscan configured."), Result);
        return true;
    }

    // configure_projectile

    if (SubAction == TEXT("configure_projectile"))
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

        FString ProjectileClass = GetStringFieldCombat(Payload, TEXT("projectileClass"));
        double ProjectileSpeed = GetNumberFieldCombat(Payload, TEXT("projectileSpeed"), 5000.0);

        // Add variables
        AddBlueprintVariableCombat(Blueprint, TEXT("ProjectileClassPath"), MakeStringPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("ProjectileSpeed"), MakeFloatPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        // Set values
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FStrProperty* ClassProp = FindFProperty<FStrProperty>(BPGC, TEXT("ProjectileClassPath")))
                {
                    ClassProp->SetPropertyValue_InContainer(CDO, ProjectileClass);
                }
                if (FDoubleProperty* SpeedProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("ProjectileSpeed")))
                {
                    SpeedProp->SetPropertyValue_InContainer(CDO, ProjectileSpeed);
                }
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("projectileClass"), ProjectileClass);
        Result->SetNumberField(TEXT("projectileSpeed"), ProjectileSpeed);

        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Projectile firing configured."), Result);
        return true;
    }

    // configure_spread_pattern

    if (SubAction == TEXT("configure_spread_pattern"))
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

        FString PatternType = GetStringFieldCombat(Payload, TEXT("spreadPattern"), TEXT("Random"));
        double SpreadIncrease = GetNumberFieldCombat(Payload, TEXT("spreadIncrease"), 0.5);
        double SpreadRecovery = GetNumberFieldCombat(Payload, TEXT("spreadRecovery"), 2.0);

        // Add variables
        AddBlueprintVariableCombat(Blueprint, TEXT("SpreadPatternType"), MakeStringPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("SpreadIncreasePerShot"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("SpreadRecoveryRate"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("CurrentSpread"), MakeFloatPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        // Set values
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FStrProperty* PatternProp = FindFProperty<FStrProperty>(BPGC, TEXT("SpreadPatternType")))
                {
                    PatternProp->SetPropertyValue_InContainer(CDO, PatternType);
                }
                if (FDoubleProperty* IncreaseProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("SpreadIncreasePerShot")))
                {
                    IncreaseProp->SetPropertyValue_InContainer(CDO, SpreadIncrease);
                }
                if (FDoubleProperty* RecoveryProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("SpreadRecoveryRate")))
                {
                    RecoveryProp->SetPropertyValue_InContainer(CDO, SpreadRecovery);
                }
                if (FDoubleProperty* CurrentProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("CurrentSpread")))
                {
                    CurrentProp->SetPropertyValue_InContainer(CDO, 0.0);
                }
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("patternType"), PatternType);
        Result->SetNumberField(TEXT("spreadIncrease"), SpreadIncrease);
        Result->SetNumberField(TEXT("spreadRecovery"), SpreadRecovery);

        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Spread pattern configured."), Result);
        return true;
    }

    // configure_recoil_pattern

    return false;
}
#endif
}
