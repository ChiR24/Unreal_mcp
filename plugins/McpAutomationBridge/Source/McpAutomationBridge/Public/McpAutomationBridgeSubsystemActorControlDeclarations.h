#define MCP_SUBSYSTEM_ACTOR_CONTROL_DECLARATIONS \
AActor* FindActorByName(const FString& Target, bool bExactMatchOnly = false); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorSpawn); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorSpawnBlueprint); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorDelete); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorApplyForce); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorSetTransform); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorGetTransform); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorSetVisibility); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorAddComponent); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorSetComponentProperties); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorSetMaterial); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorGetComponents); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorDuplicate); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorAttach); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorDetach); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorFindByTag); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorAddTag); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorRemoveTag); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorFindByName); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorDeleteByTag); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorSetBlueprintVariables); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorCreateSnapshot); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorList); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorGet); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorRestoreSnapshot); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorExport); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorGetBoundingBox); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorGetMetadata); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorFindByClass); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorRemoveComponent); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorGetComponentProperty); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorSetCollision); \
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorCallFunction);
