#include "Domains/GAS/McpAutomationBridge_GASAbilityReflection.h"
#include "Domains/GAS/McpAutomationBridge_GASPayloadFields.h"
#include "Domains/GAS/McpAutomationBridge_GASRequestContext.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Safety/McpSafeOperations.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonValue.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace McpGASHandlers
{
bool HandleGASAbilityTags(const FGASRequestContext& Context, const FString& SubAction)
{
    if (SubAction != TEXT("set_ability_tags"))
    {
        return false;
    }
    UMcpAutomationBridgeSubsystem* Bridge = Context.Subsystem;
    const FString& RequestId = Context.RequestId;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    const FString& BlueprintPath = Context.BlueprintPath;

    if (BlueprintPath.IsEmpty())
    {
        Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UBlueprint* Blueprint = nullptr;
    UGameplayAbility* AbilityCDO = LoadGASBlueprintCDO<UGameplayAbility>(Context, Blueprint, TEXT("GameplayAbility"));
    if (!AbilityCDO)
    {
        return true;
    }

    // ---- Resolve EVERYTHING before writing ANYTHING. --------------------------------------------
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
    // The declared singular spellings win; the plural is only a legacy fallback.
    ResolveArray(TEXT("cancelAbilitiesWithTag"), TEXT("cancelAbilitiesWithTags"), FName(TEXT("CancelAbilitiesWithTag")));
    ResolveArray(TEXT("blockAbilitiesWithTag"), TEXT("blockAbilitiesWithTags"), FName(TEXT("BlockAbilitiesWithTag")));
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

    // ---- Write. ---------------------------------------------------------------------------------
    // AbilityTags is deprecated (5.5+) for direct use but is still the asset-tag field on every engine;
    // SetAssetTags only works in constructors.
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    for (const auto& Pair : AssetTagWrites)
    {
        AbilityCDO->AbilityTags.AddTag(Pair.Value);
    }
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
    for (const FReflectionTagWrite& Write : ReflectionWrites)
    {
        AddTagToAbilityContainer(AbilityCDO, Write.ContainerProp, Write.Tag);
    }

    // ---- Compile, VERIFY on the recompiled CDO, and only then save. -----------------------------
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
            PRAGMA_DISABLE_DEPRECATION_WARNINGS
            const FGameplayTagContainer PersistedAssetTags = CompiledCDO->AbilityTags;
            PRAGMA_ENABLE_DEPRECATION_WARNINGS
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

    // Every requested write must have been MEASURED -- verified or lost. If the compiled class or its
    // CDO was unavailable, both loops above are skipped, TagsLost stays empty, and a gate that only
    // looked at TagsLost would save and report success having verified nothing.
    const int32 RequestedWrites = AssetTagWrites.Num() + ReflectionWrites.Num();
    const int32 MeasuredWrites = TagsVerified.Num() + OtherTagsVerified.Num() + TagsLost.Num();
    const bool bAllMeasured = MeasuredWrites == RequestedWrites;
    if (!bCompiled || TagsLost.Num() > 0 || !bAllMeasured)
    {
        const FString Why = !bCompiled
            ? FString(GASCompileFailureNote(false))
            : (!bAllMeasured
                ? FString(TEXT(" (the compiled class or its default object was unavailable, so nothing could be read back)"))
                : FString());
        Bridge->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Ability tags did not persist onto the compiled class%s: %s. The asset was NOT saved."),
                *Why,
                TagsLost.Num() > 0 ? *FString::Join(TagsLost, TEXT(", ")) : TEXT("(no write could be verified)")),
            TEXT("ABILITY_TAGS_NOT_APPLIED"));
        return true;
    }

    if (!SaveVerifiedGASBlueprint(Context, Blueprint, TEXT("Ability tags")))
    {
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetArrayField(TEXT("tagsAdded"), McpHandlerUtils::ToJsonStringArray(TagsAdded));
    // Read back from the compiled class rather than echoing what we were asked to write.
    Result->SetArrayField(TEXT("tagsVerified"), McpHandlerUtils::ToJsonStringArray(TagsVerified));
    Result->SetArrayField(TEXT("otherTagsVerified"), McpHandlerUtils::ToJsonStringArray(OtherTagsVerified));
    Result->SetBoolField(TEXT("verifiedOnCompiledClass"), true);
    Result->SetBoolField(TEXT("savedToDisk"), true);
    Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ability tags set"), Result);
    return true;
}
}
