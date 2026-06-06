#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_MaterialGraphHandlersPrivate.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionParameter.h"

namespace McpMaterialGraphHandlers
{
bool HandleAddNode(
    UMcpAutomationBridgeSubsystem& Bridge,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    UMaterial& Material)
{
    if (SubAction != TEXT("add_node"))
    {
        return false;
    }

    FString NodeType;
    Payload->TryGetStringField(TEXT("nodeType"), NodeType);
    float X = 0.0f;
    float Y = 0.0f;
    Payload->TryGetNumberField(TEXT("x"), X);
    Payload->TryGetNumberField(TEXT("y"), Y);

    UClass* ExpressionClass = ResolveGraphNodeClass(NodeType, true);
    if (!ExpressionClass)
    {
        Bridge.SendAutomationError(
            Socket,
            RequestId,
            FString::Printf(
                TEXT("Unknown node type: %s. Available types: TextureSample, "
                     "VectorParameter, ScalarParameter, Add, Multiply, "
                     "Constant, Constant3Vector, Color, ConstantVectorParameter. "
                     "Or use full class name like 'MaterialExpressionLerp'."),
                *NodeType),
            TEXT("UNKNOWN_TYPE"));
        return true;
    }

    UMaterialExpression* NewExpr =
        NewObject<UMaterialExpression>(&Material, ExpressionClass, NAME_None, RF_Transactional);
    if (!NewExpr)
    {
        Bridge.SendAutomationError(Socket, RequestId, TEXT("Failed to create expression."), TEXT("CREATE_FAILED"));
        return true;
    }

    NewExpr->MaterialExpressionEditorX = (int32)X;
    NewExpr->MaterialExpressionEditorY = (int32)Y;
    AddExpressionToMaterial(Material, *NewExpr);

    FString ParamName;
    if (Payload->TryGetStringField(TEXT("name"), ParamName))
    {
        if (UMaterialExpressionParameter* ParamExpr = Cast<UMaterialExpressionParameter>(NewExpr))
        {
            ParamExpr->ParameterName = FName(*ParamName);
        }
    }

    Material.PostEditChange();
    Material.MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, &Material);
    Result->SetStringField(TEXT("nodeId"), NewExpr->GetName());
    Result->SetStringField(TEXT("nodeType"), ExpressionClass->GetName());
    Bridge.SendAutomationResponse(Socket, RequestId, true, TEXT("Node added."), Result);
    return true;
}

bool HandleRemoveNode(
    UMcpAutomationBridgeSubsystem& Bridge,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    UMaterial& Material)
{
    if (SubAction != TEXT("remove_node"))
    {
        return false;
    }

    FString NodeId;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    int32 ExpressionIndex = -1;
    Payload->TryGetNumberField(TEXT("expressionIndex"), ExpressionIndex);

    if (NodeId.IsEmpty() && ExpressionIndex < 0)
    {
        Bridge.SendAutomationError(Socket, RequestId, TEXT("Missing 'nodeId' or 'expressionIndex'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UMaterialExpression* TargetExpr = FindExpressionByIdOrNameOrIndex(Material, NodeId, ExpressionIndex);
    if (!TargetExpr)
    {
        Bridge.SendAutomationError(Socket, RequestId, TEXT("Node not found."), TEXT("NODE_NOT_FOUND"));
        return true;
    }

    const FString RemovedNodeId = TargetExpr->GetName();
#if WITH_EDITORONLY_DATA
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    if (Material.GetEditorOnlyData())
    {
        MCP_GET_MATERIAL_EXPRESSIONS((&Material)).Remove(TargetExpr);
    }
#else
    Material.Expressions.Remove(TargetExpr);
#endif
#endif

    Material.PostEditChange();
    Material.MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, &Material);
    Result->SetStringField(TEXT("nodeId"), RemovedNodeId);
    Result->SetBoolField(TEXT("removed"), true);
    Bridge.SendAutomationResponse(Socket, RequestId, true, TEXT("Node removed."), Result);
    return true;
}
}
#endif
