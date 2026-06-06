#include "McpAutomationBridge_GASAbilityReflection.h"
#include "McpAutomationBridge_GASAssetValidation.h"
#include "McpAutomationBridge_GASPayloadFields.h"
#include "McpAutomationBridge_GASRequestContext.h"
#include "McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR && MCP_HAS_GAS
#include "AbilitySystemComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "GameplayCueNotify_Actor.h"
#include "GameplayCueNotify_Static.h"
#include "GameplayEffect.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

#if WITH_EDITOR && MCP_HAS_GAS
namespace McpGASHandlers
{
bool HandleGASTagAssets(const FGASRequestContext& Context, const FString& SubAction)
{
    UMcpAutomationBridgeSubsystem* Bridge = Context.Subsystem;
    const FString& RequestId = Context.RequestId;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    const FString& Name = Context.Name;
    const FString& Path = Context.Path;
    const FString& BlueprintPath = Context.BlueprintPath;
    const FString& AssetPath = Context.AssetPath;

    if (SubAction == TEXT("add_tag_to_asset"))
    {
        if (AssetPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing assetPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString TagString = GetGASStringFieldWithFallback(Payload, TEXT("tag"), TEXT("tagName"));
        if (TagString.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing tag."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FGameplayTag Tag = GetOrRequestTag(TagString);
        const bool bTagIsRegistered = Tag.IsValid();

        auto AddLooseTagVariable = [&TagString](UBlueprint* Blueprint) -> bool
        {
            if (!Blueprint)
            {
                return false;
            }

            FString VariableName = FString::Printf(TEXT("MCPGameplayTag_%s"), *TagString);
            VariableName = SanitizeAssetName(VariableName).Left(64);

            FEdGraphPinType StringPinType;
            StringPinType.PinCategory = UEdGraphSchema_K2::PC_String;

            bool bHasVariable = false;
            for (const FBPVariableDescription& VarDesc : Blueprint->NewVariables)
            {
                if (VarDesc.VarName == FName(*VariableName))
                {
                    bHasVariable = true;
                    break;
                }
            }

            if (!bHasVariable)
            {
                FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*VariableName), StringPinType);
            }

            for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
            {
                if (VarDesc.VarName == FName(*VariableName))
                {
                    VarDesc.DefaultValue = TagString;
                    VarDesc.Category = FText::FromString(TEXT("Gameplay Tags"));
                    break;
                }
            }

            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
            McpSafeCompileBlueprint(Blueprint);
            McpSafeAssetSave(Blueprint);
            return true;
        };

        // Load the asset
        UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
        if (!Asset)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Asset not found: %s"), *AssetPath), TEXT("NOT_FOUND"));
            return true;
        }

        FString AssetType = TEXT("Unknown");
        bool bTagAdded = false;

        // Check if it's a Blueprint asset
        UBlueprint* Blueprint = Cast<UBlueprint>(Asset);
        if (Blueprint && Blueprint->GeneratedClass)
        {
            UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();

            // Try GameplayAbility
            if (UGameplayAbility* AbilityCDO = Cast<UGameplayAbility>(CDO))
            {
                if (bTagIsRegistered)
                {
                    // AbilityTags is deprecated in UE 5.5+, suppress warning unconditionally
                    PRAGMA_DISABLE_DEPRECATION_WARNINGS
                    AbilityCDO->AbilityTags.AddTag(Tag);
                    PRAGMA_ENABLE_DEPRECATION_WARNINGS
                }
                else
                {
                    AddLooseTagVariable(Blueprint);
                }
                AssetType = TEXT("GameplayAbility");
                bTagAdded = true;
                FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
                McpSafeAssetSave(Blueprint);
            }
            // Try GameplayEffect
            else if (UGameplayEffect* EffectCDO = Cast<UGameplayEffect>(CDO))
            {
                if (bTagIsRegistered)
                {
                    // InheritableOwnedTagsContainer is deprecated, suppress warning unconditionally
                    PRAGMA_DISABLE_DEPRECATION_WARNINGS
                    EffectCDO->InheritableOwnedTagsContainer.AddTag(Tag);
                    PRAGMA_ENABLE_DEPRECATION_WARNINGS
                }
                else
                {
                    AddLooseTagVariable(Blueprint);
                }
                AssetType = TEXT("GameplayEffect");
                bTagAdded = true;
                FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
                McpSafeAssetSave(Blueprint);
            }
            // Try GameplayCue Notify (Static)
            else if (UGameplayCueNotify_Static* CueStaticCDO = Cast<UGameplayCueNotify_Static>(CDO))
            {
                if (bTagIsRegistered)
                {
                    CueStaticCDO->GameplayCueTag = Tag;
                }
                else
                {
                    AddLooseTagVariable(Blueprint);
                }
                AssetType = TEXT("GameplayCueNotify_Static");
                bTagAdded = true;
                FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
                McpSafeAssetSave(Blueprint);
            }
            // Try GameplayCue Notify (Actor)
            else if (AGameplayCueNotify_Actor* CueActorCDO = Cast<AGameplayCueNotify_Actor>(CDO))
            {
                if (bTagIsRegistered)
                {
                    CueActorCDO->GameplayCueTag = Tag;
                }
                else
                {
                    AddLooseTagVariable(Blueprint);
                }
                AssetType = TEXT("GameplayCueNotify_Actor");
                bTagAdded = true;
                FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
                McpSafeAssetSave(Blueprint);
            }
            // Try Actor with AbilitySystemComponent
            else if (AActor* ActorCDO = Cast<AActor>(CDO))
            {
                // Look for ASC on the actor's component list in SCS
                if (Blueprint->SimpleConstructionScript)
                {
                    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
                    {
                        if (Node && Node->ComponentTemplate)
                        {
                            if (UAbilitySystemComponent* ASC = Cast<UAbilitySystemComponent>(Node->ComponentTemplate))
                            {
                                // ASC doesn't have a direct tag container on CDO, but we can add OwnedTags
                                // For actors, we'll add a gameplay tag variable instead
                                FEdGraphPinType TagContainerPinType;
                                TagContainerPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
                                TagContainerPinType.PinSubCategoryObject = FGameplayTagContainer::StaticStruct();

                                // Check if OwnedGameplayTags variable exists, if not create it
                                bool bHasTagVar = false;
                                for (const FBPVariableDescription& VarDesc : Blueprint->NewVariables)
                                {
                                    if (VarDesc.VarName == TEXT("OwnedGameplayTags"))
                                    {
                                        bHasTagVar = true;
                                        break;
                                    }
                                }

                                if (!bHasTagVar)
                                {
                                    FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("OwnedGameplayTags"), TagContainerPinType);
                                }

                                AssetType = TEXT("Actor with ASC");
                                bTagAdded = true;
                                FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
                                McpSafeAssetSave(Blueprint);
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (!bTagAdded)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                TEXT("Asset is not a supported GAS type (GameplayAbility, GameplayEffect, GameplayCue, or Actor with ASC)"),
                TEXT("UNSUPPORTED_TYPE"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("assetPath"), AssetPath);
        Result->SetStringField(TEXT("tag"), TagString);
        Result->SetStringField(TEXT("assetType"), AssetType);
        Result->SetBoolField(TEXT("tagValid"), bTagIsRegistered);
        Result->SetBoolField(TEXT("tagAdded"), bTagAdded);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Tag added to asset"), Result);
        return true;
    }

    // ============================================================
    // 13.5 UTILITY
    // ============================================================

    // get_gas_info

    return false;
}
}
#endif
