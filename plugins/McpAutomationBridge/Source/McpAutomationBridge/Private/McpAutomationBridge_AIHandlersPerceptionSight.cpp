#include "McpAutomationBridge_AIHandlerContext.h"

#if WITH_EDITOR
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"

namespace McpAIHandlers
{
bool HandleAddAIPerceptionComponent(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("add_ai_perception_component");
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (SubAction == TEXT("add_ai_perception_component"))
    {
        FString BlueprintPath = GetStringFieldAI(Payload, TEXT("blueprintPath"));

        // CRITICAL: Explicitly check if asset exists before LoadObject
        // LoadObject may return non-null for invalid paths due to UE's path resolution behavior
        if (!UEditorAssetLibrary::DoesAssetExist(BlueprintPath))
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
                                TEXT("NOT_FOUND"));
            return true;
        }

        USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
        if (!SCS)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                                TEXT("Blueprint has no SimpleConstructionScript"),
                                TEXT("INVALID_BLUEPRINT"));
            return true;
        }

        // Create perception component
        USCS_Node* NewNode = SCS->CreateNode(UAIPerceptionComponent::StaticClass(), TEXT("AIPerception"));
        if (NewNode)
        {
            SCS->AddNode(NewNode);
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

            Result->SetStringField(TEXT("componentName"), TEXT("AIPerception"));
            Result->SetStringField(TEXT("message"), TEXT("AI Perception component added"));
            McpHandlerUtils::AddVerification(Result, Blueprint);
            Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Perception component added"), Result);
        }
        else
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                                TEXT("Failed to create AI Perception component"),
                                TEXT("CREATION_FAILED"));
        }

        return true;
    }

    return true;
}

bool HandleConfigureSightConfig(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("configure_sight_config");
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (SubAction == TEXT("configure_sight_config"))
    {
        FString BlueprintPath = GetStringFieldAI(Payload, TEXT("blueprintPath"));

        // CRITICAL: Explicitly check if asset exists before LoadObject
        // LoadObject may return non-null for invalid paths due to UE's path resolution behavior
        if (!UEditorAssetLibrary::DoesAssetExist(BlueprintPath))
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
                                TEXT("NOT_FOUND"));
            return true;
        }

        // Get sight config parameters
        double SightRadius = GetNumberFieldAI(Payload, TEXT("sightRadius"), 3000.0);
        double LoseSightRadius = GetNumberFieldAI(Payload, TEXT("loseSightRadius"), SightRadius + 500.0);
        double PeripheralAngle = GetNumberFieldAI(Payload, TEXT("peripheralVisionAngle"), 90.0);
        const TSharedPtr<FJsonObject>* SightConfigObj = nullptr;
        if (Payload->TryGetObjectField(TEXT("sightConfig"), SightConfigObj) && SightConfigObj->IsValid())
        {
            SightRadius = GetNumberFieldAI(*SightConfigObj, TEXT("sightRadius"), SightRadius);
            LoseSightRadius = GetNumberFieldAI(*SightConfigObj, TEXT("loseSightRadius"), LoseSightRadius);
            PeripheralAngle = GetNumberFieldAI(*SightConfigObj, TEXT("peripheralVisionAngle"), PeripheralAngle);
        }

        if (!Blueprint->SimpleConstructionScript)
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint has no SimpleConstructionScript"), TEXT("INVALID_STATE"));
            return true;
        }

        UAIPerceptionComponent* PerceptionComp = nullptr;
        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (Node && Node->ComponentTemplate)
            {
                if (UAIPerceptionComponent* Comp = Cast<UAIPerceptionComponent>(Node->ComponentTemplate))
                {
                    PerceptionComp = Comp;
                    break;
                }
            }
        }

        if (!PerceptionComp)
        {
            USCS_Node* PerceptionNode = Blueprint->SimpleConstructionScript->CreateNode(
                UAIPerceptionComponent::StaticClass(), TEXT("AIPerceptionComponent"));
            if (!PerceptionNode)
            {
                Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create perception component node"), TEXT("CREATION_FAILED"));
                return true;
            }
            Blueprint->SimpleConstructionScript->AddNode(PerceptionNode);
            PerceptionComp = Cast<UAIPerceptionComponent>(PerceptionNode->ComponentTemplate);
        }

        if (!PerceptionComp)
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Perception component is null"), TEXT("NULL_COMPONENT"));
            return true;
        }

        UAISenseConfig_Sight* SightConfig = NewObject<UAISenseConfig_Sight>(PerceptionComp);
        SightConfig->SightRadius = SightRadius;
        SightConfig->LoseSightRadius = LoseSightRadius;
        SightConfig->PeripheralVisionAngleDegrees = PeripheralAngle;
        SightConfig->DetectionByAffiliation.bDetectEnemies = true;
        SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
        SightConfig->DetectionByAffiliation.bDetectFriendlies = false;
        SightConfig->SetMaxAge(5.0f);
        PerceptionComp->ConfigureSense(*SightConfig);

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeAssetSave(Blueprint);
        Result->SetNumberField(TEXT("sightRadius"), SightRadius);
        Result->SetNumberField(TEXT("loseSightRadius"), LoseSightRadius);
        Result->SetNumberField(TEXT("peripheralVisionAngle"), PeripheralAngle);
        Result->SetStringField(TEXT("message"), TEXT("Sight sense configured"));
        McpHandlerUtils::AddVerification(Result, Blueprint);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Sight config set"), Result);
        return true;
    }

    return true;
}
}
#endif
