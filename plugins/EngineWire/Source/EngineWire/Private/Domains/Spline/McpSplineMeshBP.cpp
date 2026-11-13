#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Spline/McpSplinePrivate.h"

#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Components/SplineMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/StaticMesh.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Materials/MaterialInterface.h"

bool HandleCreateSplineMeshComponent(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"), TEXT("SplineMesh"));
    FString MeshPath = GetJsonStringField(Payload, TEXT("meshPath"));
    FString ForwardAxis = GetJsonStringField(Payload, TEXT("forwardAxis"), TEXT("X"));
    ESplineMeshAxis::Type Axis = ESplineMeshAxis::X;
    if (!ParseSplineMeshAxis(ForwardAxis, Axis))
    {
        return RefuseSplineMeshAxis(Self, RequestId, Socket, ForwardAxis);
    }

    const FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    if (BlueprintPath.IsEmpty() && !ActorName.IsEmpty())
    {
        return HandleCreateSplineMeshComponentOnActor(Self, RequestId, Payload, Socket, ActorName, ComponentName, MeshPath, ForwardAxis);
    }
    if (BlueprintPath.IsEmpty())
    {
        Self->SendAutomationError(Socket, RequestId,
            TEXT("blueprintPath (Blueprint route) or actorName (level actor route) is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    const FString SafeBlueprintPath = RequireSplineProjectPath(Self, RequestId, Socket, TEXT("blueprintPath"), BlueprintPath);
    if (SafeBlueprintPath.IsEmpty())
    {
        return true;
    }

    const FString SafeMeshPath = MeshPath.IsEmpty() ? FString() : RequireSplineProjectPath(Self, RequestId, Socket, TEXT("meshPath"), MeshPath);
    if (!MeshPath.IsEmpty() && SafeMeshPath.IsEmpty())
    {
        return true;
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *SafeBlueprintPath);
    if (!Blueprint)
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
        return true;
    }

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (!SCS)
    {
        Self->SendAutomationError(Socket, RequestId,
            TEXT("Blueprint has no SimpleConstructionScript"), TEXT("INVALID_BP"));
        return true;
    }

    for (USCS_Node* Node : SCS->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            Self->SendAutomationError(Socket, RequestId,
                FString::Printf(TEXT("Component '%s' already exists"), *ComponentName), TEXT("ALREADY_EXISTS"));
            return true;
        }
    }

    USCS_Node* NewNode = SCS->CreateNode(USplineMeshComponent::StaticClass(), *ComponentName);
    if (!NewNode)
    {
        Self->SendAutomationError(Socket, RequestId,
            TEXT("Failed to create SCS node"), TEXT("CREATE_FAILED"));
        return true;
    }

    USplineMeshComponent* MeshComp = Cast<USplineMeshComponent>(NewNode->ComponentTemplate);
    if (MeshComp)
    {
        if (!SafeMeshPath.IsEmpty())
        {
            UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *SafeMeshPath);
            if (!Mesh)
            {
                Self->SendAutomationError(Socket, RequestId,
                    FString::Printf(TEXT("Mesh not found: %s"), *SafeMeshPath), TEXT("MESH_NOT_FOUND"));
                return true;
            }
            MeshComp->SetStaticMesh(Mesh);
        }

        MeshComp->SetForwardAxis(Axis);

        if (MeshComp->GetMaterial(0) == nullptr)
        {
            UMaterialInterface* FallbackMaterial = McpLoadMaterialWithFallback(TEXT(""), true);
            if (FallbackMaterial)
            {
                MeshComp->SetMaterial(0, FallbackMaterial);
            }
        }
    }

    SCS->AddNode(NewNode);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    if (GetJsonBoolField(Payload, TEXT("save"), false))
    {
        McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("componentName"), ComponentName);
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetBoolField(TEXT("existsAfter"), true);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("SplineMeshComponent '%s' added to Blueprint"), *ComponentName), Result);
    return true;
}
