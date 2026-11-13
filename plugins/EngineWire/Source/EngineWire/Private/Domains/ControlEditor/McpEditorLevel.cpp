#include "Domains/ControlEditor/McpEditorSupport.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"
#include "Safety/McpSafeOperationsLevelFile.h"

#include "ContentStreaming.h"
#include "Misc/PackageName.h"

namespace {
constexpr int32 McpLevelSettleFrames = 10;
constexpr double McpLevelSettleMaxSeconds = 8.0;
} // namespace

// A level's water, streamed textures and effects take some frames to appear after it loads: a picture taken at once
// showed black water and placeholder textures. The reply waits McpLevelSettleFrames frames, and for texture streaming
// to go quiet, at most McpLevelSettleMaxSeconds, and says how long it waited.
void McpReplyWhenLevelSettles(TWeakObjectPtr<UEngineWireSubsystem> WeakThis, TSharedPtr<FMcpBridgeWebSocket> Socket,
                              const FString &RequestId, const FString &Message, TSharedPtr<FJsonObject> Resp,
                              bool bRestoreGameView) {
  FEditorViewportClient *View = bRestoreGameView ? GetActiveEditorViewportClientForMcp() : nullptr;
  if (View && !View->IsInGameView()) {
    View->SetGameView(true);
  }
  const double Start = FPlatformTime::Seconds();
  int32 Frames = 0;
  FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis, Socket, RequestId, Message, Resp, Start, Frames](float) mutable {
    if (!WeakThis.IsValid()) {
      return false;
    }
    const double Waited = FPlatformTime::Seconds() - Start;
    const int32 Streaming = IStreamingManager::Get().GetNumWantingResources();
    if (++Frames < McpLevelSettleFrames || (Streaming > 0 && Waited < McpLevelSettleMaxSeconds)) {
      return true;
    }
    Resp->SetNumberField(TEXT("settledSeconds"), McpRoundTo(Waited, 100.0));
    if (Streaming > 0) {
      Resp->SetNumberField(TEXT("texturesStillStreaming"), Streaming);
    }
    WeakThis->SendAutomationResponse(Socket, RequestId, true, Message, Resp, FString());
    return false;
  }));
}

bool UEngineWireSubsystem::HandleControlEditorOpenLevel(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString LevelPath;
  if (!McpReadRequiredField(this, Socket, RequestId, Payload, TEXT("levelPath"), LevelPath))
    return true;

  if (!LevelPath.StartsWith(TEXT("/"))) {
    LevelPath = FString::Printf(TEXT("/Game/%s"), *LevelPath);
  }

  const FString LevelPathAsGiven = LevelPath;
  LevelPath = SanitizeProjectRelativePath(LevelPath);
  if (LevelPath.IsEmpty() ||
      !(LevelPath.StartsWith(TEXT("/Game/")) || LevelPath.StartsWith(TEXT("/Engine/")))) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("SECURITY_VIOLATION"),
                              LevelPath.IsEmpty()
                                  ? McpPathRefusalMessage(TEXT("levelPath"), LevelPathAsGiven)
                                  : FString::Printf(TEXT("Invalid levelPath '%s': only a level under /Game or /Engine can be used here."), *LevelPath),
                              nullptr);
    return true;
  }

  if (LevelPath.EndsWith(TEXT(".umap"))) {
    LevelPath.LeftChopInline(5);
  }
  // An asset listing hands out object paths (/Game/Maps/Demo.Demo); one was looked for as a file and not found.
  LevelPath = FPackageName::ObjectPathToPackageName(LevelPath);

  const McpSafeOperations::FMcpLevelFile LevelFile = McpSafeOperations::McpFindLevelFile(LevelPath);
  if (!LevelFile.bFolder && !LevelFile.bFlat) {
    TSharedPtr<FJsonObject> ErrorDetails;
    const FString Missing = McpSafeOperations::McpLevelFileNotFound(LevelPath, LevelFile, ErrorDetails);
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("FILE_NOT_FOUND"), Missing, ErrorDetails);
    return true;
  }
  // The level as the load is handed it and the reply names it: /Game/Path/Name/Name.umap, or /Game/Path/Name.umap.
  const FString MapPathToLoad = LevelFile.bFolder ? LevelPath + TEXT("/") + FPaths::GetBaseFilename(LevelPath) + TEXT(".umap")
                                                 : LevelPath + TEXT(".umap");

  if (McpSafeOperations::McpIsHeadlessEditor()) {
    int32 BlockingWorldPackages = 0;
    int32 BlockingContentPackages = 0;
    McpSafeOperations::McpCountBlockingDirtyPackages(BlockingWorldPackages, BlockingContentPackages);
    if (BlockingWorldPackages + BlockingContentPackages > 0) {
      TSharedPtr<FJsonObject> Details = McpHandlerUtils::CreateResultObject();
      Details->SetNumberField(TEXT("dirtyWorldPackages"), BlockingWorldPackages);
      Details->SetNumberField(TEXT("dirtyContentPackages"), BlockingContentPackages);
      Details->SetStringField(TEXT("levelPath"), LevelPath);
      SendStandardErrorResponse(this, Socket, RequestId, TEXT("DIRTY_PACKAGES"),
                                TEXT("Cannot open a level in unattended/headless mode while packages are dirty. Save or discard changes first."),
                                Details);
      return true;
    }
  }

  TSharedPtr<FJsonObject> LossDetails;
  const FString Loss = McpSafeOperations::McpRefuseLoadOverUnsavedLevels(
      MapPathToLoad, TEXT("Save them first (manage_level save), or open the level with manage_level load, which takes saveDirtyPackages or discardUnsaved."), LossDetails);
  if (!Loss.IsEmpty()) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("DIRTY_PACKAGES"), Loss, LossDetails);
    return true;
  }

  TWeakObjectPtr<UEngineWireSubsystem> WeakThis(this);
  // The level viewport's game view (no editor icons) was switched off by every level load.
  const FEditorViewportClient *ViewBefore = GetActiveEditorViewportClientForMcp();
  const bool bWasGameView = ViewBefore && ViewBefore->IsInGameView();
  FTSTicker::GetCoreTicker().AddTicker(
      FTickerDelegate::CreateLambda([WeakThis, Socket, RequestId, LevelPath,
                                     MapPathToLoad, bWasGameView](float) {
        if (!WeakThis.IsValid()) {
          return false;
        }

        bool bOpened = McpSafeLoadMap(MapPathToLoad);

        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetBoolField(TEXT("success"), bOpened);
        Resp->SetStringField(TEXT("levelPath"), LevelPath);
        Resp->SetStringField(TEXT("loadedPath"), MapPathToLoad);

        if (bOpened) {
          UE_LOG(LogEngineWireSubsystem, Display,
                 TEXT("OpenLevel: Successfully opened level: %s"), *MapPathToLoad);
          McpReplyWhenLevelSettles(WeakThis, Socket, RequestId, TEXT("Level opened"), Resp, bWasGameView);
        } else {
          SendStandardErrorResponse(WeakThis.Get(), Socket, RequestId,
                                    TEXT("OPEN_FAILED"),
                                    TEXT("Failed to open level"), Resp);
        }

        return false;
      }),
      0.0f);
  return true;
}
