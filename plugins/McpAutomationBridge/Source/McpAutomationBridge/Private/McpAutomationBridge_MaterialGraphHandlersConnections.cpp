#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_MaterialGraphHandlersPrivate.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"
#include "UObject/UnrealType.h"

#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"

namespace McpMaterialGraphHandlers
{
bool HandleConnectNodes(
    UMcpAutomationBridgeSubsystem& Bridge,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    UMaterial& Material)
{
    if (SubAction != TEXT("connect_nodes") && SubAction != TEXT("connect_pins"))
    {
        return false;
    }

    FString SourceNodeId;
    FString TargetNodeId;
    FString InputName;
    Payload->TryGetStringField(TEXT("sourceNodeId"), SourceNodeId);
    Payload->TryGetStringField(TEXT("targetNodeId"), TargetNodeId);
    Payload->TryGetStringField(TEXT("inputName"), InputName);
    if (SourceNodeId.IsEmpty()) { Payload->TryGetStringField(TEXT("fromExpression"), SourceNodeId); }
    if (TargetNodeId.IsEmpty()) { Payload->TryGetStringField(TEXT("toExpression"), TargetNodeId); }

    int32 SourceIndex = -1;
    int32 TargetIndex = -1;
    Payload->TryGetNumberField(TEXT("fromExpression"), SourceIndex);
    Payload->TryGetNumberField(TEXT("toExpression"), TargetIndex);

    UMaterialExpression* SourceExpr = SourceIndex >= 0
        ? FindExpressionByIdOrNameOrIndex(Material, FString(), SourceIndex)
        : FindExpressionByIdOrNameOrIndex(Material, SourceNodeId);
    if (!SourceExpr)
    {
        Bridge.SendAutomationError(Socket, RequestId, TEXT("Source node not found."), TEXT("NODE_NOT_FOUND"));
        return true;
    }

    if ((TargetNodeId.IsEmpty() || TargetNodeId == TEXT("Main")) && TargetIndex < 0)
    {
        if (!SetMainMaterialInputExpression(Material, InputName, SourceExpr))
        {
            Bridge.SendAutomationError(
                Socket,
                RequestId,
                FString::Printf(TEXT("Unknown input on main node: %s"), *InputName),
                TEXT("INVALID_PIN"));
            return true;
        }

        Material.PostEditChange();
        Material.MarkPackageDirty();

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        McpHandlerUtils::AddVerification(Result, &Material);
        Result->SetStringField(TEXT("inputName"), InputName);
        Bridge.SendAutomationResponse(Socket, RequestId, true, TEXT("Connected to main material node."), Result);
        return true;
    }

    UMaterialExpression* TargetExpr = TargetIndex >= 0
        ? FindExpressionByIdOrNameOrIndex(Material, FString(), TargetIndex)
        : FindExpressionByIdOrNameOrIndex(Material, TargetNodeId);
    if (!TargetExpr)
    {
        Bridge.SendAutomationError(Socket, RequestId, TEXT("Target node not found."), TEXT("NODE_NOT_FOUND"));
        return true;
    }

    FProperty* Prop = TargetExpr->GetClass()->FindPropertyByName(FName(*InputName));
    if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
    {
        if (StructProp->Struct->GetFName() == FName("ExpressionInput"))
        {
            if (FExpressionInput* InputPtr = StructProp->ContainerPtrToValuePtr<FExpressionInput>(TargetExpr))
            {
                InputPtr->Expression = SourceExpr;
                Material.PostEditChange();
                Material.MarkPackageDirty();

                TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
                McpHandlerUtils::AddVerification(Result, &Material);
                Result->SetStringField(TEXT("inputName"), InputName);
                Bridge.SendAutomationResponse(Socket, RequestId, true, TEXT("Nodes connected."), Result);
                return true;
            }
        }
    }

    Bridge.SendAutomationError(
        Socket,
        RequestId,
        FString::Printf(TEXT("Input pin '%s' not found or not compatible."), *InputName),
        TEXT("PIN_NOT_FOUND"));
    return true;
}

bool HandleBreakConnections(
    UMcpAutomationBridgeSubsystem& Bridge,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    UMaterial& Material)
{
    if (SubAction != TEXT("break_connections"))
    {
        return false;
    }

    FString NodeId;
    FString PinName;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    Payload->TryGetStringField(TEXT("pinName"), PinName);

    if ((NodeId.IsEmpty() || NodeId == TEXT("Main")) && !PinName.IsEmpty())
    {
        if (SetMainMaterialInputExpression(Material, PinName, nullptr))
        {
            Material.PostEditChange();
            Material.MarkPackageDirty();

            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            McpHandlerUtils::AddVerification(Result, &Material);
            Result->SetStringField(TEXT("pinName"), PinName);
            Bridge.SendAutomationResponse(Socket, RequestId, true, TEXT("Disconnected from main material pin."), Result);
            return true;
        }
    }

    int32 ExpressionIndex = -1;
    Payload->TryGetNumberField(TEXT("expressionIndex"), ExpressionIndex);

    UMaterialExpression* TargetExpr = ExpressionIndex >= 0
        ? FindExpressionByIdOrNameOrIndex(Material, FString(), ExpressionIndex)
        : FindExpressionByIdOrNameOrIndex(Material, NodeId);
    if (!TargetExpr)
    {
        Bridge.SendAutomationError(Socket, RequestId, TEXT("Node not found."), TEXT("NODE_NOT_FOUND"));
        return true;
    }

    Material.PostEditChange();
    Material.MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, &Material);
    Bridge.SendAutomationResponse(
        Socket,
        RequestId,
        true,
        TEXT("Node disconnection partial (generic inputs not cleared)."),
        Result);
    return true;
}
}
#endif
