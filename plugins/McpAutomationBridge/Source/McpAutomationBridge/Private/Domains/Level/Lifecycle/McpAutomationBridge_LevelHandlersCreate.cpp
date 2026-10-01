#include "Domains/Level/McpAutomationBridge_LevelHandlersActions.h"
#include "Domains/Level/Lifecycle/McpAutomationBridge_LevelHandlersDirtyPackageLoad.h"

#include "Editor.h"
#include "Engine/World.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"

#include "Safety/McpSafeOperationsMapLoad.h"

using McpSafeOperations::McpSafeLoadMap;

namespace McpLevelHandlers {
bool HandleCreateNewLevelAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
    FString LevelName;
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("levelName"), LevelName);

    // SECURITY: Sanitize LevelName to prevent path injection
    // Remove any path separators (only allow the final name component)
    // and reject traversal sequences
    if (!LevelName.IsEmpty()) {
      int32 LastSlash = -1;
      LevelName.FindLastChar(TEXT('/'), LastSlash);
      if (LastSlash >= 0) {
        LevelName = LevelName.RightChop(LastSlash + 1);
      }
      LevelName.FindLastChar(TEXT('\\'), LastSlash);
      if (LastSlash >= 0) {
        LevelName = LevelName.RightChop(LastSlash + 1);
      }
      if (LevelName.Contains(TEXT(".."))) {
        Subsystem.SendAutomationResponse(
            RequestingSocket, RequestId, false,
            TEXT("Invalid levelName: contains path traversal (..)"),
            nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
      }
    }

    // savePath is the published alias: create_level declared it but read only
    // levelPath, so a savePath call quietly landed in /Game/Maps.
    const FString LevelPath = McpGetFirstStringField(Payload, {TEXT("levelPath"), TEXT("savePath")});

    // Parse useWorldPartition - default to false for faster level creation
    // World Partition levels take 20+ seconds to unload in UE 5.7
    bool bUseWorldPartition = false;
    if (Payload.IsValid()) {
      Payload->TryGetBoolField(TEXT("useWorldPartition"), bUseWorldPartition);
    }

    // SECURITY: Sanitize LevelPath to prevent path traversal attacks
    // Rejects paths containing "..", double slashes, or invalid characters
    // that could cause engine crashes or security violations
    FString SanitizedLevelPath = SanitizeProjectRelativePath(LevelPath);
    if (!LevelPath.IsEmpty() && SanitizedLevelPath.IsEmpty()) {
      Subsystem.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          McpPathRefusalMessage(TEXT("levelPath"), LevelPath),
          nullptr, TEXT("SECURITY_VIOLATION"));
      return true;
    }

    // CRITICAL FIX: Properly combine levelPath (parent directory) and levelName
    // If both are provided, levelPath is the parent directory and levelName is the level name
    // If only levelName is provided and it starts with '/', it's treated as a full path
    // If only levelPath is provided, it's treated as a full path (backwards compatibility)
    FString SavePath;

    if (!SanitizedLevelPath.IsEmpty() && !LevelName.IsEmpty() &&
        FPaths::GetBaseFilename(SanitizedLevelPath).Equals(LevelName, ESearchCase::IgnoreCase)) {
      // The folder already ends in the level name: it is the full path.
      SavePath = SanitizedLevelPath;
    } else if (!SanitizedLevelPath.IsEmpty() && !LevelName.IsEmpty()) {
      // Both provided: levelPath is parent directory, levelName is the level name
      // Combine them: /Game/MCPTest + TestLevel = /Game/MCPTest/TestLevel
      SavePath = SanitizedLevelPath;
      if (!SavePath.EndsWith(TEXT("/"))) {
        SavePath += TEXT("/");
      }
      SavePath += LevelName;
    } else if (!LevelName.IsEmpty()) {
      // Only levelName provided
      if (LevelName.StartsWith(TEXT("/"))) {
        // levelName is actually a full path
        SavePath = LevelName;
      } else {
        // Just the name - save to default location
        SavePath = FString::Printf(TEXT("/Game/Maps/%s"), *LevelName);
      }
    } else if (!SanitizedLevelPath.IsEmpty()) {
      // Only levelPath provided - treat as full path (backwards compatibility)
      SavePath = SanitizedLevelPath;
    }

    if (SavePath.IsEmpty()) {
      Subsystem.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("levelName or levelPath required for create_level"), nullptr,
          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // create_level ends by loading the level, which replaces the open one:
    // saveDirtyPackages saves unsaved work first instead of dropping it.
    bool bSaveDirtyPackages = false;
    Payload->TryGetBoolField(TEXT("saveDirtyPackages"), bSaveDirtyPackages);
    if (bSaveDirtyPackages) {
      int32 WorldBefore = 0, ContentBefore = 0, WorldAfter = 0, ContentAfter = 0, Failed = 0;
      if (!SaveBlockingDirtyPackagesForLevelLoad(WorldBefore, ContentBefore, WorldAfter, ContentAfter, Failed)) {
        Subsystem.SendAutomationResponse(
            RequestingSocket, RequestId, false,
            FString::Printf(TEXT("saveDirtyPackages: %d package(s) failed to save and %d remain dirty; nothing was created. Save or discard them, then retry"),
                            Failed, WorldAfter + ContentAfter),
            nullptr, TEXT("DIRTY_PACKAGES"));
        return true;
      }
    }
    // Both branches below end in a load; refused here, nothing is created.
    TSharedPtr<FJsonObject> LossDetails;
    const FString Loss = McpSafeOperations::McpRefuseLoadOverUnsavedLevels(
        SavePath, TEXT("Pass saveDirtyPackages true to save them first, or save them with manage_level save."), LossDetails);
    if (!Loss.IsEmpty()) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false, Loss, LossDetails, TEXT("DIRTY_PACKAGES"));
      return true;
    }

    // Check if map already exists
    if (FPackageName::DoesPackageExist(SavePath)) {
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetStringField(TEXT("levelPath"), SavePath);
      Resp->SetStringField(TEXT("packagePath"), SavePath);
      Resp->SetBoolField(TEXT("alreadyExists"), true);
      const bool bLoaded = McpSafeLoadMap(SavePath, true);
      Resp->SetBoolField(TEXT("loaded"), bLoaded);
      if (bLoaded && GEditor && GEditor->GetEditorWorldContext().World()) {
        UWorld* LoadedWorld = GEditor->GetEditorWorldContext().World();
        if (LoadedWorld && LoadedWorld->GetOutermost()) {
          Resp->SetStringField(TEXT("currentLevelPath"), LoadedWorld->GetOutermost()->GetName());
        }
      }
      Subsystem.SendAutomationResponse(
          RequestingSocket, RequestId, bLoaded,
          bLoaded ? FString::Printf(TEXT("Level already exists and was loaded: %s"), *SavePath)
                  : FString::Printf(TEXT("Level already exists but could not be loaded: %s"), *SavePath),
          Resp, bLoaded ? FString() : TEXT("LOAD_FAILED"));
      return true;
    }

    // UE 5.7: GEditor->NewMap can assert while destroying the current editor
    // world if TickTaskManager still tracks a level from a previous automation
    // map transition. The manage_level_structure create_level path creates and
    // saves an inactive UWorld package without switching the editor world, so it
    // avoids EditorDestroyWorld/NewMap entirely while still producing a real
    // level asset that manage_level load/stream/export actions can use.
    TSharedPtr<FJsonObject> CreatePayload = MakeShared<FJsonObject>();
    CreatePayload->SetStringField(TEXT("subAction"), TEXT("create_level"));
    CreatePayload->SetStringField(TEXT("levelName"), FPaths::GetBaseFilename(SavePath));
    CreatePayload->SetStringField(TEXT("levelPath"), FPaths::GetPath(SavePath));
    CreatePayload->SetBoolField(TEXT("bCreateWorldPartition"), bUseWorldPartition);
    CreatePayload->SetBoolField(TEXT("save"), true);
    CreatePayload->SetBoolField(TEXT("loadAfterCreate"), true);
    return FMcpLevelHandlerAccess::ManageLevelStructure(
        Subsystem, RequestId, TEXT("manage_level_structure"), CreatePayload,
        RequestingSocket);
}
} // namespace McpLevelHandlers
