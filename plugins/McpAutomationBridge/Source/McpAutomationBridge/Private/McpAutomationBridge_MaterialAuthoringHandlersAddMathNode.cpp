#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleAddMathNode(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("add_math_node")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    FString Operation;
    if (!Payload->TryGetStringField(TEXT("operation"), Operation)) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'operation'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    UMaterialExpression *MathNode = nullptr;
    if (Operation == TEXT("Add")) {
      MathNode = NewObject<UMaterialExpressionAdd>(
          HostOuter, UMaterialExpressionAdd::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("Subtract")) {
      MathNode = NewObject<UMaterialExpressionSubtract>(
          HostOuter, UMaterialExpressionSubtract::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("Multiply")) {
      MathNode = NewObject<UMaterialExpressionMultiply>(
          HostOuter, UMaterialExpressionMultiply::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("Divide")) {
      MathNode = NewObject<UMaterialExpressionDivide>(
          HostOuter, UMaterialExpressionDivide::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("Lerp")) {
      MathNode = NewObject<UMaterialExpressionLinearInterpolate>(
          HostOuter, UMaterialExpressionLinearInterpolate::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("Clamp")) {
      MathNode = NewObject<UMaterialExpressionClamp>(
          HostOuter, UMaterialExpressionClamp::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("Power")) {
      MathNode = NewObject<UMaterialExpressionPower>(
          HostOuter, UMaterialExpressionPower::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("Frac")) {
      MathNode = NewObject<UMaterialExpressionFrac>(
          HostOuter, UMaterialExpressionFrac::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("OneMinus")) {
      MathNode = NewObject<UMaterialExpressionOneMinus>(
          HostOuter, UMaterialExpressionOneMinus::StaticClass(), NAME_None,
          RF_Transactional);
    } else if (Operation == TEXT("Append")) {
      MathNode = NewObject<UMaterialExpressionAppendVector>(
          HostOuter, UMaterialExpressionAppendVector::StaticClass(), NAME_None,
          RF_Transactional);
    } else {
      Bridge->SendAutomationError(
          Socket, RequestId,
          FString::Printf(TEXT("Unknown operation: %s"), *Operation),
          TEXT("UNKNOWN_OPERATION"));
      return true;
    }

    MathNode->MaterialExpressionEditorX = (int32)X;
    MathNode->MaterialExpressionEditorY = (int32)Y;

    AddExpressionToContainer(Material, Function, MathNode);
    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"),
                           MCP_NODE_ID(MathNode));
    Bridge->SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Math node '%s' added."), *Operation), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_world_position, add_vertex_normal, add_pixel_depth, add_fresnel,
  // add_reflection_vector, add_panner, add_rotator, add_noise, add_voronoi
  // --------------------------------------------------------------------------
  return false;
}
}
#endif
