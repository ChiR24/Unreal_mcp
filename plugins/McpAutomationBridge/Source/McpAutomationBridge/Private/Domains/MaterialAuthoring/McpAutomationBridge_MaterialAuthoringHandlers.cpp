#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

bool UMcpAutomationBridgeSubsystem::HandleManageMaterialAuthoringAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (Action != TEXT("manage_material_authoring")) {
    return false;
  }

  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId, TEXT("Missing payload."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction;
  if (!Payload->TryGetStringField(TEXT("subAction"), SubAction) ||
      SubAction.IsEmpty()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Missing 'subAction' for manage_material_authoring"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (SubAction == TEXT("connect_material_pins")) {
    SubAction = TEXT("connect_nodes");
  } else if (SubAction == TEXT("break_material_connections")) {
    SubAction = TEXT("disconnect_nodes");
  } else if (SubAction == TEXT("rebuild_material")) {
    SubAction = TEXT("compile_material");
  } else if (SubAction == TEXT("remove_material_node")) {
    SubAction = TEXT("delete_node");
  } else if (SubAction == TEXT("get_material_function_info")) {
    SubAction = TEXT("get_material_info");
  }

  // create_landscape/decal/post_process_material: create_material with a domain + blend preset.
  static const TMap<FString, TPair<const TCHAR*, const TCHAR*>> MaterialPresets = {
      {TEXT("create_landscape_material"), {TEXT("Surface"), TEXT("Opaque")}},
      {TEXT("create_decal_material"), {TEXT("DeferredDecal"), TEXT("Translucent")}},
      {TEXT("create_post_process_material"), {TEXT("PostProcess"), TEXT("Opaque")}},
  };
  TSharedPtr<FJsonObject> EffectivePayload = Payload;
  if (const TPair<const TCHAR*, const TCHAR*>* Preset = MaterialPresets.Find(SubAction)) {
    EffectivePayload = MakeShared<FJsonObject>(*Payload);
    if (!EffectivePayload->HasField(TEXT("materialDomain"))) EffectivePayload->SetStringField(TEXT("materialDomain"), Preset->Key);
    if (!EffectivePayload->HasField(TEXT("blendMode"))) EffectivePayload->SetStringField(TEXT("blendMode"), Preset->Value);
    SubAction = TEXT("create_material");
  }

  using namespace McpMaterialAuthoringHandlers;
  // Each batch step comes back through this entry point, so it gets exactly the
  // aliases and handler a single call gets.
  if (SubAction == TEXT("build_material_graph")) {
    return HandleBuildMaterialGraph(this, RequestId, Payload, Socket,
        [this, &Action, Socket](const FString& StepId, const TSharedPtr<FJsonObject>& Step) {
          HandleManageMaterialAuthoringAction(StepId, Action, Step, Socket);
        });
  }
    if (McpMaterialAuthoringHandlers::HandleCreateMaterial(this, RequestId, SubAction, EffectivePayload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleSetMaterialEnumProperty(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleAddTextureSample(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleAddTextureCoordinate(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleAddParameterNode(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleAddClassNode(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleAddCustomExpression(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleConnectNodes(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleDisconnectNodes(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleCreateMaterialFunction(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleFunctionInputsOutputs(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleUseMaterialFunction(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleCreateMaterialInstance(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleSetScalarParameterValue(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleSetVectorParameterValue(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleSetTextureParameterValue(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleAddLandscapeLayer(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleConfigureLayerBlend(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleCompileMaterial(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleGetMaterialInfo(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleFindNode(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleGetNodeConnections(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleGetNodeProperties(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleSetStaticSwitchParameterValue(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleDeleteNode(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleUpdateCustomExpression(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleGetNodeChain(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleGetConnectedSubgraph(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleAddMaterialNode(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleSetNodePosition(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleSetMaterialParameter(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleGetMaterialNodeDetails(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleSetTwoSided(this, RequestId, SubAction, Payload, Socket)) { return true; }

  SendAutomationError(
      Socket, RequestId,
      FString::Printf(TEXT("Unknown subAction: %s"), *SubAction),
      TEXT("INVALID_SUBACTION"));
  return true;
}
