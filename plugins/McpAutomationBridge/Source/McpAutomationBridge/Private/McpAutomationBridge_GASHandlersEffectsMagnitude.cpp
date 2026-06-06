#include "McpAutomationBridge_GASBlueprintCreation.h"
#include "McpAutomationBridge_GASPayloadFields.h"
#include "McpAutomationBridge_GASRequestContext.h"
#include "McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR && MCP_HAS_GAS
#include "Engine/Blueprint.h"
#include "GameplayEffect.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

#if WITH_EDITOR && MCP_HAS_GAS
namespace McpGASHandlers
{
bool HandleGASEffectsMagnitude(const FGASRequestContext& Context, const FString& SubAction)
{
    UMcpAutomationBridgeSubsystem* Bridge = Context.Subsystem;
    const FString& RequestId = Context.RequestId;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    const FString& Name = Context.Name;
    const FString& Path = Context.Path;
    const FString& BlueprintPath = Context.BlueprintPath;
    const FString& AssetPath = Context.AssetPath;

    if (SubAction == TEXT("create_gameplay_effect"))
    {
        if (Name.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing name."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString Error;
        bool bReusedExisting = false;
        UBlueprint* Blueprint = CreateGASBlueprint(Path, Name, UGameplayEffect::StaticClass(), Error, bReusedExisting);
        if (!Blueprint)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
            return true;
        }

        FString DurationType = GetStringFieldGAS(Payload, TEXT("durationType"), TEXT("Instant"));
        const FString DurationTypeToken = NormalizeGASToken(DurationType);

        // Only set duration policy on CDO if we created a new blueprint
        if (!bReusedExisting)
        {
            // Set duration policy on CDO
            if (Blueprint->GeneratedClass)
            {
                UGameplayEffect* EffectCDO = Cast<UGameplayEffect>(Blueprint->GeneratedClass->GetDefaultObject());
                if (EffectCDO)
                {
                    if (DurationTypeToken == TEXT("instant"))
                    {
                        EffectCDO->DurationPolicy = EGameplayEffectDurationType::Instant;
                    }
                    else if (DurationTypeToken == TEXT("infinite"))
                    {
                        EffectCDO->DurationPolicy = EGameplayEffectDurationType::Infinite;
                    }
                    else if (DurationTypeToken == TEXT("hasduration"))
                    {
                        EffectCDO->DurationPolicy = EGameplayEffectDurationType::HasDuration;
                    }
                }
            }

            McpSafeAssetSave(Blueprint);
        }

        // Use the actual blueprint name (which may have been sanitized) in the response
        FString ActualName = Blueprint->GetName();

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("assetPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("name"), ActualName);
        Result->SetStringField(TEXT("parentClass"), TEXT("GameplayEffect"));
        Result->SetStringField(TEXT("durationType"), DurationType);
        Result->SetBoolField(TEXT("reusedExisting"), bReusedExisting);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true,
            bReusedExisting ? TEXT("Effect already exists") : TEXT("Effect created"), Result);
        return true;
    }

    // set_effect_duration
    if (SubAction == TEXT("set_effect_duration"))
    {
        if (BlueprintPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        UGameplayEffect* EffectCDO = Cast<UGameplayEffect>(Blueprint->GeneratedClass->GetDefaultObject());
        if (!EffectCDO)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Not a GameplayEffect blueprint"), TEXT("INVALID_TYPE"));
            return true;
        }

        FString DurationType = GetStringFieldGAS(Payload, TEXT("durationType"), TEXT("Instant"));
        const FString DurationTypeToken = NormalizeGASToken(DurationType);
        float Duration = static_cast<float>(GetNumberFieldGAS(Payload, TEXT("duration"), 0.0));

        if (DurationTypeToken == TEXT("instant"))
        {
            EffectCDO->DurationPolicy = EGameplayEffectDurationType::Instant;
        }
        else if (DurationTypeToken == TEXT("infinite"))
        {
            EffectCDO->DurationPolicy = EGameplayEffectDurationType::Infinite;
        }
        else if (DurationTypeToken == TEXT("hasduration"))
        {
            EffectCDO->DurationPolicy = EGameplayEffectDurationType::HasDuration;
            // Note: SetValue doesn't exist in UE 5.6, FScalableFloat constructor used in 5.7+
            // Use assignment with FGameplayEffectModifierMagnitude constructor
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
            EffectCDO->DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Duration));
#else
            // UE 5.6: Assign FScalableFloat directly to the magnitude
            EffectCDO->DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Duration));
#endif
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("durationType"), DurationType);
        Result->SetNumberField(TEXT("duration"), Duration);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Duration set"), Result);
        return true;
    }

    // add_effect_modifier
    if (SubAction == TEXT("add_effect_modifier"))
    {
        if (BlueprintPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        UGameplayEffect* EffectCDO = Cast<UGameplayEffect>(Blueprint->GeneratedClass->GetDefaultObject());
        if (!EffectCDO)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Not a GameplayEffect blueprint"), TEXT("INVALID_TYPE"));
            return true;
        }

        FString Operation = GetGASStringFieldWithFallback(Payload, TEXT("operation"), TEXT("modifierOperation"), TEXT("Add"));
        const FString OperationToken = NormalizeGASToken(Operation);
        float Magnitude = static_cast<float>(GetGASNumberFieldWithFallback(Payload, TEXT("magnitude"), TEXT("modifierMagnitude"), 0.0));

        FGameplayModifierInfo Modifier;

        if (OperationToken == TEXT("additive") || OperationToken == TEXT("add"))
        {
            Modifier.ModifierOp = EGameplayModOp::Additive;
        }
        else if (OperationToken == TEXT("multiplicative") || OperationToken == TEXT("multiply"))
        {
            Modifier.ModifierOp = EGameplayModOp::Multiplicitive;
        }
        else if (OperationToken == TEXT("division") || OperationToken == TEXT("divide"))
        {
            Modifier.ModifierOp = EGameplayModOp::Division;
        }
        else if (OperationToken == TEXT("override"))
        {
            Modifier.ModifierOp = EGameplayModOp::Override;
        }

        // Note: SetValue doesn't exist in UE 5.6. Use FScalableFloat constructor.
        Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Magnitude));
        EffectCDO->Modifiers.Add(Modifier);

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("operation"), Operation);
        Result->SetNumberField(TEXT("magnitude"), Magnitude);
        Result->SetNumberField(TEXT("modifierCount"), EffectCDO->Modifiers.Num());
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Modifier added"), Result);
        return true;
    }

    // set_modifier_magnitude
    if (SubAction == TEXT("set_modifier_magnitude"))
    {
        if (BlueprintPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        UGameplayEffect* EffectCDO = Cast<UGameplayEffect>(Blueprint->GeneratedClass->GetDefaultObject());
        if (!EffectCDO)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Not a GameplayEffect blueprint"), TEXT("INVALID_TYPE"));
            return true;
        }

        int32 ModifierIndex = static_cast<int32>(GetNumberFieldGAS(Payload, TEXT("modifierIndex"), 0));
        float Value = static_cast<float>(GetGASNumberFieldWithFallback(Payload, TEXT("value"), TEXT("modifierMagnitude"), 0.0));
        FString MagnitudeType = GetGASStringFieldWithFallback(Payload, TEXT("magnitudeType"), TEXT("magnitudeCalculationType"), TEXT("ScalableFloat"));

        if (ModifierIndex >= EffectCDO->Modifiers.Num())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Modifier index out of range"), TEXT("INVALID_INDEX"));
            return true;
        }

        // Note: SetValue doesn't exist in UE 5.6. Use FScalableFloat constructor.
        EffectCDO->Modifiers[ModifierIndex].ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Value));

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("modifierIndex"), ModifierIndex);
        Result->SetStringField(TEXT("magnitudeType"), MagnitudeType);
        Result->SetNumberField(TEXT("value"), Value);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Magnitude set"), Result);
        return true;
    }

    // add_effect_execution_calculation - REAL IMPLEMENTATION

    return false;
}
}
#endif
