#include "Domains/AI/McpAIContext.h"
#include "Domains/AI/Perception/McpAIPerceptionConfigs.h"

#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"

namespace McpAIHandlers
{
// Implements the "add_ai_perception_component" action. Every call added another component (CreateNode picked
// AIPerception1 while the reply said AIPerception) and none was saved; one already there is kept now.
bool HandleAddAIPerceptionComponent(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!Blueprint)
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
                            TEXT("NOT_FOUND"));
        return true;
    }

    bool bCreated = false;
    UAIPerceptionComponent* PerceptionComp = FindOrCreatePerceptionComponent(Self, RequestId, RequestingSocket, Blueprint, &bCreated);
    if (!PerceptionComp)
    {
        return true;
    }
    FString ComponentName;
    for (const USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->ComponentTemplate == PerceptionComp)
        {
            ComponentName = Node->GetVariableName().ToString();
        }
    }
    if (bCreated)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        Result->SetBoolField(TEXT("saved"), McpSafeAssetSave(Blueprint));
    }
    else
    {
        McpHandlerUtils::MarkNoAssetsChanged(Result);
    }

    Result->SetStringField(TEXT("componentName"), ComponentName);
    Result->SetBoolField(TEXT("alreadyExists"), !bCreated);
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true,
        bCreated ? TEXT("Perception component added") : TEXT("The Blueprint already has a perception component; nothing changed"), Result);
    return true;
}

// Implements the "configure_sight_config" action.
bool HandleConfigureSightConfig(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!Blueprint)
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
                            TEXT("NOT_FOUND"));
        return true;
    }

    UAIPerceptionComponent* PerceptionComp = FindOrCreatePerceptionComponent(
        Self, RequestId, RequestingSocket, Blueprint, nullptr);
    if (!PerceptionComp)
    {
        return true;
    }

    // Only the sightConfig radii and angle were read; its offsets, maxAge and affiliation were dropped.
    bool bCreated = false;
    PerceptionComp->Modify();
    UAISenseConfig_Sight* SightConfig = FindOrAddSenseConfig<UAISenseConfig_Sight>(PerceptionComp, bCreated);
    ApplySightSettings(SightConfig, MergedSenseSettings(Payload, TEXT("sightConfig")), bCreated);
    PerceptionComp->ConfigureSense(*SightConfig);

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    Result->SetBoolField(TEXT("createdSense"), bCreated);
    AddSenseSettings(SightConfig, Result);
    Result->SetBoolField(TEXT("saved"), McpSafeAssetSave(Blueprint));
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Sight config set"), Result);
    return true;
}
}
