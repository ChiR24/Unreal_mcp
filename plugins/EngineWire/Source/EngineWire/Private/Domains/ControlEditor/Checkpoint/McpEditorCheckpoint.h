// McpEditorCheckpoint.h — control_editor restore_point: a point to roll the editor back to that undoes only what calls
// through this server changed. Which steps are this server's comes from the undo ledger (McpUndoLedger), so a rollback
// refuses before undoing anything when a step in the way was the user's, or when the history was cleared since.
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UEngineWireSubsystem;
class FMcpBridgeWebSocket;

namespace McpEditorCheckpoint
{
// op begin | status | rollback | end; label names the restore point on begin.
bool Handle(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket);

// Closes an open restore point and unbinds its engine delegates: a module unload (Live Coding) must leave none behind.
void Shutdown();
}
