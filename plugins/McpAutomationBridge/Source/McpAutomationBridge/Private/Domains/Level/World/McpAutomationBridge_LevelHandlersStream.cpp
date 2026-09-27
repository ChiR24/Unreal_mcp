#include "Domains/Level/McpAutomationBridge_LevelHandlersActions.h"
#include "Domains/Level/Lifecycle/McpAutomationBridge_LevelHandlersPathSafety.h"

#include "Editor.h"
#include "EditorLevelUtils.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"

namespace McpLevelHandlers {
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
      Subsystem.SendAutomationResponse(
          RequestingSocket, RequestId, false,
          TEXT("stream_level requires levelName or levelPath"), nullptr,
          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // CRITICAL FIX: Use UEditorLevelUtils for streaming instead of console command
    // Console command StreamLevel is unreliable and returns EXEC_FAILED in many cases
    if (!GEditor) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("Editor not available"), nullptr,
                             TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World) {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No world loaded"), nullptr,
                             TEXT("NO_WORLD"));
      return true;
    }

    ULevelStreaming* TargetStreamingLevel = nullptr;
    FString NormalizedLevelName = LevelName;

    if (NormalizedLevelName.EndsWith(TEXT(".umap"))) {
      NormalizedLevelName = NormalizedLevelName.LeftChop(5);
    }

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
      TargetStreamingLevel->SetShouldBeLoaded(bLoad);
      TargetStreamingLevel->SetShouldBeVisible(bVis);

      Result->SetStringField(TEXT("streamingState"),
          TargetStreamingLevel->IsStreamingStatePending() ? TEXT("Pending") :
          TargetStreamingLevel->IsLevelLoaded() ? TEXT("Loaded") : TEXT("Unloaded"));

      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true,
                             FString::Printf(TEXT("Streaming level state updated: %s (Loaded=%s, Visible=%s)"),
                                 *NormalizedLevelName,
                                 bLoad ? TEXT("true") : TEXT("false"),
                                 bVis ? TEXT("true") : TEXT("false")),
                             Result);
    } else {
      Subsystem.SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("Streaming level not found in the editor world: %s (add it with add_sublevel first)"),
                                             *NormalizedLevelName),
                             Result, TEXT("STREAMING_LEVEL_NOT_FOUND"));
    }
    return true;
}
} // namespace McpLevelHandlers
