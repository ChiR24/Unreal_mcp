#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"
#include "Materials/MaterialExpressionStaticSwitch.h"

namespace McpMaterialAuthoringHandlers
{
namespace
{
// Creates one expression of Class in the host at x/y, lets Configure touch it, adds it,
// and replies with its nodeId, placement fields and Message.
template <typename TConfigure>
bool AddPlacedExpression(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket,
                         UClass* Class, const FString& Message, TConfigure&& Configure)
{
  LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();
  if (!Class) {
    Bridge->SendAutomationError(Socket, RequestId,
        FString::Printf(TEXT("Failed to create the node for '%s': its class is not available in this engine."), *Message),
        TEXT("CREATE_FAILED"));
    return true;
  }
  UMaterialExpression *NewExpr = NewObject<UMaterialExpression>(HostOuter, Class, NAME_None, RF_Transactional);
  Configure(NewExpr);
  NewExpr->MaterialExpressionEditorX = (int32)X;
  NewExpr->MaterialExpressionEditorY = (int32)Y;
  AddExpressionToContainer(Material, Function, NewExpr);
  FINALIZE_HOST();

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("nodeId"), MCP_NODE_ID(NewExpr));
  AddMaterialNodePlacementFields(Result, Material, NewExpr);
  Bridge->SendAutomationResponse(Socket, RequestId, true, Message, Result);
  return true;
}

struct FNodeKind
{
  const TCHAR* Name;
  UClass* Class;
};
}

bool HandleAddClassNode(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  // add_<scene/conditional node>: one expression class each.
  static const TMap<FString, FNodeKind> Kinds = {
      {TEXT("add_world_position"), {TEXT("WorldPosition"), UMaterialExpressionWorldPosition::StaticClass()}},
      {TEXT("add_vertex_normal"), {TEXT("VertexNormalWS"), UMaterialExpressionVertexNormalWS::StaticClass()}},
      {TEXT("add_pixel_depth"), {TEXT("PixelDepth"), UMaterialExpressionPixelDepth::StaticClass()}},
      {TEXT("add_fresnel"), {TEXT("Fresnel"), UMaterialExpressionFresnel::StaticClass()}},
      {TEXT("add_reflection_vector"), {TEXT("ReflectionVectorWS"), UMaterialExpressionReflectionVectorWS::StaticClass()}},
      {TEXT("add_panner"), {TEXT("Panner"), UMaterialExpressionPanner::StaticClass()}},
      // Rotator is not MinimalAPI on 5.0, so its StaticClass() does not link there; look it up.
      {TEXT("add_rotator"), {TEXT("Rotator"), FindObject<UClass>(nullptr, TEXT("/Script/Engine.MaterialExpressionRotator"))}},
      {TEXT("add_noise"), {TEXT("Noise"), UMaterialExpressionNoise::StaticClass()}},
      {TEXT("add_voronoi"), {TEXT("Voronoi"), UMaterialExpressionNoise::StaticClass()}},
      {TEXT("add_if"), {TEXT("If"), UMaterialExpressionIf::StaticClass()}},
      // add_switch authors a real StaticSwitch (dogfood #204: it used to create an If node).
      {TEXT("add_switch"), {TEXT("StaticSwitch"), UMaterialExpressionStaticSwitch::StaticClass()}},
  };
  if (const FNodeKind* Kind = Kinds.Find(SubAction)) {
    const bool bVoronoi = SubAction == TEXT("add_voronoi");
    return AddPlacedExpression(Bridge, RequestId, Payload, Socket, Kind->Class,
        FString::Printf(TEXT("%s node added."), Kind->Name), [bVoronoi](UMaterialExpression* Expr) {
          if (bVoronoi) {
            // Voronoi is Noise with the Voronoi function.
            CastChecked<UMaterialExpressionNoise>(Expr)->NoiseFunction = ENoiseFunction::NOISEFUNCTION_VoronoiALU;
          }
        });
  }

  if (SubAction == TEXT("add_math_node")) {
    static const TMap<FString, UClass*> Operations = {
        {TEXT("Add"), UMaterialExpressionAdd::StaticClass()},
        {TEXT("Subtract"), UMaterialExpressionSubtract::StaticClass()},
        {TEXT("Multiply"), UMaterialExpressionMultiply::StaticClass()},
        {TEXT("Divide"), UMaterialExpressionDivide::StaticClass()},
        {TEXT("Lerp"), UMaterialExpressionLinearInterpolate::StaticClass()},
        {TEXT("Clamp"), UMaterialExpressionClamp::StaticClass()},
        {TEXT("Power"), UMaterialExpressionPower::StaticClass()},
        {TEXT("Frac"), UMaterialExpressionFrac::StaticClass()},
        {TEXT("OneMinus"), UMaterialExpressionOneMinus::StaticClass()},
        {TEXT("Append"), UMaterialExpressionAppendVector::StaticClass()},
    };
    FString Operation;
    if (!Payload->TryGetStringField(TEXT("operation"), Operation)) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'operation'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    UClass* const* OperationClass = Operations.Find(Operation);
    if (!OperationClass) {
      Bridge->SendAutomationError(Socket, RequestId,
          FString::Printf(TEXT("Unknown operation: %s"), *Operation), TEXT("UNKNOWN_OPERATION"));
      return true;
    }
    return AddPlacedExpression(Bridge, RequestId, Payload, Socket, *OperationClass,
        FString::Printf(TEXT("Math node '%s' added."), *Operation), [](UMaterialExpression*) {});
  }
  return false;
}
}
