#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

bool UMcpAutomationBridgeSubsystem::HandleManageMaterialAuthoringAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (Action != TEXT("manage_material_authoring")) {
    return false;
  }

#if WITH_EDITOR
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
  }

  using namespace McpMaterialAuthoringHandlers;
    if (McpMaterialAuthoringHandlers::HandleCreateMaterial(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleSetBlendMode(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleSetShadingModel(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleSetMaterialDomain(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleAddTextureSample(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleAddTextureCoordinate(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleAddScalarParameter(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleAddVectorParameter(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleAddStaticSwitchParameter(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleAddMathNode(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleAddSceneDataNode(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleAddConditionalNode(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleAddComponentMask(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleAddDotProduct(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleAddCrossProduct(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleAddDesaturation(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleAddAppendVector(this, RequestId, SubAction, Payload, Socket)) { return true; }
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
    if (McpMaterialAuthoringHandlers::HandleCreateSpecializedMaterial(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleAddLandscapeLayer(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleConfigureLayerBlend(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleCompileMaterial(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleGetMaterialInfo(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleGetMaterialFunctionInfo(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleFindNode(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleGetNodeConnections(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleGetNodeProperties(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleSetStaticSwitchParameterValue(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleDeleteNode(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleUpdateCustomExpression(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleGetNodeChain(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleGetConnectedSubgraph(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleAddMaterialNode(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleRemoveMaterialNode(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleSetMaterialParameter(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleGetMaterialNodeDetails(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleSetTwoSided(this, RequestId, SubAction, Payload, Socket)) { return true; }
    if (McpMaterialAuthoringHandlers::HandleSetCastShadows(this, RequestId, SubAction, Payload, Socket)) { return true; }

  SendAutomationError(
      Socket, RequestId,
      FString::Printf(TEXT("Unknown subAction: %s"), *SubAction),
      TEXT("INVALID_SUBACTION"));
  return true;
#else
  SendAutomationError(Socket, RequestId, TEXT("Editor only."),
                      TEXT("EDITOR_ONLY"));
  return true;
#endif
}
