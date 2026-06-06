#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_MaterialGraphHandlersPrivate.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleAddMaterialExpression(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString MaterialPath;
    if (!Payload->TryGetStringField(TEXT("materialPath"), MaterialPath) || MaterialPath.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'materialPath'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString ExpressionClassName;
    if (!Payload->TryGetStringField(TEXT("expressionClass"), ExpressionClassName) || ExpressionClassName.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'expressionClass'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material)
    {
        SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Could not load Material: %s"), *MaterialPath), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    float X = 0.0f;
    float Y = 0.0f;
    Payload->TryGetNumberField(TEXT("x"), X);
    Payload->TryGetNumberField(TEXT("y"), Y);

    UClass* ExpressionClass = McpMaterialGraphHandlers::ResolveExpressionClass(ExpressionClassName);
    if (!ExpressionClass)
    {
        SendAutomationError(
            Socket,
            RequestId,
            FString::Printf(
                TEXT("Unknown expression class: %s. Try using the full class name like 'MaterialExpressionAdd' or 'Add'."),
                *ExpressionClassName),
            TEXT("CLASS_NOT_FOUND"));
        return true;
    }

    UMaterialExpression* NewExpr =
        NewObject<UMaterialExpression>(Material, ExpressionClass, NAME_None, RF_Transactional);
    if (!NewExpr)
    {
        SendAutomationError(Socket, RequestId, TEXT("Failed to create expression."), TEXT("CREATE_FAILED"));
        return true;
    }

    NewExpr->MaterialExpressionEditorX = (int32)X;
    NewExpr->MaterialExpressionEditorY = (int32)Y;
    McpMaterialGraphHandlers::AddExpressionToMaterial(*Material, *NewExpr);

    Material->PreEditChange(nullptr);
    Material->PostEditChange();
    McpSafeAssetSave(Material);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Material);
    Result->SetStringField(TEXT("nodeId"), NewExpr->GetName());
    Result->SetStringField(TEXT("expressionClass"), ExpressionClass->GetName());

    SendAutomationResponse(
        Socket,
        RequestId,
        true,
        FString::Printf(TEXT("Expression '%s' added to material."), *ExpressionClass->GetName()),
        Result);
    return true;
#else
    SendAutomationError(Socket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}
