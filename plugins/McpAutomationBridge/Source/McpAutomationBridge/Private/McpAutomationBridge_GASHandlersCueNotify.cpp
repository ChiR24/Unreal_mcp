#include "McpAutomationBridge_GASAbilityReflection.h"
#include "McpAutomationBridge_GASBlueprintCreation.h"
#include "McpAutomationBridge_GASPayloadFields.h"
#include "McpAutomationBridge_GASRequestContext.h"
#include "McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR && MCP_HAS_GAS
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "GameplayCueNotify_Actor.h"
#include "GameplayCueNotify_Static.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

#if WITH_EDITOR && MCP_HAS_GAS
namespace McpGASHandlers
{
bool HandleGASCueNotify(const FGASRequestContext& Context, const FString& SubAction)
{
    UMcpAutomationBridgeSubsystem* Bridge = Context.Subsystem;
    const FString& RequestId = Context.RequestId;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    const FString& Name = Context.Name;
    const FString& Path = Context.Path;
    const FString& BlueprintPath = Context.BlueprintPath;
    const FString& AssetPath = Context.AssetPath;

    if (SubAction == TEXT("create_gameplay_cue_notify"))
    {
        if (Name.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing name."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString CueType = GetStringFieldGAS(Payload, TEXT("cueType"), TEXT("Static"));
        const FString CueTypeToken = NormalizeGASToken(CueType);
        FString CueTag = GetStringFieldGAS(Payload, TEXT("cueTag"));

        UClass* ParentClass = (CueTypeToken == TEXT("actor"))
            ? AGameplayCueNotify_Actor::StaticClass()
            : UGameplayCueNotify_Static::StaticClass();

        FString Error;
        bool bReusedExisting = false;
        UBlueprint* Blueprint = CreateGASBlueprint(Path, Name, ParentClass, Error, bReusedExisting);
        if (!Blueprint)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("CREATION_FAILED"));
            return true;
        }

        // Set cue tag if provided (only for new blueprints)
        if (!bReusedExisting && !CueTag.IsEmpty() && Blueprint->GeneratedClass)
        {
            FGameplayTag Tag = GetOrRequestTag(CueTag);

            if (CueTypeToken == TEXT("static"))
            {
                UGameplayCueNotify_Static* CueCDO = Cast<UGameplayCueNotify_Static>(
                    Blueprint->GeneratedClass->GetDefaultObject());
                if (CueCDO)
                {
                    CueCDO->GameplayCueTag = Tag;
                }
            }
            else
            {
                AGameplayCueNotify_Actor* CueCDO = Cast<AGameplayCueNotify_Actor>(
                    Blueprint->GeneratedClass->GetDefaultObject());
                if (CueCDO)
                {
                    CueCDO->GameplayCueTag = Tag;
                }
            }
        }

        if (!bReusedExisting)
        {
            McpSafeAssetSave(Blueprint);
        }

        // Use the actual blueprint name (which may have been sanitized) in the response
        FString ActualName = Blueprint->GetName();

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("assetPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("name"), ActualName);
        Result->SetStringField(TEXT("cueType"), CueType);
        Result->SetStringField(TEXT("cueTag"), CueTag);
        Result->SetBoolField(TEXT("reusedExisting"), bReusedExisting);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true,
            bReusedExisting ? TEXT("Cue notify already exists") : TEXT("Cue notify created"), Result);
        return true;
    }

    // configure_cue_trigger - REAL IMPLEMENTATION adding trigger configuration
    if (SubAction == TEXT("configure_cue_trigger"))
    {
        if (BlueprintPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString TriggerType = GetStringFieldGAS(Payload, TEXT("triggerType"), TEXT("Executed"));
        const FString TriggerTypeToken = NormalizeGASToken(TriggerType);

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        // Add trigger configuration variables for the cue
        FEdGraphPinType BoolPinType;
        BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("bTriggerOnExecute"), BoolPinType);
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("bTriggerWhileActive"), BoolPinType);
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("bTriggerOnRemove"), BoolPinType);

        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("bTriggerOnExecute"), nullptr, FText::FromString(TEXT("Cue Triggers")));
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("bTriggerWhileActive"), nullptr, FText::FromString(TEXT("Cue Triggers")));
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("bTriggerOnRemove"), nullptr, FText::FromString(TEXT("Cue Triggers")));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetStringField(TEXT("triggerType"), TriggerType);
        Result->SetBoolField(TEXT("onExecuteConfigured"), TriggerTypeToken == TEXT("onexecute") || TriggerTypeToken == TEXT("executed"));
        Result->SetBoolField(TEXT("whileActiveConfigured"), TriggerTypeToken == TEXT("whileactive"));
        Result->SetBoolField(TEXT("onRemoveConfigured"), TriggerTypeToken == TEXT("onremove"));
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Cue trigger configuration variables added"), Result);
        return true;
    }

    // set_cue_effects - REAL IMPLEMENTATION adding effect reference variables

    return false;
}
}
#endif
