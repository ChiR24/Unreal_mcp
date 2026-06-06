#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_CombatHandlersPrivate.h"

namespace McpCombatHandlers
{
#if WITH_EDITOR
bool FCombatActionContext::HandleMeleeCore() const
{
    if (SubAction == TEXT("create_melee_trace"))
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

        FString TraceStartSocket = GetStringFieldCombat(Payload, TEXT("meleeTraceStartSocket"), TEXT("WeaponBase"));
        FString TraceEndSocket = GetStringFieldCombat(Payload, TEXT("meleeTraceEndSocket"), TEXT("WeaponTip"));
        double TraceRadius = GetNumberFieldCombat(Payload, TEXT("meleeTraceRadius"), 10.0);

        // Add variables
        AddBlueprintVariableCombat(Blueprint, TEXT("MeleeTraceStartSocket"), MakeNamePinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("MeleeTraceEndSocket"), MakeNamePinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("MeleeTraceRadius"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("bIsTracing"), MakeBoolPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        // Set values
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FNameProperty* StartProp = FindFProperty<FNameProperty>(BPGC, TEXT("MeleeTraceStartSocket")))
                {
                    StartProp->SetPropertyValue_InContainer(CDO, FName(*TraceStartSocket));
                }
                if (FNameProperty* EndProp = FindFProperty<FNameProperty>(BPGC, TEXT("MeleeTraceEndSocket")))
                {
                    EndProp->SetPropertyValue_InContainer(CDO, FName(*TraceEndSocket));
                }
                if (FDoubleProperty* RadiusProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("MeleeTraceRadius")))
                {
                    RadiusProp->SetPropertyValue_InContainer(CDO, TraceRadius);
                }
                if (FBoolProperty* TracingProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bIsTracing")))
                {
                    TracingProp->SetPropertyValue_InContainer(CDO, false);
                }
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("traceStartSocket"), TraceStartSocket);
        Result->SetStringField(TEXT("traceEndSocket"), TraceEndSocket);
        Result->SetNumberField(TEXT("traceRadius"), TraceRadius);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Melee trace configured."), Result);
        return true;
    }

    // configure_combo_system

    if (SubAction == TEXT("configure_combo_system"))
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

        double ComboWindowTime = GetNumberFieldCombat(Payload, TEXT("comboWindowTime"), 0.5);
        int32 MaxComboCount = static_cast<int32>(GetNumberFieldCombat(Payload, TEXT("maxComboCount"), 3));

        // Add variables
        AddBlueprintVariableCombat(Blueprint, TEXT("ComboWindowTime"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("MaxComboCount"), MakeIntPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("CurrentComboIndex"), MakeIntPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("bInComboWindow"), MakeBoolPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        // Set values
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FDoubleProperty* WindowProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("ComboWindowTime")))
                {
                    WindowProp->SetPropertyValue_InContainer(CDO, ComboWindowTime);
                }
                if (FIntProperty* MaxProp = FindFProperty<FIntProperty>(BPGC, TEXT("MaxComboCount")))
                {
                    MaxProp->SetPropertyValue_InContainer(CDO, MaxComboCount);
                }
                if (FIntProperty* IndexProp = FindFProperty<FIntProperty>(BPGC, TEXT("CurrentComboIndex")))
                {
                    IndexProp->SetPropertyValue_InContainer(CDO, 0);
                }
                if (FBoolProperty* InWindowProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bInComboWindow")))
                {
                    InWindowProp->SetPropertyValue_InContainer(CDO, false);
                }
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetNumberField(TEXT("comboWindowTime"), ComboWindowTime);
        Result->SetNumberField(TEXT("maxComboCount"), MaxComboCount);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Combo system configured."), Result);
        return true;
    }

    // create_hit_pause (hitstop)

    if (SubAction == TEXT("create_hit_pause"))
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

        double HitPauseDuration = GetNumberFieldCombat(Payload, TEXT("hitPauseDuration"), 0.05);
        double TimeDilation = GetNumberFieldCombat(Payload, TEXT("hitPauseTimeDilation"), 0.1);

        // Add variables
        AddBlueprintVariableCombat(Blueprint, TEXT("HitPauseDuration"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("HitPauseTimeDilation"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("bEnableHitPause"), MakeBoolPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        // Set values
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FDoubleProperty* DurationProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("HitPauseDuration")))
                {
                    DurationProp->SetPropertyValue_InContainer(CDO, HitPauseDuration);
                }
                if (FDoubleProperty* DilationProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("HitPauseTimeDilation")))
                {
                    DilationProp->SetPropertyValue_InContainer(CDO, TimeDilation);
                }
                if (FBoolProperty* EnableProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bEnableHitPause")))
                {
                    EnableProp->SetPropertyValue_InContainer(CDO, true);
                }
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetNumberField(TEXT("hitPauseDuration"), HitPauseDuration);
        Result->SetNumberField(TEXT("timeDilation"), TimeDilation);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Hit pause (hitstop) configured."), Result);
        return true;
    }

    // configure_hit_reaction

    return false;
}
#endif
}
