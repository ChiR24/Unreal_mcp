#define MCP_SUBSYSTEM_AUTHORING_DECLARATIONS \
MCP_DECLARE_ACTION_HANDLER(HandleAddMaterialNode); \
MCP_DECLARE_ACTION_HANDLER(HandleConnectMaterialPins); \
MCP_DECLARE_ACTION_HANDLER(HandleRemoveMaterialNode); \
MCP_DECLARE_ACTION_HANDLER(HandleBreakMaterialConnections); \
MCP_DECLARE_ACTION_HANDLER(HandleGetMaterialNodeDetails); \
MCP_DECLARE_ACTION_HANDLER(HandleRebuildMaterial); \
MCP_DECLARE_ACTION_HANDLER(HandleCreateLandscape); \
MCP_DECLARE_ACTION_HANDLER(HandleCreateLandscapeGrassType); \
MCP_DECLARE_ACTION_HANDLER(HandleEditLandscape); \
MCP_DECLARE_ACTION_HANDLER(HandleModifyHeightmap); \
MCP_DECLARE_ACTION_HANDLER(HandlePaintLandscapeLayer); \
MCP_DECLARE_ACTION_HANDLER(HandleSculptLandscape); \
MCP_DECLARE_ACTION_HANDLER(HandleSetLandscapeMaterial); \
MCP_DECLARE_ACTION_HANDLER(HandleCreateAnimBlueprint); \
MCP_DECLARE_ACTION_HANDLER(HandleCreateMaterialNodes); \
MCP_DECLARE_ACTION_HANDLER(HandleCreateNiagaraSystem); \
MCP_DECLARE_ACTION_HANDLER(HandleCreateNiagaraRibbon); \
MCP_DECLARE_ACTION_HANDLER(HandleCreateNiagaraEmitter); \
MCP_DECLARE_ACTION_HANDLER(HandleSpawnNiagaraActor); \
MCP_DECLARE_ACTION_HANDLER(HandleModifyNiagaraParameter); \
MCP_DECLARE_ACTION_HANDLER(HandlePlayAnimMontage); \
MCP_DECLARE_ACTION_HANDLER(HandleSetupRagdoll); \
MCP_DECLARE_ACTION_HANDLER(HandleActivateRagdoll); \
MCP_DECLARE_ACTION_HANDLER(HandleAddMaterialTextureSample); \
MCP_DECLARE_ACTION_HANDLER(HandleAddMaterialExpression); \
MCP_DECLARE_ACTION_HANDLER(HandleSCSAction); \
MCP_DECLARE_ACTION_HANDLER(HandleManageMaterialAuthoringAction); \
MCP_DECLARE_ACTION_HANDLER(HandleManageTextureAction); \
TSharedPtr<FJsonObject> HandleManageTextureAction(const TSharedPtr<FJsonObject>& Params); \
MCP_DECLARE_ACTION_HANDLER(HandleManageAnimationAuthoringAction); \
MCP_DECLARE_ACTION_HANDLER(HandleManageAudioAuthoringAction); \
MCP_DECLARE_ACTION_HANDLER(HandleManageNiagaraAuthoringAction);
