#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_MiscHandlersSupport.h"
#include "McpHandlerUtils.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "Components/SplineComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"

#if WITH_EDITOR
namespace McpMiscHandlers
{
bool HandleCreateSplineComponent(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"), TEXT(""));
    FString ComponentName = GetStringField(Payload, TEXT("componentName"), TEXT("SplineComponent"));
    bool bClosedLoop = GetBoolField(Payload, TEXT("closedLoop"), false);

    if (BlueprintPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("blueprintPath is required"), nullptr, TEXT("INVALID_PARAMS"));
        return true;
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!Blueprint)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (!SCS)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("Blueprint has no SimpleConstructionScript"), nullptr, TEXT("INVALID_BP"));
        return true;
    }

    for (USCS_Node* Node : SCS->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("Component '%s' already exists"), *ComponentName), nullptr, TEXT("ALREADY_EXISTS"));
            return true;
        }
    }

    USCS_Node* NewNode = SCS->CreateNode(USplineComponent::StaticClass(), *ComponentName);
    if (!NewNode)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("Failed to create SCS node"), nullptr, TEXT("CREATE_FAILED"));
        return true;
    }

    USplineComponent* SplineComp = Cast<USplineComponent>(NewNode->ComponentTemplate);
    if (SplineComp)
    {
        SplineComp->SetClosedLoop(bClosedLoop);
    }

    SCS->AddNode(NewNode);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    if (GetBoolField(Payload, TEXT("save"), false))
    {
        McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetStringField(TEXT("componentName"), ComponentName);
    ResponseJson->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    ResponseJson->SetBoolField(TEXT("closedLoop"), bClosedLoop);
    McpHandlerUtils::AddVerification(ResponseJson, Blueprint);

    Subsystem->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("SplineComponent '%s' added to Blueprint"), *ComponentName), ResponseJson);
    return true;
}
}
#endif
