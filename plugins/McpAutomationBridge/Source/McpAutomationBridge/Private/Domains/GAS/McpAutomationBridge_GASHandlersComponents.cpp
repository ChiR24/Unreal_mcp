#include "Domains/GAS/McpAutomationBridge_GASPayloadFields.h"
#include "Domains/GAS/McpAutomationBridge_GASRequestContext.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Safety/McpSafeOperations.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "AbilitySystemComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace McpGASHandlers
{
bool HandleGASComponents(const FGASRequestContext& Context, const FString& SubAction)
{
    UMcpAutomationBridgeSubsystem* Bridge = Context.Subsystem;
    const FString& RequestId = Context.RequestId;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    const FString& Name = Context.Name;
    const FString& Path = Context.Path;
    const FString& BlueprintPath = Context.BlueprintPath;
    const FString& AssetPath = Context.AssetPath;

    if (SubAction == TEXT("add_ability_system_component"))
    {
        if (BlueprintPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"), TEXT("AbilitySystemComponent"));

        USCS_Node* NewNode = Blueprint->SimpleConstructionScript->CreateNode(
            UAbilitySystemComponent::StaticClass(), FName(*ComponentName));

        if (!NewNode)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                TEXT("Failed to create ASC node"), TEXT("CREATION_FAILED"));
            return true;
        }

        Blueprint->SimpleConstructionScript->AddNode(NewNode);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("componentName"), ComponentName);
        Result->SetStringField(TEXT("componentClass"), TEXT("AbilitySystemComponent"));
        McpHandlerUtils::AddVerification(Result, Blueprint);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("ASC added"), Result);
        return true;
    }

    // configure_asc
    if (SubAction == TEXT("configure_asc"))
    {
        if (BlueprintPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"), TEXT("AbilitySystemComponent"));
        FString ReplicationMode = GetJsonStringField(Payload, TEXT("replicationMode"), TEXT("Full"));

        // Find ASC in SCS
        UAbilitySystemComponent* ASCTemplate = nullptr;
        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (Node && Node->ComponentTemplate &&
                Node->ComponentTemplate->IsA<UAbilitySystemComponent>())
            {
                if (Node->GetVariableName().ToString() == ComponentName)
                {
                    ASCTemplate = Cast<UAbilitySystemComponent>(Node->ComponentTemplate);
                    break;
                }
            }
        }

        if (!ASCTemplate)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("ASC not found: %s"), *ComponentName), TEXT("NOT_FOUND"));
            return true;
        }

        // Full / Mixed / Minimal. An unknown mode used to leave the template alone while it was echoed
        // back as applied, and the edit was never saved.
        EGameplayEffectReplicationMode Mode = EGameplayEffectReplicationMode::Full;
        if (!TryParseGASEnum(ReplicationMode, Mode))
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Unknown replicationMode '%s'; use Full, Mixed or Minimal."), *ReplicationMode), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        ASCTemplate->Modify();
        ASCTemplate->SetReplicationMode(Mode);
        if (!CommitGASBlueprintEdit(Context, Blueprint, TEXT("ASC replication mode")))
        {
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("componentName"), ComponentName);
        Result->SetStringField(TEXT("replicationMode"), GASEnumName(Mode));
        McpHandlerUtils::AddVerification(Result, Blueprint);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("ASC configured"), Result);
        return true;
    }

    return false;
}
}
