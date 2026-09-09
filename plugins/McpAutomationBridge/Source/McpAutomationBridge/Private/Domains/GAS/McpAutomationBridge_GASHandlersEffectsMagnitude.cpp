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
            }
        }

        const bool bPeriodVerified = !bHasPeriod || DurationTypeToken == TEXT("instant") ||
            FMath::IsNearlyEqual(VerifiedPeriod, Period, KINDA_SMALL_NUMBER);
        if (!bCompiled || VerifiedPolicy.IsEmpty() || !bPeriodVerified)
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
        Result->SetStringField(TEXT("durationType"), DurationType);
        Result->SetNumberField(TEXT("duration"), Duration);
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

        // ---- Bind the modifier to the attribute it is supposed to modify. -------------------------
        // This was the whole defect: FGameplayModifierInfo was default-constructed, given an op and a
        // magnitude, and pushed onto Modifiers with its Attribute left null. The effect then carried a
        // modifier that modified nothing, while the handler answered success:true with modifierCount:1 --
        // a real read-back of the wrong thing. Both spellings are accepted because both are advertised:
        // the native schema documents `targetAttribute` and the TS route lists `attributeName`.
        const FString TargetAttribute =
            GetGASStringFieldWithFallback(Payload, TEXT("targetAttribute"), TEXT("attributeName"), TEXT("")).TrimStartAndEnd();
        if (TargetAttribute.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                TEXT("Missing targetAttribute (alias: attributeName) - a modifier with no attribute modifies nothing."),
                TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // Accept both "Health" and "AS_Foo.Health". The part after the last dot is the property name;
        // the part before it, when present, names the owning AttributeSet class and IS HONORED below --
        // an accepted-but-ignored qualifier would bind a same-named attribute from the wrong set and
        // report success, which is this defect class with extra steps.
        FString AttributeShortName = TargetAttribute;
        FString ClassQualifier;
        int32 DotIdx = INDEX_NONE;
        if (AttributeShortName.FindLastChar(TEXT('.'), DotIdx))
        {
            ClassQualifier = AttributeShortName.Left(DotIdx);
            AttributeShortName = AttributeShortName.Mid(DotIdx + 1);
        }

        // Resolve against every UAttributeSet subclass, native or Blueprint-generated: the caller's
        // attribute usually lives in a Blueprint AttributeSet authored moments earlier by add_attribute.
        FProperty* AttributeProperty = nullptr;
        UClass* OwningAttributeSetClass = nullptr;
        TArray<FString> AmbiguousOwners;
        for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
        {
            UClass* Candidate = *ClassIt;
            if (!Candidate || !Candidate->IsChildOf(UAttributeSet::StaticClass()))
            {
                continue;
            }

            // Skip compilation debris. Recompiling a Blueprint leaves REINST_*/SKEL_*/TRASHCLASS_*
            // classes alive in memory, and they carry the same property names as the real generated
            // class. Binding an attribute to one of those produces an FGameplayAttribute that looks
            // valid -- IsValid() is true and GetName() returns the right string -- while pointing at a
            // class the game never loads. Measured: without this filter, an attribute on a freshly
            // recompiled Blueprint AttributeSet resolved to its REINST_SKEL_* debris class and the
            // handler reported success while binding to a class the game never loads.
            const FString CandidateName = Candidate->GetName();
            if (Candidate->HasAnyClassFlags(CLASS_NewerVersionExists) ||
                CandidateName.StartsWith(TEXT("REINST_")) ||
                CandidateName.StartsWith(TEXT("SKEL_")) ||
                CandidateName.StartsWith(TEXT("TRASHCLASS_")))
            {
                continue;
            }
            if (FProperty* Found = Candidate->FindPropertyByName(FName(*AttributeShortName)))
            {
                // A gameplay attribute is an FGameplayAttributeData struct property; anything else with a
                // matching name is a different member and must not be silently accepted.
                if (const FStructProperty* AsStruct = CastField<FStructProperty>(Found))
                {
                    if (AsStruct->Struct == FGameplayAttributeData::StaticStruct())
                    {
                        // Honor the qualifier: "AS_Foo.Health" matches the class named AS_Foo (or its
                        // Blueprint generated form AS_Foo_C) and nothing else.
                        if (!ClassQualifier.IsEmpty())
                        {
                            // CandidateName is the loop-level name computed for the debris filter above.
                            if (CandidateName != ClassQualifier && CandidateName != ClassQualifier + TEXT("_C"))
                            {
                                continue;
                            }
                            AttributeProperty = Found;
                            OwningAttributeSetClass = Candidate;
                            break;
                        }
                        // Unqualified: collect every match. One match binds; several is an ERROR the
                        // caller resolves with the qualified form -- first-match-wins would bind an
                        // arbitrary same-named attribute (native classes load first, so the wrong winner
                        // is even deterministic) and report success.
                        AmbiguousOwners.Add(Candidate->GetName());
                        if (!AttributeProperty)
                        {
                            AttributeProperty = Found;
                            OwningAttributeSetClass = Candidate;
                        }
                    }
                }
            }
        }

        if (!AttributeProperty)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Attribute '%s' not found on any loaded AttributeSet. If it was just authored, make sure its Blueprint compiled."), *TargetAttribute),
                TEXT("ATTRIBUTE_NOT_FOUND"));
            return true;
        }
        if (ClassQualifier.IsEmpty() && AmbiguousOwners.Num() > 1)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Attribute '%s' exists on %d different AttributeSet classes (%s). Disambiguate with the qualified form, e.g. '%s.%s'."),
                    *AttributeShortName, AmbiguousOwners.Num(), *FString::Join(AmbiguousOwners, TEXT(", ")),
                    *AmbiguousOwners[0], *AttributeShortName),
                TEXT("AMBIGUOUS_ATTRIBUTE"));
            return true;
        }

        Modifier.Attribute.SetUProperty(AttributeProperty);
        EffectCDO->Modifiers.Add(Modifier);

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        const bool bCompiled = McpSafeCompileBlueprint(Blueprint);

        // Read back from the recompiled CDO. Compilation reinstances, so re-fetch rather than trusting
        // the pointer we mutated: the question being answered is "is the modifier bound on the asset the
        // engine will load", not "did we write to some object". The save comes AFTER verification --
        // persisting a state we are about to report as failed would leave a broken asset on disk under
        // an error message that denies it exists.
        bool bBindingVerified = false;
        FString VerifiedAttributeName;
        if (UClass* CompiledClass = Blueprint->GeneratedClass)
        {
            if (UGameplayEffect* CompiledCDO = Cast<UGameplayEffect>(CompiledClass->GetDefaultObject()))
            {
                if (CompiledCDO->Modifiers.Num() > 0)
                {
                    const FGameplayModifierInfo& Last = CompiledCDO->Modifiers.Last();
                    VerifiedAttributeName = Last.Attribute.GetName();
                    // IsValid() alone is NOT enough: it stays true for a property owned by a
                    // reinstanced/skeleton class, which is exactly the failure this handler used to
                    // ship. Require a live owner class as well.
                    const FProperty* BoundProp = Last.Attribute.GetUProperty();
                    const UClass* BoundOwner = BoundProp ? BoundProp->GetOwner<UClass>() : nullptr;
                    const FString BoundOwnerName = BoundOwner ? BoundOwner->GetName() : FString();
                    bBindingVerified =
                        Last.Attribute.IsValid() && BoundOwner != nullptr &&
                        !BoundOwner->HasAnyClassFlags(CLASS_NewerVersionExists) &&
                        !BoundOwnerName.StartsWith(TEXT("REINST_")) &&
                        !BoundOwnerName.StartsWith(TEXT("SKEL_")) &&
                        !BoundOwnerName.StartsWith(TEXT("TRASHCLASS_"));
                }
            }
        }

        if (!bCompiled || !bBindingVerified)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Modifier was added but its attribute binding could not be verified on the compiled class (attribute '%s')%s - the effect would modify nothing. The asset was NOT saved."),
                    *TargetAttribute,
                    bCompiled ? TEXT("") : TEXT(" (the Blueprint failed to compile - it may have unrelated graph errors)")),
                TEXT("MODIFIER_NOT_BOUND"));
            return true;
        }

        if (!McpSafeAssetSave(Blueprint))
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                TEXT("Modifier verified on the compiled class but the asset could NOT be written to disk (file may be read-only or held by source control). The change exists only in this editor session."),
                TEXT("SAVE_FAILED"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("operation"), Operation);
        Result->SetNumberField(TEXT("magnitude"), Magnitude);
        Result->SetNumberField(TEXT("modifierCount"), EffectCDO->Modifiers.Num());
        Result->SetStringField(TEXT("targetAttribute"), TargetAttribute);
        // Read back from the compiled CDO, not echoed from the request.
        Result->SetStringField(TEXT("boundAttribute"), VerifiedAttributeName);
        Result->SetStringField(TEXT("attributeSetClass"), OwningAttributeSetClass ? OwningAttributeSetClass->GetName() : FString());
        Result->SetBoolField(TEXT("verifiedOnCompiledClass"), true);
        Result->SetBoolField(TEXT("savedToDisk"), true);
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
