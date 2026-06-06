#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_MaterialGraphHandlersPrivate.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Engine/Texture.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSample.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleAddMaterialTextureSample(
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

    FString TexturePath;
    if (!Payload->TryGetStringField(TEXT("texturePath"), TexturePath) || TexturePath.IsEmpty())
    {
        SendAutomationError(Socket, RequestId, TEXT("Missing 'texturePath'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Material)
    {
        SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Could not load Material: %s"), *MaterialPath), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    UTexture* Texture = LoadObject<UTexture>(nullptr, *TexturePath);
    if (!Texture)
    {
        SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Could not load Texture: %s"), *TexturePath), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    int32 CoordinateIndex = 0;
    Payload->TryGetNumberField(TEXT("coordinateIndex"), CoordinateIndex);
    float X = 0.0f;
    float Y = 0.0f;
    Payload->TryGetNumberField(TEXT("x"), X);
    Payload->TryGetNumberField(TEXT("y"), Y);

    UMaterialExpressionTextureSample* TexSample =
        NewObject<UMaterialExpressionTextureSample>(Material, UMaterialExpressionTextureSample::StaticClass(), NAME_None, RF_Transactional);
    if (!TexSample)
    {
        SendAutomationError(Socket, RequestId, TEXT("Failed to create TextureSample expression."), TEXT("CREATE_FAILED"));
        return true;
    }

    TexSample->Texture = Texture;
    TexSample->ConstCoordinate = CoordinateIndex;
    TexSample->MaterialExpressionEditorX = (int32)X;
    TexSample->MaterialExpressionEditorY = (int32)Y;
    McpMaterialGraphHandlers::AddExpressionToMaterial(*Material, *TexSample);

    Material->PreEditChange(nullptr);
    Material->PostEditChange();
    McpSafeAssetSave(Material);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Material);
    Result->SetStringField(TEXT("nodeId"), TexSample->GetName());
    Result->SetStringField(TEXT("texturePath"), Texture->GetPathName());

    SendAutomationResponse(Socket, RequestId, true, TEXT("TextureSample expression added to material."), Result);
    return true;
#else
    SendAutomationError(Socket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}
