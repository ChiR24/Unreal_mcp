#pragma once

#include "CoreMinimal.h"

class ALandscape;
class FJsonObject;
class FMcpBridgeWebSocket;
class FMcpScopedEditorTransaction;
class ULandscapeInfo;
class UEngineWireSubsystem;

namespace McpLandscapeHandlers {
// The landscape the payload names by landscapeName (actor label) or landscapePath. Replies and returns null when the
// path is unsafe, nothing matches, or OutInfo is requested and the landscape has no LandscapeInfo.
ALandscape *ResolveLandscapeOrReply(UEngineWireSubsystem &Bridge,
                                    const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
                                    ULandscapeInfo **OutInfo = nullptr);

// A height or paint edit made through FLandscapeEditDataInterface, done as the editor's own tools do it. With edit
// layers (every landscape from 5.6, most before) heights and paint live in a layer that a merge turns into what renders
// and collides; an edit that named no layer was never drawn or collided with, and the next merge wrote over it. The
// edit goes to the layer being edited, else the first, in one undo step, and the layers merge when the scope closes
// (on a later editor tick with bDeferMerge). Construct it before the FLandscapeEditDataInterface: the interface then
// reads and writes that layer, and is flushed before the merge.
class FMcpLandscapeEditScope {
public:
  FMcpLandscapeEditScope(ALandscape *InLandscape, const TCHAR *Title, bool bInDeferMerge);
  ~FMcpLandscapeEditScope();
  FMcpLandscapeEditScope(const FMcpLandscapeEditScope &) = delete;
  FMcpLandscapeEditScope &operator=(const FMcpLandscapeEditScope &) = delete;
  // The reply's undo block.
  void DescribeInto(const TSharedPtr<FJsonObject> &Result) const;

private:
  TUniquePtr<FMcpScopedEditorTransaction> Transaction;
  ALandscape *Landscape = nullptr;
  FGuid PreviousLayer;
  bool bLayered = false;
  bool bDeferMerge = false;
};
}
