#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_MaterialGraphHandlersPrivate.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Engine/Texture.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleCreateMaterialNodes(
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

    UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material)
    {
        SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Could not load Material: %s"), *MaterialPath), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    const TArray<TSharedPtr<FJsonValue>>* NodesArray = nullptr;
    if (!Payload->TryGetArrayField(TEXT("nodes"), NodesArray) || !NodesArray)
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'nodes' array."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    TArray<TSharedPtr<FJsonValue>> CreatedNodes;
    int32 SuccessCount = 0;
    int32 FailCount = 0;

    for (const TSharedPtr<FJsonValue>& NodeVal : *NodesArray)
    {
        TSharedPtr<FJsonObject> NodeObj = NodeVal->AsObject();
        if (!NodeObj.IsValid())
        {
            ++FailCount;
            continue;
        }

        FString NodeType;
        if (!NodeObj->TryGetStringField(TEXT("type"), NodeType) || NodeType.IsEmpty())
        {
            ++FailCount;
            continue;
        }

        float X = 0.0f;
        float Y = 0.0f;
        NodeObj->TryGetNumberField(TEXT("x"), X);
        NodeObj->TryGetNumberField(TEXT("y"), Y);

        UClass* ExpressionClass = McpMaterialGraphHandlers::ResolveBatchNodeClass(NodeType);
        if (!ExpressionClass)
        {
            ++FailCount;
            continue;
        }

        UMaterialExpression* NewExpr =
            NewObject<UMaterialExpression>(Material, ExpressionClass, NAME_None, RF_Transactional);
        if (!NewExpr)
        {
            ++FailCount;
            continue;
        }

        NewExpr->MaterialExpressionEditorX = (int32)X;
        NewExpr->MaterialExpressionEditorY = (int32)Y;

        FString ParamName;
        if (NodeObj->TryGetStringField(TEXT("name"), ParamName))
        {
            if (UMaterialExpressionParameter* ParamExpr = Cast<UMaterialExpressionParameter>(NewExpr))
            {
                ParamExpr->ParameterName = FName(*ParamName);
            }
        }

        FString TexturePath;
        if (NodeObj->TryGetStringField(TEXT("texturePath"), TexturePath))
        {
            if (UMaterialExpressionTextureSample* TexSample = Cast<UMaterialExpressionTextureSample>(NewExpr))
            {
                if (UTexture* Texture = LoadObject<UTexture>(nullptr, *TexturePath))
                {
                    TexSample->Texture = Texture;
                }
            }
        }

        double DefaultValue = 0.0;
        if (NodeObj->TryGetNumberField(TEXT("value"), DefaultValue))
        {
            if (UMaterialExpressionConstant* ConstExpr = Cast<UMaterialExpressionConstant>(NewExpr))
            {
                ConstExpr->R = (float)DefaultValue;
            }
        }

        McpMaterialGraphHandlers::AddExpressionToMaterial(*Material, *NewExpr);

        TSharedPtr<FJsonObject> NodeInfo = McpHandlerUtils::CreateResultObject();
        NodeInfo->SetStringField(TEXT("nodeId"), NewExpr->GetName());
        NodeInfo->SetStringField(TEXT("type"), ExpressionClass->GetName());
        CreatedNodes.Add(MakeShared<FJsonValueObject>(NodeInfo));
        ++SuccessCount;
    }

    Material->PreEditChange(nullptr);
    Material->PostEditChange();
    McpSafeAssetSave(Material);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Material);
    Result->SetArrayField(TEXT("createdNodes"), CreatedNodes);
    Result->SetNumberField(TEXT("successCount"), SuccessCount);
    Result->SetNumberField(TEXT("failCount"), FailCount);

    SendAutomationResponse(
        Socket,
        RequestId,
        true,
        FString::Printf(TEXT("Created %d nodes (%d failed)."), SuccessCount, FailCount),
        Result);
    return true;
#else
    SendAutomationError(Socket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}
