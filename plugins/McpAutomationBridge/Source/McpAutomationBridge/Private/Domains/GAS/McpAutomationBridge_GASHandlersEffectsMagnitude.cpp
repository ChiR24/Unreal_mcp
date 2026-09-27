#include "Domains/GAS/McpAutomationBridge_GASBlueprintCreation.h"
#include "Domains/GAS/McpAutomationBridge_GASPayloadFields.h"
#include "Domains/GAS/McpAutomationBridge_GASRequestContext.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Safety/McpSafeOperations.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Engine/Blueprint.h"
#include "AttributeSet.h"
#include "GameplayEffect.h"
#include "UObject/UObjectIterator.h"
#include "Kismet2/BlueprintEditorUtils.h"

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
        const FString DurationType = GetJsonStringField(Payload, TEXT("durationType"), TEXT("Instant"));
        bool bReusedExisting = false;
        // A new effect gets its duration policy, and for a non-Instant one the requested duration and
        // period (both used to be dropped, so a "5 s, ticking every second" effect never ticked).
        const TSharedPtr<FJsonObject> Result = CreateGASAsset(Context, UGameplayEffect::StaticClass(), TEXT("GameplayEffect"),
            bReusedExisting, [&DurationType, &Payload](UBlueprint* Blueprint) {
                UGameplayEffect* EffectCDO = Blueprint->GeneratedClass
                    ? Cast<UGameplayEffect>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
                if (!EffectCDO || !TryParseGASEnum(DurationType, EffectCDO->DurationPolicy)
                    || EffectCDO->DurationPolicy == EGameplayEffectDurationType::Instant)
                {
                    return;
                }
                if (Payload->HasField(TEXT("duration")))
                {
                    EffectCDO->DurationMagnitude = FGameplayEffectModifierMagnitude(
                        FScalableFloat(static_cast<float>(GetJsonNumberField(Payload, TEXT("duration"), 0.0))));
                }
                if (Payload->HasField(TEXT("period")))
                {
                    EffectCDO->Period = FScalableFloat(static_cast<float>(GetJsonNumberField(Payload, TEXT("period"), 0.0)));
                }
            });
        if (!Result)
        {
            return true;
        }
        Result->SetStringField(TEXT("durationType"), DurationType);
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

        UBlueprint* Blueprint = nullptr;
        UGameplayEffect* EffectCDO = LoadGASBlueprintCDO<UGameplayEffect>(Context, Blueprint, TEXT("GameplayEffect"));
        if (!EffectCDO)
        {
            return true;
        }

        FString DurationType = GetJsonStringField(Payload, TEXT("durationType"), TEXT("Instant"));
        float Duration = static_cast<float>(GetJsonNumberField(Payload, TEXT("duration"), 0.0));

        // Resolve the requested policy BEFORE touching the CDO. An unrecognized token used to fall through
        // every branch, leave the previous policy in place, pass a read-back that only checked that SOME
        // policy was present, and report success for a request that changed nothing.
        EGameplayEffectDurationType ExpectedPolicyValue = EGameplayEffectDurationType::Instant;
        if (!TryParseGASEnum(DurationType, ExpectedPolicyValue))
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Unsupported durationType '%s'. Expected one of: Instant, Infinite, HasDuration. Nothing was changed."), *DurationType),
                TEXT("INVALID_ARGUMENT"));
            return true;
        }
        const FString ExpectedPolicy = GASEnumName(ExpectedPolicyValue);

        EffectCDO->DurationPolicy = ExpectedPolicyValue;
        if (ExpectedPolicyValue == EGameplayEffectDurationType::HasDuration)
        {
            EffectCDO->DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Duration));
        }

        // Period had NO write path anywhere in the codebase: create_gameplay_effect reads only
        // durationType, and this handler read only durationType/duration. A "damage over time" effect
        // authored through these tools therefore had Period 0 and never ticked -- it applied once and
        // stopped, which is a different effect from the one the caller asked for. Only meaningful for a
        // non-Instant policy, so it is applied but not invented for Instant.
        const bool bHasPeriod = Payload.IsValid() && Payload->HasField(TEXT("period"));
        float Period = static_cast<float>(GetJsonNumberField(Payload, TEXT("period"), 0.0));
        if (bHasPeriod && ExpectedPolicyValue != EGameplayEffectDurationType::Instant)
        {
            EffectCDO->Period = FScalableFloat(Period);
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        const bool bCompiled = McpSafeCompileBlueprint(Blueprint);

        // Read back policy AND period from the recompiled CDO rather than echoing the request --
        // periodApplied used to be computed from the request token, which is a prediction, not a
        // measurement. Save only after the read-back agrees.
        FString VerifiedPolicy;
        float VerifiedPeriod = 0.0f;
        float VerifiedDuration = 0.0f;
        bool bDurationReadable = false;
        if (UClass* CompiledClass = Blueprint->GeneratedClass)
        {
            if (UGameplayEffect* CompiledCDO = Cast<UGameplayEffect>(CompiledClass->GetDefaultObject()))
            {
                VerifiedPolicy = GASEnumName(CompiledCDO->DurationPolicy);
                VerifiedPeriod = CompiledCDO->Period.GetValueAtLevel(0.0f);
                bDurationReadable = CompiledCDO->DurationMagnitude.GetStaticMagnitudeIfPossible(0.0f, VerifiedDuration);
            }
        }

        const bool bPeriodVerified = !bHasPeriod || ExpectedPolicyValue == EGameplayEffectDurationType::Instant ||
            FMath::IsNearlyEqual(VerifiedPeriod, Period, KINDA_SMALL_NUMBER);
        // Compare against what was REQUESTED, not merely that something is present: a leftover policy from
        // before this call is non-empty too.
        const bool bPolicyVerified = VerifiedPolicy == ExpectedPolicy;
        const bool bDurationVerified = ExpectedPolicyValue != EGameplayEffectDurationType::HasDuration ||
            (bDurationReadable && FMath::IsNearlyEqual(VerifiedDuration, Duration, KINDA_SMALL_NUMBER));
        if (!bCompiled || !bPolicyVerified || !bDurationVerified || !bPeriodVerified)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Duration/period could not be verified on the compiled class%s. The asset was NOT saved."),
                    bCompiled ? TEXT("") : TEXT(" (the Blueprint failed to compile - it may have unrelated graph errors)")),
                TEXT("DURATION_NOT_APPLIED"));
            return true;
        }

        if (!McpSafeAssetSave(Blueprint))
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                TEXT("Duration verified on the compiled class but the asset could NOT be written to disk (file may be read-only or held by source control). The change exists only in this editor session."),
                TEXT("SAVE_FAILED"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("durationType"), VerifiedPolicy);
        // Read back from the compiled CDO for HasDuration; the other policies have no duration to measure.
        Result->SetNumberField(TEXT("duration"),
            ExpectedPolicyValue == EGameplayEffectDurationType::HasDuration ? VerifiedDuration : Duration);
        if (bHasPeriod)
        {
            // Read back from the compiled CDO, not echoed from the request.
            Result->SetNumberField(TEXT("period"), VerifiedPeriod);
            Result->SetBoolField(TEXT("periodApplied"), ExpectedPolicyValue != EGameplayEffectDurationType::Instant && VerifiedPeriod > 0.0f);
        }
        Result->SetStringField(TEXT("durationPolicy"), VerifiedPolicy);
        Result->SetBoolField(TEXT("verifiedOnCompiledClass"), true);
        Result->SetBoolField(TEXT("savedToDisk"), true);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Duration set"), Result);
        return true;
    }

    return false;
}
}
