#define MCP_SUBSYSTEM_AUTHORING_DECLARATIONS \
MCP_DECLARE_ACTION_HANDLER(HandleCreateLandscape); \
MCP_DECLARE_ACTION_HANDLER(HandleCreateLandscapeGrassType); \
MCP_DECLARE_ACTION_HANDLER(HandleModifyHeightmap); \
MCP_DECLARE_ACTION_HANDLER(HandlePaintLandscapeLayer); \
MCP_DECLARE_ACTION_HANDLER(HandleSculptLandscape); \
MCP_DECLARE_ACTION_HANDLER(HandleSetLandscapeMaterial); \
MCP_DECLARE_ACTION_HANDLER(HandlePlayAnimMontage); \
MCP_DECLARE_ACTION_HANDLER(HandleSetupRagdoll); \
MCP_DECLARE_ACTION_HANDLER(HandleSCSAction); \
MCP_DECLARE_ACTION_HANDLER(HandleManageMaterialAuthoringAction); \
MCP_DECLARE_ACTION_HANDLER(HandleManageTextureAction); \
TSharedPtr<FJsonObject> HandleManageTextureAction(const TSharedPtr<FJsonObject>& Params); \
MCP_DECLARE_ACTION_HANDLER(HandleManageAnimationAuthoringAction); \
MCP_DECLARE_ACTION_HANDLER(HandleManageAudioAuthoringAction); \
MCP_DECLARE_ACTION_HANDLER(HandleManageNiagaraAuthoringAction);
