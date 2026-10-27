// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class AActor;
class FMcpBridgeWebSocket;
class UEngineWireSubsystem;

// manage_sequence auto_edit: a Level Sequence cut between its cameras the way an editor would. The range is split into
// segments; each camera is scored per segment by how well it shows the subjects as the sequence plays them (hidden or
// out of frame, too small or overfilling), each cut by its jump (two cameras within 30 degrees of the main subject) or
// its crossing of the line between the first two subjects, and the cheapest edit whose shots all last between the
// shortest and longest allowed is found by dynamic programming (Cut).
namespace McpAutoEdit
{
/** The camera to hold in each segment, given each segment's shot costs (one per camera) and the cost of cutting from
 *  one camera to another at a segment; every shot lasts MinRun to MaxRun segments (the last one too, unless the whole
 *  range is shorter). Empty when no edit fits. */
TArray<int32> Cut(const TArray<TArray<double>>& Shot, TFunctionRef<double(int32 Segment, int32 From, int32 To)> CutCost, int32 MinRun,
                  int32 MaxRun);

bool Handle(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket, TFunctionRef<AActor*(const FString&)> Find);
} // namespace McpAutoEdit
