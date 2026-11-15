#include "Domains/ControlEditor/McpEditorSupport.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"

#include "ContentStreaming.h"

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
