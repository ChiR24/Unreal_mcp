#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_MaterialGraphHandlersPrivate.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"

namespace McpMaterialGraphHandlers
{
bool HandleGetNodeDetails(
    UMcpAutomationBridgeSubsystem& Bridge,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    UMaterial& Material)
{
    if (SubAction != TEXT("get_node_details"))
    {
        return false;
    }

    FString NodeId;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);

    int32 ExpressionIndex = -1;
    Payload->TryGetNumberField(TEXT("expressionIndex"), ExpressionIndex);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    auto AllExpressions = Material.GetExpressions();
#else
    auto& AllExpressions = Material.Expressions;
#endif

    UMaterialExpression* TargetExpr = nullptr;
    if (ExpressionIndex >= 0)
    {
        TargetExpr = FindExpressionByIdOrNameOrIndex(Material, FString(), ExpressionIndex);
    }
    else if (!NodeId.IsEmpty())
    {
        TargetExpr = FindExpressionByIdOrNameOrIndex(Material, NodeId);
    }

    if (TargetExpr)
    {
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        McpHandlerUtils::AddVerification(Result, &Material);
        Result->SetStringField(TEXT("nodeType"), TargetExpr->GetClass()->GetName());
        Result->SetStringField(TEXT("desc"), TargetExpr->Desc);
        Result->SetNumberField(TEXT("x"), TargetExpr->MaterialExpressionEditorX);
        Result->SetNumberField(TEXT("y"), TargetExpr->MaterialExpressionEditorY);
        Bridge.SendAutomationResponse(Socket, RequestId, true, TEXT("Node details retrieved."), Result);
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    TArray<TSharedPtr<FJsonValue>> NodeList;
    for (int32 Index = 0; Index < AllExpressions.Num(); ++Index)
    {
        UMaterialExpression* Expr = AllExpressions[Index];
        TSharedPtr<FJsonObject> NodeInfo = McpHandlerUtils::CreateResultObject();
        NodeInfo->SetStringField(TEXT("nodeId"), Expr->GetName());
        NodeInfo->SetStringField(TEXT("nodeType"), Expr->GetClass()->GetName());
        NodeInfo->SetNumberField(TEXT("index"), Index);
        if (!Expr->Desc.IsEmpty())
        {
            NodeInfo->SetStringField(TEXT("desc"), Expr->Desc);
        }
        NodeList.Add(MakeShared<FJsonValueObject>(NodeInfo));
    }

    Result->SetArrayField(TEXT("availableNodes"), NodeList);
    Result->SetNumberField(TEXT("nodeCount"), AllExpressions.Num());

    if (NodeId.IsEmpty() && ExpressionIndex < 0)
    {
        Bridge.SendAutomationResponse(
            Socket,
            RequestId,
            true,
            FString::Printf(TEXT("Material has %d nodes. Available nodes listed."), AllExpressions.Num()),
            Result);
        return true;
    }

    Bridge.SendAutomationResponse(
        Socket,
        RequestId,
        false,
        FString::Printf(
            TEXT("Node '%s' not found. Material has %d nodes."),
            NodeId.IsEmpty() ? *FString::FromInt(ExpressionIndex) : *NodeId,
            AllExpressions.Num()),
        Result,
        TEXT("NODE_NOT_FOUND"));
    return true;
}
}
#endif
