#include "McpAutomationBridge_LevelHandlersActions.h"

#include "Editor.h"
#include "Engine/World.h"

#include "McpSafeOperationsMapLoad.h"

using McpSafeOperations::McpSafeLoadMap;

namespace McpLevelHandlers {
#if WITH_EDITOR
#define SendAutomationResponse(...) Subsystem.SendAutomationResponse(__VA_ARGS__)
#define SendAutomationError(...) Subsystem.SendAutomationError(__VA_ARGS__)
#define HandleExecuteEditorFunction(...) Subsystem.HandleExecuteEditorFunction(__VA_ARGS__)
#define HandleManageLevelStructureAction(...) Subsystem.HandleManageLevelStructureAction(__VA_ARGS__)
#define HandleSetMetadata(...) Subsystem.HandleSetMetadata(__VA_ARGS__)
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
        SendAutomationResponse(
            RequestingSocket, RequestId, false,
            TEXT("Invalid levelName: contains path traversal (..)"),
            nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
      }
    }

    FString LevelPath;
    if (Payload.IsValid())
      Payload->TryGetStringField(TEXT("levelPath"), LevelPath);

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
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("Invalid levelPath: contains path traversal (..), double slashes, or invalid characters"),
          nullptr, TEXT("SECURITY_VIOLATION"));
      return true;
    }

    // CRITICAL FIX: Properly combine levelPath (parent directory) and levelName
    // If both are provided, levelPath is the parent directory and levelName is the level name
    // If only levelName is provided and it starts with '/', it's treated as a full path
    // If only levelPath is provided, it's treated as a full path (backwards compatibility)
    FString SavePath;

    if (!SanitizedLevelPath.IsEmpty() && !LevelName.IsEmpty()) {
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
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("levelName or levelPath required for create_level"), nullptr,
          TEXT("INVALID_ARGUMENT"));
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
      SendAutomationResponse(
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
#undef SendAutomationResponse
#undef SendAutomationError
#undef HandleExecuteEditorFunction
#undef HandleManageLevelStructureAction
#undef HandleSetMetadata
#endif
} // namespace McpLevelHandlers
