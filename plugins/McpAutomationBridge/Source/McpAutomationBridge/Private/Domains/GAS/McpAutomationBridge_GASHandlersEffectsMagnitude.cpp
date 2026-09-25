#include "Domains/GAS/McpAutomationBridge_GASBlueprintCreation.h"
#include "Domains/GAS/McpAutomationBridge_GASPayloadFields.h"
#include "Domains/GAS/McpAutomationBridge_GASRequestContext.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#if WITH_EDITOR && MCP_HAS_GAS
#include "Engine/Blueprint.h"
#include "AttributeSet.h"
#include "GameplayEffect.h"
#include "UObject/UObjectIterator.h"
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

        FString DurationType = GetJsonStringField(Payload, TEXT("durationType"), TEXT("Instant"));
        const FString DurationTypeToken = NormalizeGASToken(DurationType);

        // Only set duration policy on CDO if we created a new blueprint
        if (!bReusedExisting)
        {
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

                    // duration and period were accepted by the schema and silently dropped here: the
                    // caller could ask for a 5-second effect ticking every second, be told the effect
                    // was created, and get an effect with no duration and no tick. Applying them at
                    // creation means the common one-call case authors a complete effect; the separate
                    // set_effect_duration action still exists for changing it afterwards.
                    if (DurationTypeToken != TEXT("instant"))
                    {
                        if (Payload.IsValid() && Payload->HasField(TEXT("duration")))
                        {
                            const float CreateDuration = static_cast<float>(GetJsonNumberField(Payload, TEXT("duration"), 0.0));
                            EffectCDO->DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(CreateDuration));
                        }
                        if (Payload.IsValid() && Payload->HasField(TEXT("period")))
                        {
                            const float CreatePeriod = static_cast<float>(GetJsonNumberField(Payload, TEXT("period"), 0.0));
                            EffectCDO->Period = FScalableFloat(CreatePeriod);
                        }
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

        FString DurationType = GetJsonStringField(Payload, TEXT("durationType"), TEXT("Instant"));
        const FString DurationTypeToken = NormalizeGASToken(DurationType);
        float Duration = static_cast<float>(GetJsonNumberField(Payload, TEXT("duration"), 0.0));

        // Resolve the requested policy BEFORE touching the CDO. An unrecognized token used to fall through
        // every branch, leave the previous policy in place, pass a read-back that only checked that SOME
        // policy was present, and report success for a request that changed nothing.
        FString ExpectedPolicy;
        EGameplayEffectDurationType ExpectedPolicyValue = EGameplayEffectDurationType::Instant;
        if (DurationTypeToken == TEXT("instant"))
        {
            ExpectedPolicy = TEXT("Instant");
            ExpectedPolicyValue = EGameplayEffectDurationType::Instant;
        }
        else if (DurationTypeToken == TEXT("infinite"))
        {
            ExpectedPolicy = TEXT("Infinite");
            ExpectedPolicyValue = EGameplayEffectDurationType::Infinite;
        }
        else if (DurationTypeToken == TEXT("hasduration"))
        {
            ExpectedPolicy = TEXT("HasDuration");
            ExpectedPolicyValue = EGameplayEffectDurationType::HasDuration;
        }
        else
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Unsupported durationType '%s'. Expected one of: Instant, Infinite, HasDuration. Nothing was changed."), *DurationType),
                TEXT("INVALID_ARGUMENT"));
            return true;
        }

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
        if (bHasPeriod && DurationTypeToken != TEXT("instant"))
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
                switch (CompiledCDO->DurationPolicy)
                {
                case EGameplayEffectDurationType::Instant:     VerifiedPolicy = TEXT("Instant");     break;
                case EGameplayEffectDurationType::Infinite:    VerifiedPolicy = TEXT("Infinite");    break;
                case EGameplayEffectDurationType::HasDuration: VerifiedPolicy = TEXT("HasDuration"); break;
                default: break;
                }
                VerifiedPeriod = CompiledCDO->Period.GetValueAtLevel(0.0f);
                bDurationReadable = CompiledCDO->DurationMagnitude.GetStaticMagnitudeIfPossible(0.0f, VerifiedDuration);
            }
        }

        const bool bPeriodVerified = !bHasPeriod || DurationTypeToken == TEXT("instant") ||
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
            Result->SetBoolField(TEXT("periodApplied"), DurationTypeToken != TEXT("instant") && VerifiedPeriod > 0.0f);
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
#endif
