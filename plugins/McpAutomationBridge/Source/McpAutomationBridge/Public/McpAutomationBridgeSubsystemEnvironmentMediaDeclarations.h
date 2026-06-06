#define MCP_SUBSYSTEM_ENVIRONMENT_MEDIA_DECLARATIONS \
bool CreateNiagaraEffect(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, const FString& EffectName, const FString& DefaultSystemPath); \
MCP_DECLARE_ACTION_HANDLER(HandleAudioAction); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleCreateDialogueVoice); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleCreateDialogueWave); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleSetDialogueContext); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleCreateReverbEffect); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleCreateSourceEffectChain); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleAddSourceEffect); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleCreateSubmixEffect); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleCreateInteractionComponent); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleConfigureInteractionTrace); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleConfigureInteractionWidget); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleCreateDoorActor); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleCreateSwitchActor); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleCreateChestActor); \
MCP_DECLARE_ACTION_HANDLER(HandleLightingAction); \
MCP_DECLARE_ACTION_HANDLER(HandlePerformanceAction); \
MCP_DECLARE_ACTION_HANDLER(HandleBuildEnvironmentAction); \
MCP_DECLARE_ACTION_HANDLER(HandleControlEnvironmentAction); \
MCP_DECLARE_ACTION_HANDLER(HandlePaintFoliage); \
MCP_DECLARE_ACTION_HANDLER(HandleGetFoliageInstances); \
MCP_DECLARE_ACTION_HANDLER(HandleRemoveFoliage); \
MCP_DECLARE_ACTION_HANDLER(HandleGenerateLODs); \
MCP_DECLARE_ACTION_HANDLER(HandleBakeLightmap); \
MCP_DECLARE_ACTION_HANDLER(HandleCreateProceduralTerrain); \
MCP_DECLARE_ACTION_HANDLER(HandleCreateProceduralFoliage); \
MCP_DECLARE_ACTION_HANDLER(HandleAddFoliageType); \
MCP_DECLARE_ACTION_HANDLER(HandleAddFoliageInstances);
