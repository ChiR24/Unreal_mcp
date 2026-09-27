#include "Domains/Level/McpAutomationBridge_LevelHandlersActions.h"

bool UMcpAutomationBridgeSubsystem::HandleLevelAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (Lower != TEXT("manage_level")) {
    return false;
  }

  FString EffectiveAction = Lower;
  bool bForceStreamUnload = false;
  if (Lower == TEXT("manage_level")) {
    if (!Payload.IsValid()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("manage_level payload missing"),
                          TEXT("INVALID_PAYLOAD"));
      return true;
    }

    FString SubAction;
    if (!Payload->TryGetStringField(TEXT("subAction"), SubAction) || SubAction.IsEmpty()) {
      Payload->TryGetStringField(TEXT("action"), SubAction);
    }
    const FString LowerSub = SubAction.ToLower();
    if (LowerSub == TEXT("load") || LowerSub == TEXT("load_level")) {
      return McpLevelHandlers::HandleLoadLevelAction(*this, RequestId, Payload,
                                                     RequestingSocket);
    }

    if (LowerSub == TEXT("save") || LowerSub == TEXT("save_level")) {
      EffectiveAction = TEXT("save_current_level");
    } else if (LowerSub == TEXT("save_as") || LowerSub == TEXT("save_level_as")) {
      EffectiveAction = TEXT("save_level_as");
    } else if (LowerSub == TEXT("create_level")) {
      EffectiveAction = TEXT("create_new_level");
    } else if (LowerSub == TEXT("stream")) {
      EffectiveAction = TEXT("stream_level");
    } else if (LowerSub == TEXT("create_light")) {
      EffectiveAction = TEXT("spawn_light");
    } else if (LowerSub == TEXT("build_lighting")) {
      EffectiveAction = TEXT("build_lighting");
    } else if (LowerSub == TEXT("set_metadata")) {
      EffectiveAction = TEXT("set_metadata");
    } else if (LowerSub == TEXT("set_world_settings") ||
               LowerSub == TEXT("set_level_world_settings")) {
      // Published as manage_level.set_world_settings; the gateway forwards the
      // canonical action name rather than the record's dispatchAction, so the
      // normalizer has to accept it alongside the native spelling.
      EffectiveAction = TEXT("set_level_world_settings");
    } else if (LowerSub == TEXT("validate_level")) {
      EffectiveAction = TEXT("validate_level");
    } else if (LowerSub == TEXT("list") || LowerSub == TEXT("list_levels")) {
      EffectiveAction = TEXT("list_levels");
    } else if (LowerSub == TEXT("get_current_level")) {
      EffectiveAction = TEXT("get_current_level");
    } else if (LowerSub == TEXT("export_level")) {
      EffectiveAction = TEXT("export_level");
    } else if (LowerSub == TEXT("import_level")) {
      EffectiveAction = TEXT("import_level");
    } else if (LowerSub == TEXT("add_sublevel")) {
      EffectiveAction = TEXT("add_sublevel");
    } else if (LowerSub == TEXT("delete") || LowerSub == TEXT("delete_level")) {
      EffectiveAction = TEXT("delete_level");
    } else if (LowerSub == TEXT("rename") || LowerSub == TEXT("rename_level")) {
      EffectiveAction = TEXT("rename_level");
    } else if (LowerSub == TEXT("duplicate") || LowerSub == TEXT("duplicate_level")) {
      EffectiveAction = TEXT("duplicate_level");
    } else if (LowerSub == TEXT("get_summary")) {
      EffectiveAction = TEXT("get_level_info");
    } else if (LowerSub == TEXT("unload") || LowerSub == TEXT("unload_level")) {
      EffectiveAction = TEXT("stream_level");
      bForceStreamUnload = true;
    } else {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Unknown manage_level action: %s"), *SubAction),
          TEXT("UNKNOWN_ACTION"));
      return true;
    }
  }

  struct FLevelRoute {
    const TCHAR* Name;
    bool (*Handler)(UMcpAutomationBridgeSubsystem&, const FString&, const TSharedPtr<FJsonObject>&, TSharedPtr<FMcpBridgeWebSocket>);
  };
  static const FLevelRoute Routes[] = {
      {TEXT("get_current_level"), McpLevelHandlers::HandleGetCurrentLevelAction},
      {TEXT("save_current_level"), McpLevelHandlers::HandleSaveCurrentLevelAction},
      {TEXT("save_level_as"), McpLevelHandlers::HandleSaveLevelAsAction},
      {TEXT("set_metadata"), McpLevelHandlers::HandleSetMetadataAction},
      {TEXT("validate_level"), McpLevelHandlers::HandleValidateLevelAction},
      {TEXT("create_new_level"), McpLevelHandlers::HandleCreateNewLevelAction},
      {TEXT("spawn_light"), McpLevelHandlers::HandleSpawnLightAction},
      {TEXT("list_levels"), McpLevelHandlers::HandleListLevelsAction},
      {TEXT("export_level"), McpLevelHandlers::HandleExportLevelAction},
      {TEXT("import_level"), McpLevelHandlers::HandleImportLevelAction},
      {TEXT("add_sublevel"), McpLevelHandlers::HandleAddSublevelAction},
      {TEXT("delete_level"), McpLevelHandlers::HandleDeleteLevelAction},
      {TEXT("rename_level"), McpLevelHandlers::HandleRenameLevelAction},
      {TEXT("duplicate_level"), McpLevelHandlers::HandleDuplicateLevelAction},
      {TEXT("get_level_info"), McpLevelHandlers::HandleGetLevelInfoAction},
      {TEXT("set_level_world_settings"), McpLevelHandlers::HandleSetLevelWorldSettingsAction},
  };

  if (EffectiveAction == TEXT("stream_level")) {
    return McpLevelHandlers::HandleStreamLevelAction(*this, RequestId, Payload,
                                                     RequestingSocket,
                                                     bForceStreamUnload);
  }
  if (EffectiveAction == TEXT("build_lighting")) {
    return McpLevelHandlers::HandleBuildLightingAction(*this, RequestId, Payload,
                                                       RequestingSocket);
  }
  for (const FLevelRoute& Route : Routes) {
    if (EffectiveAction == Route.Name) {
      return Route.Handler(*this, RequestId, Payload, RequestingSocket);
    }
  }
  return false;
}
