#include "Domains/GAS/McpAutomationBridge_GASAbilityReflection.h"
#include "Domains/GAS/McpAutomationBridge_GASPayloadFields.h"
#include "Domains/GAS/McpAutomationBridge_GASRequestContext.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Safety/McpSafeOperations.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Engine/Blueprint.h"
#include "GameplayEffect.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace McpGASHandlers
{
bool HandleGASModifierMagnitude(const FGASRequestContext& Context, const FString& SubAction)
{
    UMcpAutomationBridgeSubsystem* Bridge = Context.Subsystem;
    const FString& RequestId = Context.RequestId;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    const FString& BlueprintPath = Context.BlueprintPath;

    // set_modifier_magnitude
    if (SubAction == TEXT("set_modifier_magnitude"))
    {
        if (BlueprintPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = nullptr;
        UGameplayEffect* EffectCDO = LoadGASBlueprintCDO<UGameplayEffect>(Context, Blueprint, TEXT("GameplayEffect"));
        if (!EffectCDO)
        {
            return true;
        }

        int32 ModifierIndex = static_cast<int32>(GetJsonNumberField(Payload, TEXT("modifierIndex"), 0));
        float Value = static_cast<float>(GetGASNumberFieldWithFallback(Payload, TEXT("modifierMagnitude"), TEXT("value"), 0.0));
        FString MagnitudeType = GetGASStringFieldWithFallback(Payload, TEXT("magnitudeCalculationType"), TEXT("magnitudeType"), TEXT("ScalableFloat"));

        if (ModifierIndex < 0 || ModifierIndex >= EffectCDO->Modifiers.Num())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Modifier index out of range"), TEXT("INVALID_INDEX"));
            return true;
        }

        // ScalableFloat (default) is the value; SetByCaller is keyed by setByCallerTag - a registered tag
        // as the DataTag, anything else as the DataName (both are how a spec supplies the value later).
        const FString MagnitudeToken = NormalizeGASToken(MagnitudeType);
        const bool bSetByCaller = MagnitudeToken == TEXT("setbycaller");
        const FString SetByCallerKey = GetJsonStringField(Payload, TEXT("setByCallerTag"));
        if (MagnitudeToken != TEXT("scalablefloat") && !bSetByCaller)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("magnitudeCalculationType '%s' is not settable here; use ScalableFloat or SetByCaller. Nothing was changed."), *MagnitudeType),
                TEXT("UNSUPPORTED_MAGNITUDE_TYPE"));
            return true;
        }
        if (bSetByCaller && SetByCallerKey.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                TEXT("SetByCaller needs setByCallerTag (the key the effect spec supplies the value under)."), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        FSetByCallerFloat SetByCaller;
        SetByCaller.DataTag = GetOrRequestTag(SetByCallerKey);
        SetByCaller.DataName = SetByCaller.DataTag.IsValid() ? NAME_None : FName(*SetByCallerKey);
        EffectCDO->Modifiers[ModifierIndex].ModifierMagnitude = bSetByCaller
            ? FGameplayEffectModifierMagnitude(SetByCaller)
            : FGameplayEffectModifierMagnitude(FScalableFloat(Value));

        // Same persistence gap as its siblings: the CDO was edited in memory and the change was never
        // compiled or written, so it survived only until the editor closed. And like its siblings the
        // result is read back from the RECOMPILED CDO before success is reported -- this was the one
        // fixed handler without a read-back, which made the change set's own "every mutation verifies"
        // record false.
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        const bool bCompiled = McpSafeCompileBlueprint(Blueprint);

        bool bMagnitudeVerified = false;
        float VerifiedValue = 0.0f;
        if (UClass* CompiledClass = Blueprint->GeneratedClass)
        {
            if (UGameplayEffect* CompiledCDO = Cast<UGameplayEffect>(CompiledClass->GetDefaultObject()))
            {
                if (CompiledCDO->Modifiers.IsValidIndex(ModifierIndex))
                {
                    const FGameplayEffectModifierMagnitude& Stored = CompiledCDO->Modifiers[ModifierIndex].ModifierMagnitude;
                    bMagnitudeVerified = bSetByCaller
                        ? Stored.GetMagnitudeCalculationType() == EGameplayEffectMagnitudeCalculation::SetByCaller &&
                          Stored.GetSetByCallerFloat().DataTag == SetByCaller.DataTag &&
                          Stored.GetSetByCallerFloat().DataName == SetByCaller.DataName
                        : Stored.GetStaticMagnitudeIfPossible(1.0f, VerifiedValue) &&
                          FMath::IsNearlyEqual(VerifiedValue, Value, KINDA_SMALL_NUMBER);
                }
            }
        }

        if (!bCompiled || !bMagnitudeVerified)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Modifier magnitude could not be verified on the compiled class (index %d)%s. The asset was NOT saved."),
                    ModifierIndex,
                    GASCompileFailureNote(bCompiled)),
                TEXT("MAGNITUDE_NOT_APPLIED"));
            return true;
        }

        if (!SaveVerifiedGASBlueprint(Context, Blueprint, TEXT("Magnitude")))
        {
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetNumberField(TEXT("modifierIndex"), ModifierIndex);
        Result->SetStringField(TEXT("magnitudeType"), MagnitudeType);
        // Read back from the compiled CDO, not echoed from the request.
        Result->SetNumberField(TEXT("value"), VerifiedValue);
        Result->SetBoolField(TEXT("verifiedOnCompiledClass"), true);
        Result->SetBoolField(TEXT("savedToDisk"), true);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Magnitude set"), Result);
        return true;
    }

    return false;
}
}
