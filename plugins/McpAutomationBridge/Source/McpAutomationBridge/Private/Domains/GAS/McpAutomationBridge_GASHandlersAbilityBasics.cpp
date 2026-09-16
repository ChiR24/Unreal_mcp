#include "Domains/GAS/McpAutomationBridge_GASAbilityReflection.h"
#include "Domains/GAS/McpAutomationBridge_GASBlueprintCreation.h"
#include "Domains/GAS/McpAutomationBridge_GASEffectClassResolution.h"
#include "Domains/GAS/McpAutomationBridge_GASPayloadFields.h"
#include "Domains/GAS/McpAutomationBridge_GASRequestContext.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#if WITH_EDITOR && MCP_HAS_GAS
#include "Dom/JsonValue.h"
#include "Engine/Blueprint.h"
#include "GameplayEffect.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

#if WITH_EDITOR && MCP_HAS_GAS
namespace McpGASHandlers
{
bool HandleGASAbilityBasics(const FGASRequestContext& Context, const FString& SubAction)
{
    UMcpAutomationBridgeSubsystem* Bridge = Context.Subsystem;
    const FString& RequestId = Context.RequestId;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    const FString& Name = Context.Name;
    const FString& Path = Context.Path;
    const FString& BlueprintPath = Context.BlueprintPath;
    const FString& AssetPath = Context.AssetPath;

    if (SubAction == TEXT("create_gameplay_ability"))
    {
        if (Name.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing name."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString Error;
        bool bReusedExisting = false;
        UBlueprint* Blueprint = CreateGASBlueprint(Path, Name, UGameplayAbility::StaticClass(), Error, bReusedExisting);
        if (!Blueprint)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
            return true;
        }

        if (!bReusedExisting)
        {
            McpSafeAssetSave(Blueprint);
        }

        // Use the actual blueprint name (which may have been sanitized) in the response
        FString ActualName = Blueprint->GetName();
        FString ActualPath = Path / ActualName;

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("assetPath"), ActualPath);
        Result->SetStringField(TEXT("name"), ActualName);
        Result->SetStringField(TEXT("parentClass"), TEXT("GameplayAbility"));
        Result->SetBoolField(TEXT("reusedExisting"), bReusedExisting);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true,
            bReusedExisting ? TEXT("Ability already exists") : TEXT("Ability created"), Result);
        return true;
    }

    if (SubAction == TEXT("set_ability_tags"))
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

        UGameplayAbility* AbilityCDO = Cast<UGameplayAbility>(Blueprint->GeneratedClass->GetDefaultObject());
        if (!AbilityCDO)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Not a GameplayAbility blueprint"), TEXT("INVALID_TYPE"));
            return true;
        }

        // ---- Phase 1: resolve EVERYTHING before writing ANYTHING. -------------------------------
        // Two defects lived here. First, only the abilityTags array detected unregistered tags -- the
        // four other containers (and their singular aliases) silently dropped them and the handler still
        // answered "Ability tags set", which is the exact silent-success class this change exists to end.
        // Second, validation ran AFTER the writes, so a refused mixed call left half-applied tags on the
        // in-memory CDO for a later unrelated save to quietly persist. Resolving everything up front
        // makes the refusal side-effect-free and covers every container equally.
        struct FReflectionTagWrite
        {
            FName ContainerProp;
            FGameplayTag Tag;
            FString Requested;
        };
        TArray<FString> TagsUnresolved;
        TArray<FString> TagsAdded;
        TArray<TPair<FString, FGameplayTag>> AssetTagWrites;
        TArray<FReflectionTagWrite> ReflectionWrites;

        auto ResolveArray = [&](const TCHAR* Primary, const TCHAR* SingularAlias, const FName& ContainerProp)
        {
            const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
            if (!Payload->TryGetArrayField(Primary, Arr) && SingularAlias != nullptr)
            {
                Payload->TryGetArrayField(SingularAlias, Arr);
            }
            if (!Arr)
            {
                return;
            }
            for (const auto& TagValue : *Arr)
            {
                const FString TagStr = TagValue->AsString();
                const FGameplayTag Tag = GetOrRequestTag(TagStr);
                if (!Tag.IsValid())
                {
                    // GetOrRequestTag does NOT create a tag that is not in the project's registry;
                    // an unregistered tag used to be skipped in silence here.
                    TagsUnresolved.Add(TagStr);
                    continue;
                }
                if (ContainerProp.IsNone())
                {
                    AssetTagWrites.Emplace(TagStr, Tag);
                    TagsAdded.Add(TagStr);
                }
                else
                {
                    ReflectionWrites.Add({ContainerProp, Tag, TagStr});
                }
            }
        };

        ResolveArray(TEXT("abilityTags"), nullptr, NAME_None);
        ResolveArray(TEXT("cancelAbilitiesWithTags"), TEXT("cancelAbilitiesWithTag"), FName(TEXT("CancelAbilitiesWithTag")));
        ResolveArray(TEXT("blockAbilitiesWithTags"), TEXT("blockAbilitiesWithTag"), FName(TEXT("BlockAbilitiesWithTag")));
        ResolveArray(TEXT("activationRequiredTags"), nullptr, FName(TEXT("ActivationRequiredTags")));
        ResolveArray(TEXT("activationBlockedTags"), nullptr, FName(TEXT("ActivationBlockedTags")));

        if (TagsUnresolved.Num() > 0)
        {
            // Refuse BEFORE any write, so the CDO is exactly as it was. The remedy names the project's
            // own tag registry rather than any particular tool.
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Gameplay tag(s) not registered in this project: %s. Register them in the project's GameplayTags settings (e.g. DefaultGameplayTags.ini) first, then retry. Nothing was changed."),
                    *FString::Join(TagsUnresolved, TEXT(", "))),
                TEXT("GAMEPLAY_TAG_NOT_REGISTERED"));
            return true;
        }

        // ---- Phase 2: write. --------------------------------------------------------------------
        if (AssetTagWrites.Num() > 0)
        {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
            // UE 5.7+: AbilityTags is deprecated for direct use; read via GetAssetTags, write the
            // container back under deprecation suppression (SetAssetTags only works in constructors).
            FGameplayTagContainer CurrentTags = AbilityCDO->GetAssetTags();
            for (const auto& Pair : AssetTagWrites)
            {
                CurrentTags.AddTag(Pair.Value);
            }
            PRAGMA_DISABLE_DEPRECATION_WARNINGS
            AbilityCDO->AbilityTags = CurrentTags;
            PRAGMA_ENABLE_DEPRECATION_WARNINGS
#else
            PRAGMA_DISABLE_DEPRECATION_WARNINGS
            for (const auto& Pair : AssetTagWrites)
            {
                AbilityCDO->AbilityTags.AddTag(Pair.Value);
            }
            PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
        }
        for (const FReflectionTagWrite& Write : ReflectionWrites)
        {
            AddTagToAbilityContainer(AbilityCDO, Write.ContainerProp, Write.Tag);
        }

        // ---- Phase 3: compile, VERIFY on the recompiled CDO, and only then save. ----------------
        // Compilation reinstances the CDO, so every read below re-fetches. Whether hand-written CDO
        // container values survive reinstancing is MEASURED here, not assumed; and the compile/save
        // results are consulted rather than discarded -- a Blueprint whose graph has unrelated compile
        // errors, or a file source control holds read-only, must not produce success:true.
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        const bool bCompiled = McpSafeCompileBlueprint(Blueprint);

        TArray<FString> TagsVerified;
        TArray<FString> OtherTagsVerified;
        TArray<FString> TagsLost;
        if (UClass* CompiledClass = Blueprint->GeneratedClass)
        {
            if (UGameplayAbility* CompiledCDO = Cast<UGameplayAbility>(CompiledClass->GetDefaultObject()))
            {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
                const FGameplayTagContainer PersistedAssetTags = CompiledCDO->GetAssetTags();
#else
                PRAGMA_DISABLE_DEPRECATION_WARNINGS
                const FGameplayTagContainer PersistedAssetTags = CompiledCDO->AbilityTags;
                PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
                for (const auto& Pair : AssetTagWrites)
                {
                    if (PersistedAssetTags.HasTagExact(Pair.Value))
                    {
                        TagsVerified.Add(Pair.Key);
                    }
                    else
                    {
                        TagsLost.Add(Pair.Key);
                    }
                }
                for (const FReflectionTagWrite& Write : ReflectionWrites)
                {
                    FGameplayTagContainer Persisted;
                    if (GetAbilityPropertyValue<FGameplayTagContainer>(CompiledCDO, Write.ContainerProp, Persisted) &&
                        Persisted.HasTagExact(Write.Tag))
                    {
                        OtherTagsVerified.Add(Write.ContainerProp.ToString() + TEXT(":") + Write.Requested);
                    }
                    else
                    {
                        TagsLost.Add(Write.ContainerProp.ToString() + TEXT(":") + Write.Requested);
                    }
                }
            }
        }

        if (!bCompiled || TagsLost.Num() > 0)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Ability tags did not persist onto the compiled class%s: %s. The asset was NOT saved."),
                    bCompiled ? TEXT("") : TEXT(" (the Blueprint failed to compile - it may have unrelated graph errors)"),
                    TagsLost.Num() > 0 ? *FString::Join(TagsLost, TEXT(", ")) : TEXT("(compile failure)")),
                TEXT("ABILITY_TAGS_NOT_APPLIED"));
            return true;
        }

        if (!McpSafeAssetSave(Blueprint))
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                TEXT("Ability tags verified on the compiled class but the asset could NOT be written to disk (file may be read-only or held by source control). The change exists only in this editor session."),
                TEXT("SAVE_FAILED"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        TArray<TSharedPtr<FJsonValue>> TagsJsonArray;
        for (const FString& Tag : TagsAdded)
        {
            TagsJsonArray.Add(MakeShared<FJsonValueString>(Tag));
        }
        Result->SetArrayField(TEXT("tagsAdded"), TagsJsonArray);
        // Read back from the compiled class rather than echoing what we were asked to write.
        TArray<TSharedPtr<FJsonValue>> VerifiedJsonArray;
        for (const FString& Tag : TagsVerified)
        {
            VerifiedJsonArray.Add(MakeShared<FJsonValueString>(Tag));
        }
        Result->SetArrayField(TEXT("tagsVerified"), VerifiedJsonArray);
        TArray<TSharedPtr<FJsonValue>> OtherVerifiedJsonArray;
        for (const FString& Entry : OtherTagsVerified)
        {
            OtherVerifiedJsonArray.Add(MakeShared<FJsonValueString>(Entry));
        }
        Result->SetArrayField(TEXT("otherTagsVerified"), OtherVerifiedJsonArray);
        Result->SetBoolField(TEXT("verifiedOnCompiledClass"), true);
        Result->SetBoolField(TEXT("savedToDisk"), true);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ability tags set"), Result);
        return true;
    }

    if (SubAction == TEXT("set_ability_costs"))
    {
        if (BlueprintPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString CostEffectPath = GetJsonStringField(Payload, TEXT("costEffectPath"));

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        UGameplayAbility* AbilityCDO = Cast<UGameplayAbility>(Blueprint->GeneratedClass->GetDefaultObject());
        if (!AbilityCDO)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Not a GameplayAbility blueprint"), TEXT("INVALID_TYPE"));
            return true;
        }

        bool bCostEffectAssigned = false;
        if (!CostEffectPath.IsEmpty())
        {
            UClass* CostClass = ResolveGameplayEffectClassFromPath(CostEffectPath);
            if (!CostClass)
            {
                Bridge->SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("Cost GameplayEffect not found or invalid: %s"), *CostEffectPath), TEXT("ASSET_NOT_FOUND"));
                return true;
            }

            // Use reflection to set protected CostGameplayEffectClass property
            // Use string literal - GET_MEMBER_NAME_CHECKED doesn't work for protected members
            bCostEffectAssigned = SetAbilityPropertyValue(AbilityCDO, FName(TEXT("CostGameplayEffectClass")), TSubclassOf<UGameplayEffect>(CostClass));
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("costEffectPath"), CostEffectPath);
        Result->SetBoolField(TEXT("costEffectAssigned"), bCostEffectAssigned);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ability cost set"), Result);
        return true;
    }

    if (SubAction == TEXT("set_ability_cooldown"))
    {
        if (BlueprintPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString CooldownEffectPath = GetJsonStringField(Payload, TEXT("cooldownEffectPath"));

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        UGameplayAbility* AbilityCDO = Cast<UGameplayAbility>(Blueprint->GeneratedClass->GetDefaultObject());
        if (!AbilityCDO)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Not a GameplayAbility blueprint"), TEXT("INVALID_TYPE"));
            return true;
        }

        bool bCooldownEffectAssigned = false;
        if (!CooldownEffectPath.IsEmpty())
        {
            UClass* CooldownClass = ResolveGameplayEffectClassFromPath(CooldownEffectPath);
            if (!CooldownClass)
            {
                Bridge->SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("Cooldown GameplayEffect not found or invalid: %s"), *CooldownEffectPath), TEXT("ASSET_NOT_FOUND"));
                return true;
            }

            // Use reflection to set protected CooldownGameplayEffectClass property
            // Use string literal - GET_MEMBER_NAME_CHECKED doesn't work for protected members
            bCooldownEffectAssigned = SetAbilityPropertyValue(AbilityCDO, FName(TEXT("CooldownGameplayEffectClass")), TSubclassOf<UGameplayEffect>(CooldownClass));
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("cooldownEffectPath"), CooldownEffectPath);
        Result->SetBoolField(TEXT("cooldownEffectAssigned"), bCooldownEffectAssigned);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ability cooldown set"), Result);
        return true;
    }

    return false;
}
}
#endif
