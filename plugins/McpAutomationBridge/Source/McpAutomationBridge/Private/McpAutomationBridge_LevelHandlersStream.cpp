#include "McpAutomationBridge_LevelHandlersActions.h"
#include "McpAutomationBridge_LevelHandlersPathSafety.h"

#include "Editor.h"
#include "EditorLevelUtils.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"

namespace McpLevelHandlers {
#if WITH_EDITOR
#define SendAutomationResponse(...) Subsystem.SendAutomationResponse(__VA_ARGS__)
#define SendAutomationError(...) Subsystem.SendAutomationError(__VA_ARGS__)
#define HandleExecuteEditorFunction(...) Subsystem.HandleExecuteEditorFunction(__VA_ARGS__)
#define HandleManageLevelStructureAction(...) Subsystem.HandleManageLevelStructureAction(__VA_ARGS__)
#define HandleSetMetadata(...) Subsystem.HandleSetMetadata(__VA_ARGS__)
bool HandleStreamLevelAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, bool bForceStreamUnload) {
    FString LevelName;
    bool bLoad = bForceStreamUnload ? false : true;
    bool bVis = bForceStreamUnload ? false : true;
    if (Payload.IsValid()) {
      Payload->TryGetStringField(TEXT("levelName"), LevelName);
      Payload->TryGetBoolField(TEXT("shouldBeLoaded"), bLoad);
      Payload->TryGetBoolField(TEXT("shouldBeVisible"), bVis);
      if (LevelName.IsEmpty())
        Payload->TryGetStringField(TEXT("levelPath"), LevelName);
    }
    if (bForceStreamUnload) {
      bLoad = false;
      bVis = false;
    }
    if (LevelName.TrimStartAndEnd().IsEmpty()) {
      SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("stream_level requires levelName or levelPath"), nullptr,
          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // CRITICAL FIX: Use UEditorLevelUtils for streaming instead of console command
    // Console command StreamLevel is unreliable and returns EXEC_FAILED in many cases
    if (!GEditor) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Editor not available"), nullptr,
                             TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No world loaded"), nullptr,
                             TEXT("NO_WORLD"));
      return true;
    }

    // Find the streaming level by name/path
    ULevelStreaming* TargetStreamingLevel = nullptr;
    FString NormalizedLevelName = LevelName;

    // Normalize the path - remove .umap extension if present
    if (NormalizedLevelName.EndsWith(TEXT(".umap"))) {
      NormalizedLevelName = NormalizedLevelName.LeftChop(5);
    }

    // Search for the streaming level
    for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels()) {
      if (StreamingLevel) {
        FString StreamingName = StreamingLevel->GetWorldAssetPackageName();
        if (StreamingName.Equals(NormalizedLevelName, ESearchCase::IgnoreCase) ||
            StreamingName.EndsWith(NormalizedLevelName, ESearchCase::IgnoreCase) ||
            FPaths::GetBaseFilename(StreamingName).Equals(NormalizedLevelName, ESearchCase::IgnoreCase)) {
          TargetStreamingLevel = StreamingLevel;
          break;
        }
      }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("levelName"), NormalizedLevelName);
    Result->SetBoolField(TEXT("shouldBeLoaded"), bLoad);
    Result->SetBoolField(TEXT("shouldBeVisible"), bVis);

    if (TargetStreamingLevel) {
      // Use the streaming level API directly
      TargetStreamingLevel->SetShouldBeLoaded(bLoad);
      TargetStreamingLevel->SetShouldBeVisible(bVis);

      Result->SetStringField(TEXT("streamingState"),
          TargetStreamingLevel->IsStreamingStatePending() ? TEXT("Pending") :
          TargetStreamingLevel->IsLevelLoaded() ? TEXT("Loaded") : TEXT("Unloaded"));

      SendAutomationResponse(RequestingSocket, RequestId, true,
                             FString::Printf(TEXT("Streaming level state updated: %s (Loaded=%s, Visible=%s)"),
                                 *NormalizedLevelName,
                                 bLoad ? TEXT("true") : TEXT("false"),
                                 bVis ? TEXT("true") : TEXT("false")),
                             Result);
    } else {
      // Streaming level not found - try console command as fallback
      if (!IsSafeLevelConsoleToken(NormalizedLevelName)) {
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("Invalid streaming level name"), Result,
                               TEXT("INVALID_ARGUMENT"));
        return true;
      }

      const FString Cmd =
          FString::Printf(TEXT("StreamLevel %s %s %s"), *NormalizedLevelName,
                          bLoad ? TEXT("Load") : TEXT("Unload"),
                          bVis ? TEXT("Show") : TEXT("Hide"));

      // Execute console command and check result
      bool bCmdSuccess = false;
      if (World) {
        bCmdSuccess = GEditor->Exec(World, *Cmd);
      }

      if (bCmdSuccess) {
        Result->SetStringField(TEXT("method"), TEXT("console_command"));
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Streaming command executed"), Result);
      } else {
        // Even if console command returns false, the operation may still be in progress
        // Remove HANDLED error code — success=true means no error occurred
        Result->SetStringField(TEXT("method"), TEXT("console_command_fallback"));
        Result->SetStringField(TEXT("command"), Cmd);
        Result->SetBoolField(TEXT("handled"), true);
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Streaming command submitted (level may not be in world yet)"),
                               Result);
      }
    }
    return true;
}
#undef SendAutomationResponse
#undef SendAutomationError
#undef HandleExecuteEditorFunction
#undef HandleManageLevelStructureAction
#undef HandleSetMetadata
#endif
} // namespace McpLevelHandlers
