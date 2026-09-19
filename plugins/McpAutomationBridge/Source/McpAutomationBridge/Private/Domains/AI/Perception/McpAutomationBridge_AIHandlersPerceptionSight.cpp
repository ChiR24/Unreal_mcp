#include "Domains/AI/McpAutomationBridge_AIHandlerContext.h"

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
// Implements the "add_ai_perception_component" action.
bool HandleAddAIPerceptionComponent(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));

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

// Implements the "configure_sight_config" action.
bool HandleConfigureSightConfig(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));

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
    double SightRadius = GetJsonNumberField(Payload, TEXT("sightRadius"), 3000.0);
    double LoseSightRadius = GetJsonNumberField(Payload, TEXT("loseSightRadius"), SightRadius + 500.0);
    double PeripheralAngle = GetJsonNumberField(Payload, TEXT("peripheralVisionAngle"), 90.0);
    const TSharedPtr<FJsonObject>* SightConfigObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("sightConfig"), SightConfigObj) && SightConfigObj->IsValid())
    {
        SightRadius = GetJsonNumberField(*SightConfigObj, TEXT("sightRadius"), SightRadius);
        LoseSightRadius = GetJsonNumberField(*SightConfigObj, TEXT("loseSightRadius"), LoseSightRadius);
        PeripheralAngle = GetJsonNumberField(*SightConfigObj, TEXT("peripheralVisionAngle"), PeripheralAngle);
    }

    UAIPerceptionComponent* PerceptionComp = FindOrCreatePerceptionComponent(
        Self, RequestId, RequestingSocket, Blueprint, nullptr);
    if (!PerceptionComp)
    {
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
}
#endif
