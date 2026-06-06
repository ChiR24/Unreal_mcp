#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SplineHandlersPrivate.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Components/SplineMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/StaticMesh.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Materials/MaterialInterface.h"

bool HandleCreateSplineMeshComponent(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString BlueprintPath = GetJsonStringFieldSpline(Payload, TEXT("blueprintPath"));
    FString ComponentName = GetJsonStringFieldSpline(Payload, TEXT("componentName"), TEXT("SplineMesh"));
    FString MeshPath = GetJsonStringFieldSpline(Payload, TEXT("meshPath"));
    FString ForwardAxis = GetJsonStringFieldSpline(Payload, TEXT("forwardAxis"), TEXT("X"));

    if (BlueprintPath.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("blueprintPath is required"), nullptr, TEXT("MISSING_PARAM"));
        return true;
    }

    FString SafeBlueprintPath = SanitizeProjectRelativePath(BlueprintPath);
    if (SafeBlueprintPath.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Invalid or unsafe blueprintPath: %s. Path must be relative to project (e.g., /Game/...)"), *BlueprintPath),
            nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }

    FString SafeMeshPath;
    if (!MeshPath.IsEmpty())
    {
        SafeMeshPath = SanitizeProjectRelativePath(MeshPath);
        if (SafeMeshPath.IsEmpty())
        {
            Self->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("Invalid or unsafe meshPath: %s. Path must be relative to project (e.g., /Game/...)"), *MeshPath),
                nullptr, TEXT("SECURITY_VIOLATION"));
            return true;
        }
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *SafeBlueprintPath);
    if (!Blueprint)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (!SCS)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Blueprint has no SimpleConstructionScript"), nullptr, TEXT("INVALID_BP"));
        return true;
    }

    for (USCS_Node* Node : SCS->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            Self->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("Component '%s' already exists"), *ComponentName), nullptr, TEXT("ALREADY_EXISTS"));
            return true;
        }
    }

    USCS_Node* NewNode = SCS->CreateNode(USplineMeshComponent::StaticClass(), *ComponentName);
    if (!NewNode)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to create SCS node"), nullptr, TEXT("CREATE_FAILED"));
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
                Self->SendAutomationResponse(Socket, RequestId, false,
                    FString::Printf(TEXT("Mesh not found: %s"), *SafeMeshPath), nullptr, TEXT("MESH_NOT_FOUND"));
                return true;
            }
            MeshComp->SetStaticMesh(Mesh);
        }

        ESplineMeshAxis::Type Axis = ESplineMeshAxis::X;
        if (ForwardAxis == TEXT("Y")) Axis = ESplineMeshAxis::Y;
        else if (ForwardAxis == TEXT("Z")) Axis = ESplineMeshAxis::Z;
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

    if (GetJsonBoolFieldSpline(Payload, TEXT("save"), false))
    {
        McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("componentName"), ComponentName);
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetBoolField(TEXT("existsAfter"), true);
    Result->SetStringField(TEXT("action"), TEXT("manage_splines:component_added"));

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("SplineMeshComponent '%s' added to Blueprint"), *ComponentName), Result);
    return true;
}
#endif
