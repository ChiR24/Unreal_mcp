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
// op begin | status | rollback | end; label names the restore point on begin. Run names the request a script's own
// restore point belongs to (run_script passes its request id): its rollback undoes only that request's steps, so a step
// another call made while the script waited on a step that answers later is in the way as a user's is, and no other
// call may begin, roll back or end it while the script runs.
bool Handle(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& Run = FString());

// Closes an open restore point and unbinds its engine delegates: a module unload (Live Coding) must leave none behind.
void Shutdown();
}
