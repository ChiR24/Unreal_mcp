#include "Domains/GAS/McpAutomationBridge_GASPayloadFields.h"
#include "Domains/GAS/McpAutomationBridge_GASRequestContext.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#if WITH_EDITOR && MCP_HAS_GAS
#include "Engine/Blueprint.h"
#include "GameplayEffect.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

#if WITH_EDITOR && MCP_HAS_GAS
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

        int32 ModifierIndex = static_cast<int32>(GetJsonNumberField(Payload, TEXT("modifierIndex"), 0));
        float Value = static_cast<float>(GetGASNumberFieldWithFallback(Payload, TEXT("value"), TEXT("modifierMagnitude"), 0.0));
        FString MagnitudeType = GetGASStringFieldWithFallback(Payload, TEXT("magnitudeType"), TEXT("magnitudeCalculationType"), TEXT("ScalableFloat"));

        if (ModifierIndex >= EffectCDO->Modifiers.Num())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Modifier index out of range"), TEXT("INVALID_INDEX"));
            return true;
        }

        // Note: SetValue doesn't exist in UE 5.6. Use FScalableFloat constructor.
        EffectCDO->Modifiers[ModifierIndex].ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Value));

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
                    bMagnitudeVerified = CompiledCDO->Modifiers[ModifierIndex].ModifierMagnitude
                        .GetStaticMagnitudeIfPossible(1.0f, VerifiedValue) &&
                        FMath::IsNearlyEqual(VerifiedValue, Value, KINDA_SMALL_NUMBER);
                }
            }
        }

        if (!bCompiled || !bMagnitudeVerified)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Modifier magnitude could not be verified on the compiled class (index %d)%s. The asset was NOT saved."),
                    ModifierIndex,
                    bCompiled ? TEXT("") : TEXT(" (the Blueprint failed to compile - it may have unrelated graph errors)")),
                TEXT("MAGNITUDE_NOT_APPLIED"));
            return true;
        }

        if (!McpSafeAssetSave(Blueprint))
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                TEXT("Magnitude verified on the compiled class but the asset could NOT be written to disk (file may be read-only or held by source control). The change exists only in this editor session."),
                TEXT("SAVE_FAILED"));
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
#endif
