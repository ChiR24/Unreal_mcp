#include "Domains/Level/McpLevelActions.h"
#include "Domains/Lighting/McpLightingPrivate.h"

bool UEngineWireSubsystem::HandleLevelAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  // The table's manage_level entry is the one caller; a WebSocket request can still come without a payload.
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("manage_level payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  using FLevelHandler = bool (*)(UEngineWireSubsystem&, const FString&, const TSharedPtr<FJsonObject>&, TSharedPtr<FMcpBridgeWebSocket>);
  static const FLevelHandler Stream = [](UEngineWireSubsystem& Subsystem, const FString& Id, const TSharedPtr<FJsonObject>& Body,
                                         TSharedPtr<FMcpBridgeWebSocket> Socket) {
    return McpLevelHandlers::HandleStreamLevelAction(Subsystem, Id, Body, Socket, false);
  };
  static const FLevelHandler Unload = [](UEngineWireSubsystem& Subsystem, const FString& Id, const TSharedPtr<FJsonObject>& Body,
                                         TSharedPtr<FMcpBridgeWebSocket> Socket) {
    return McpLevelHandlers::HandleStreamLevelAction(Subsystem, Id, Body, Socket, true);
  };
  struct FLevelRoute {
    const TCHAR* Name;
    FLevelHandler Handler;
  };
  using namespace McpLevelHandlers;
  static const FLevelRoute Routes[] = {
      {TEXT("load"), HandleLoadLevelAction},
      {TEXT("load_level"), HandleLoadLevelAction},
      {TEXT("save"), HandleSaveCurrentLevelAction},
      {TEXT("save_level"), HandleSaveCurrentLevelAction},
      {TEXT("save_as"), HandleSaveLevelAsAction},
      {TEXT("save_level_as"), HandleSaveLevelAsAction},
      {TEXT("create_level"), HandleCreateNewLevelAction},
      {TEXT("stream"), Stream},
      {TEXT("unload"), Unload},
      {TEXT("unload_level"), Unload},
      {TEXT("create_light"), McpLightingHandlers::HandleSpawnLight},
      {TEXT("build_lighting"), McpLightingHandlers::HandleBuildLighting},
      {TEXT("set_metadata"), HandleSetMetadataAction},
      {TEXT("set_world_settings"), HandleSetLevelWorldSettingsAction},
      {TEXT("validate_level"), HandleValidateLevelAction},
      {TEXT("run_map_check"), HandleRunMapCheckAction},
      {TEXT("list_levels"), HandleListLevelsAction},
      {TEXT("get_current_level"), HandleGetCurrentLevelAction},
      {TEXT("export_level"), HandleExportLevelAction},
      {TEXT("import_level"), HandleImportLevelAction},
      {TEXT("add_sublevel"), HandleAddSublevelAction},
      {TEXT("remove_sublevel"), HandleRemoveSublevelAction},
      {TEXT("move_actors_to_level"), HandleMoveActorsToLevelAction},
      {TEXT("set_current_level"), HandleSetCurrentLevelAction},
      {TEXT("delete"), HandleDeleteLevelAction},
      {TEXT("delete_level"), HandleDeleteLevelAction},
      {TEXT("rename_level"), HandleRenameLevelAction},
      {TEXT("duplicate_level"), HandleDuplicateLevelAction},
      {TEXT("get_summary"), HandleGetLevelInfoAction},
  };

  const FString SubAction = McpGetFirstStringField(Payload, {TEXT("subAction"), TEXT("action")});
  const FString LowerSub = SubAction.ToLower();
  for (const FLevelRoute& Route : Routes) {
    if (LowerSub == Route.Name) {
      return Route.Handler(*this, RequestId, Payload, RequestingSocket);
    }
  }
  SendAutomationError(
      RequestingSocket, RequestId,
      FString::Printf(TEXT("Unknown manage_level action: %s"), *SubAction),
      TEXT("UNKNOWN_ACTION"));
  return true;
}
