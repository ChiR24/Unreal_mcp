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

// A numeric payload knob and the expression UPROPERTY it sets (speedX -> SpeedX).
struct FClassNodeNumberField
{
  const TCHAR* Field;
  const TCHAR* Property;
};

// Calls Apply for each knob the payload carries. Returns false, naming the field
// in OutMissing, when Class has no numeric property for it: a knob the node
// lacks is refused, not dropped (constA/constB, speed, scale... used to be
// declared and silently ignored).
bool ForEachClassNodeNumber(UClass* Class, const TSharedPtr<FJsonObject>& Payload, TArrayView<const FClassNodeNumberField> Fields,
                            FString& OutMissing, TFunctionRef<void(FNumericProperty*, double)> Apply)
{
  for (const FClassNodeNumberField& Entry : Fields) {
    double Value = 0.0;
    if (!Payload->TryGetNumberField(Entry.Field, Value)) { continue; }
    FNumericProperty* Prop = Class ? CastField<FNumericProperty>(Class->FindPropertyByName(Entry.Property)) : nullptr;
    if (!Prop) { OutMissing = Entry.Field; return false; }
    Apply(Prop, Value);
  }
  return true;
}

void SetClassNodeNumber(UMaterialExpression* Expr, FNumericProperty* Prop, double Value)
{
  void* Ptr = Prop->ContainerPtrToValuePtr<void>(Expr);
  if (Prop->IsFloatingPoint()) { Prop->SetFloatingPointPropertyValue(Ptr, Value); }
  else { Prop->SetIntPropertyValue(Ptr, static_cast<int64>(Value)); }
}

// Validates the knobs against Class, then creates the node with them applied.
bool AddConfiguredNode(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                       TSharedPtr<FMcpBridgeWebSocket> Socket, UClass* Class, const FString& Message,
                       TArrayView<const FClassNodeNumberField> Fields, bool bVoronoi)
{
  FString Missing;
  if (!ForEachClassNodeNumber(Class, Payload, Fields, Missing, [](FNumericProperty*, double) {})) {
    Bridge->SendAutomationError(Socket, RequestId,
        FString::Printf(TEXT("'%s' does not apply to this node (%s); wire an input pin instead."), *Missing, *Message),
        TEXT("INVALID_ARGUMENT"));
    return true;
  }
  return AddPlacedExpression(Bridge, RequestId, Payload, Socket, Class, Message, [&](UMaterialExpression* Expr) {
    // Voronoi is Noise with the Voronoi function.
    if (bVoronoi) { CastChecked<UMaterialExpressionNoise>(Expr)->NoiseFunction = ENoiseFunction::NOISEFUNCTION_VoronoiALU; }
    ForEachClassNodeNumber(Class, Payload, Fields, Missing, [Expr](FNumericProperty* Prop, double Value) { SetClassNodeNumber(Expr, Prop, Value); });
  });
}
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
  static const FClassNodeNumberField PannerFields[] = {{TEXT("speedX"), TEXT("SpeedX")}, {TEXT("speedY"), TEXT("SpeedY")}};
  static const FClassNodeNumberField RotatorFields[] = {{TEXT("speed"), TEXT("Speed")}};
  static const FClassNodeNumberField NoiseFields[] = {{TEXT("scale"), TEXT("Scale")}, {TEXT("levels"), TEXT("Levels")}};
  static const FClassNodeNumberField MathFields[] = {{TEXT("constA"), TEXT("ConstA")}, {TEXT("constB"), TEXT("ConstB")}};
  if (const FNodeKind* Kind = Kinds.Find(SubAction)) {
    TArrayView<const FClassNodeNumberField> Fields;
    if (SubAction == TEXT("add_panner")) { Fields = MakeArrayView(PannerFields); }
    else if (SubAction == TEXT("add_rotator")) { Fields = MakeArrayView(RotatorFields); }
    else if (SubAction == TEXT("add_noise")) { Fields = MakeArrayView(NoiseFields); }
    else if (SubAction == TEXT("add_voronoi")) { Fields = MakeArrayView(NoiseFields, 1); }
    return AddConfiguredNode(Bridge, RequestId, Payload, Socket, Kind->Class,
        FString::Printf(TEXT("%s node added."), Kind->Name), Fields, SubAction == TEXT("add_voronoi"));
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
    return AddConfiguredNode(Bridge, RequestId, Payload, Socket, *OperationClass,
        FString::Printf(TEXT("Math node '%s' added."), *Operation), MakeArrayView(MathFields), false);
  }
  return false;
}
}
