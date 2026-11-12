#include "Domains/MaterialAuthoring/McpMatPrivate.h"

namespace McpMaterialAuthoringHandlers
{
bool HandleAddTextureCoordinate(UEngineWireSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("add_texture_coordinate")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    int32 CoordIndex = 0;
    double UTiling = 1.0, VTiling = 1.0;
    Payload->TryGetNumberField(TEXT("coordinateIndex"), CoordIndex);
    Payload->TryGetNumberField(TEXT("uTiling"), UTiling);
    Payload->TryGetNumberField(TEXT("vTiling"), VTiling);

    UMaterialExpressionTextureCoordinate *TexCoord =
        NewObject<UMaterialExpressionTextureCoordinate>(
            HostOuter, UMaterialExpressionTextureCoordinate::StaticClass(),
            NAME_None, RF_Transactional);
    TexCoord->CoordinateIndex = CoordIndex;
    TexCoord->UTiling = UTiling;
    TexCoord->VTiling = VTiling;
    TSharedPtr<FJsonObject> Result = PlaceMaterialExpression(RequestId, Material, Function, TexCoord, X, Y);
    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Texture coordinate added."), Result);
    return true;
  }

  return false;
}
}
