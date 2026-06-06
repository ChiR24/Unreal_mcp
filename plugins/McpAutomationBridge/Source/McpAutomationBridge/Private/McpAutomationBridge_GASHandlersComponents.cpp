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
#include "Kismet2/BlueprintEditorUtils.h"
#endif

#if WITH_EDITOR && MCP_HAS_GAS
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

        FString ComponentName = GetStringFieldGAS(Payload, TEXT("componentName"), TEXT("AbilitySystemComponent"));

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

        FString ComponentName = GetStringFieldGAS(Payload, TEXT("componentName"), TEXT("AbilitySystemComponent"));
        FString ReplicationMode = GetStringFieldGAS(Payload, TEXT("replicationMode"), TEXT("Full"));
        const FString ReplicationModeToken = NormalizeGASToken(ReplicationMode);

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

        // Configure replication mode
        if (ReplicationModeToken == TEXT("full"))
        {
            ASCTemplate->SetReplicationMode(EGameplayEffectReplicationMode::Full);
        }
        else if (ReplicationModeToken == TEXT("mixed"))
        {
            ASCTemplate->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
        }
        else if (ReplicationModeToken == TEXT("minimal"))
        {
            ASCTemplate->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("componentName"), ComponentName);
        Result->SetStringField(TEXT("replicationMode"), ReplicationMode);
        McpHandlerUtils::AddVerification(Result, Blueprint);
        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("ASC configured"), Result);
        return true;
    }

    // create_attribute_set

    return false;
}
}
#endif
