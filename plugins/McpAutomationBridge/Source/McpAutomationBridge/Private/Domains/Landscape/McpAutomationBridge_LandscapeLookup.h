#pragma once

#include "CoreMinimal.h"

class ALandscape;
class FJsonObject;
class FMcpBridgeWebSocket;
class ULandscapeInfo;
class UMcpAutomationBridgeSubsystem;

namespace McpLandscapeHandlers {
// The landscape the payload names by landscapeName (actor label) or landscapePath. Replies and returns null when the
// path is unsafe, nothing matches, or OutInfo is requested and the landscape has no LandscapeInfo.
ALandscape *ResolveLandscapeOrReply(UMcpAutomationBridgeSubsystem &Bridge,
                                    const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
                                    ULandscapeInfo **OutInfo = nullptr);
}
