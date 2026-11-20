#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

class FMcpBridgeWebSocket;
class UEngineWireSubsystem;

// startPlay (S12's Play that rides on the call): sample_motion with no Play running starts one, runs once it has begun,
// and ends it after the run, so one call plays, judges and stops; a Play already running is used and left running.
// Run is the handler entered again once play has begun; a refusal it sends before its run takes the Play over ends it.
void McpSampleMotionInOwnPlay(UEngineWireSubsystem* Self, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
                              TFunction<void()> Run);

// True once for a request whose Play McpSampleMotionInOwnPlay started: its run ends that Play when it ends.
bool McpMotionTakeOwnPlay(const FString& RequestId);

// End the Play this server started for a run, then Then(whether it ended within 20 s), so the reply never comes while
// the play world still runs and the next edit lands in it.
void McpEndOwnPlayThen(TFunction<void(bool)> Then);
