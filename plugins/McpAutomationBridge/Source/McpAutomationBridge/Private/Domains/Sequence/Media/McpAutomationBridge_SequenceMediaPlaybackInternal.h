#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Templates/SharedPointer.h"

class FMcpBridgeWebSocket;
class UObject;
class UMcpAutomationBridgeSubsystem;

namespace McpSequenceMedia {

UObject *LoadMediaPlayer(const TSharedPtr<FJsonObject> &Payload,
                         FString &OutResolvedPath, FString &OutError);

bool OpenRequestedMedia(UObject *Player,
                        const TSharedPtr<FJsonObject> &Payload,
                        TSharedPtr<FJsonObject> Result,
                        bool &OutOpenRequested, FString &OutErrorCode,
                        FString &OutErrorMessage, FString &OutExpectedUrl);

void StartMediaPlaybackAfterOpen(
    UMcpAutomationBridgeSubsystem *Subsystem, FString RequestId,
    TSharedPtr<FMcpBridgeWebSocket> Socket, UObject *Player,
    TSharedPtr<FJsonObject> Result, FString ExpectedUrl);

bool InvalidatePendingMediaPlayback(UObject *Player, bool bClosePlayer);

}
