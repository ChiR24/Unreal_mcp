#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleAddSceneDataNode(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("add_world_position") ||
      SubAction == TEXT("add_vertex_normal") ||
      SubAction == TEXT("add_pixel_depth") || SubAction == TEXT("add_fresnel") ||
      SubAction == TEXT("add_reflection_vector") ||
      SubAction == TEXT("add_panner") || SubAction == TEXT("add_rotator") ||
      SubAction == TEXT("add_noise") || SubAction == TEXT("add_voronoi")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    UMaterialExpression *NewExpr = nullptr;
    FString NodeName;

    if (SubAction == TEXT("add_world_position")) {
      NewExpr = NewObject<UMaterialExpressionWorldPosition>(
          HostOuter, UMaterialExpressionWorldPosition::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("WorldPosition");
    } else if (SubAction == TEXT("add_vertex_normal")) {
      NewExpr = NewObject<UMaterialExpressionVertexNormalWS>(
          HostOuter, UMaterialExpressionVertexNormalWS::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("VertexNormalWS");
    } else if (SubAction == TEXT("add_pixel_depth")) {
      NewExpr = NewObject<UMaterialExpressionPixelDepth>(
          HostOuter, UMaterialExpressionPixelDepth::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("PixelDepth");
    } else if (SubAction == TEXT("add_fresnel")) {
      NewExpr = NewObject<UMaterialExpressionFresnel>(
          HostOuter, UMaterialExpressionFresnel::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("Fresnel");
    } else if (SubAction == TEXT("add_reflection_vector")) {
      NewExpr = NewObject<UMaterialExpressionReflectionVectorWS>(
          HostOuter, UMaterialExpressionReflectionVectorWS::StaticClass(),
          NAME_None, RF_Transactional);
      NodeName = TEXT("ReflectionVectorWS");
    } else if (SubAction == TEXT("add_panner")) {
      NewExpr = NewObject<UMaterialExpressionPanner>(
          HostOuter, UMaterialExpressionPanner::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("Panner");
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    } else if (SubAction == TEXT("add_rotator")) {
      // Use runtime class lookup to avoid GetPrivateStaticClass requirement
      // StaticClass() calls GetPrivateStaticClass() internally which isn't exported
      UClass* RotatorClass = FindObject<UClass>(nullptr, TEXT("/Script/Engine.MaterialExpressionRotator"));
      if (RotatorClass)
      {
        UObject* NewExprObj = NewObject<UObject>(HostOuter, RotatorClass, NAME_None, RF_Transactional);
        NewExpr = static_cast<UMaterialExpressionRotator*>(NewExprObj);
      }
      NodeName = TEXT("Rotator");
#endif
    } else if (SubAction == TEXT("add_noise")) {
      NewExpr = NewObject<UMaterialExpressionNoise>(
          HostOuter, UMaterialExpressionNoise::StaticClass(), NAME_None,
          RF_Transactional);
      NodeName = TEXT("Noise");
    } else if (SubAction == TEXT("add_voronoi")) {
      // Voronoi is implemented via Noise with different settings
      UMaterialExpressionNoise *NoiseExpr =
          NewObject<UMaterialExpressionNoise>(
              HostOuter, UMaterialExpressionNoise::StaticClass(), NAME_None,
              RF_Transactional);
      NoiseExpr->NoiseFunction = ENoiseFunction::NOISEFUNCTION_VoronoiALU;
      NewExpr = NoiseExpr;
      NodeName = TEXT("Voronoi");
    }

    if (NewExpr) {
      NewExpr->MaterialExpressionEditorX = (int32)X;
      NewExpr->MaterialExpressionEditorY = (int32)Y;

      AddExpressionToContainer(Material, Function, NewExpr);
      FINALIZE_HOST();

      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("nodeId"),
                             MCP_NODE_ID(NewExpr));
      Bridge->SendAutomationResponse(
          Socket, RequestId, true,
          FString::Printf(TEXT("%s node added."), *NodeName), Result);
    } else {
      // NewExpr was null - could be class lookup failure or UE < 5.1 for rotator
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Failed to create %s node."), *NodeName),
                          TEXT("CREATE_FAILED"));
    }
    return true;
  }

  // --------------------------------------------------------------------------
  // add_if, add_switch
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
