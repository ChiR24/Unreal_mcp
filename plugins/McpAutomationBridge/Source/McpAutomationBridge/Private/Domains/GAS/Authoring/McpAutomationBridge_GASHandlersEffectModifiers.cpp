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
bool HandleGASEffectModifiers(const FGASRequestContext& Context, const FString& SubAction)
{
    UMcpAutomationBridgeSubsystem* Bridge = Context.Subsystem;
    const FString& RequestId = Context.RequestId;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    const FString& BlueprintPath = Context.BlueprintPath;

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
                        // FindPropertyByName walks the super chain, so one property declared on a base
                        // AttributeSet is found again through every subclass. Count its DECLARING class
                        // once; two genuinely different properties still yield two owners.
                        const UClass* DeclaringClass = Found->GetOwnerClass();
                        AmbiguousOwners.AddUnique(DeclaringClass ? DeclaringClass->GetName() : Candidate->GetName());
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
        int32 VerifiedModifierCount = 0;
        if (UClass* CompiledClass = Blueprint->GeneratedClass)
        {
            if (UGameplayEffect* CompiledCDO = Cast<UGameplayEffect>(CompiledClass->GetDefaultObject()))
            {
                VerifiedModifierCount = CompiledCDO->Modifiers.Num();
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
        // EffectCDO is the pre-compile object; the compile above reinstanced it.
        Result->SetNumberField(TEXT("modifierCount"), VerifiedModifierCount);
        Result->SetStringField(TEXT("targetAttribute"), TargetAttribute);
        // Read back from the compiled CDO, not echoed from the request.
        Result->SetStringField(TEXT("boundAttribute"), VerifiedAttributeName);
        Result->SetStringField(TEXT("attributeSetClass"), OwningAttributeSetClass ? OwningAttributeSetClass->GetName() : FString());
        Result->SetBoolField(TEXT("verifiedOnCompiledClass"), true);
        Result->SetBoolField(TEXT("savedToDisk"), true);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Modifier added"), Result);
        return true;
    }

    return false;
}
}
#endif
