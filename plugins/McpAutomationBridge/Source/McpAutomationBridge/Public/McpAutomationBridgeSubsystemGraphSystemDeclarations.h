#define MCP_SUBSYSTEM_GRAPH_SYSTEM_DECLARATIONS \
MCP_DECLARE_ACTION_HANDLER(HandleAnalyzeGraph); \
MCP_DECLARE_ACTION_HANDLER(HandleSystemControlAction); \
MCP_DECLARE_ACTION_HANDLER(HandleConsoleCommandAction); \
MCP_DECLARE_ACTION_HANDLER(HandleInspectAction); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleInspectCdoAction); \
MCP_DECLARE_ACTION_HANDLER(HandleBlueprintGraphAction); \
MCP_DECLARE_ACTION_HANDLER(HandleNiagaraGraphAction); \
bool HandleMaterialGraphAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket); \
MCP_DECLARE_ACTION_HANDLER(HandleBehaviorTreeAction); \
MCP_DECLARE_ACTION_HANDLER(HandleWorldPartitionAction); \
MCP_DECLARE_ACTION_HANDLER(HandleRenderAction); \
MCP_DECLARE_ACTION_HANDLER(HandleListBlueprints); \
MCP_DECLARE_ACTION_HANDLER(HandleGeometryAction);
