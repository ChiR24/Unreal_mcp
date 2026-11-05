#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// What a request left on the editor's undo stack, read from the transaction buffer instead of from what its handler
// said. Handlers that opened a transaction without describing it answered with no word about undo; a transaction that
// changed nothing (or only play-world objects) is dropped by the buffer when it closes, yet its handler had already
// answered undoable; and a compile that cleared the history went unreported by every handler but the Blueprint one.
namespace McpUndoLedger
{
/** Records the transactions the request closes from now until End. */
void Begin(const FString& RequestId);

/** Stops recording; what was recorded stays readable until the next Begin. */
void End();

/**
 * Gives Result an `undo` block from the buffer: the steps the request left (undoable, transactionScope, steps), or
 * that the history was cleared. A handler's own block stands, except one claiming a step the buffer does not hold;
 * nothing is said while a transaction is still open, since what it records is not known yet. A request that answers
 * in a later bracket (Park) counts the steps of every bracket, and those its steps left answering later.
 */
void Describe(const FString& RequestId, const TSharedPtr<FJsonObject>& Result);

/** Whether a transaction was left by a request through this server, this editor session (what a restore point undoes). */
bool IsMcpStep(const FGuid& TransactionId);

/**
 * Whether RequestId left the transaction, or one of its steps did under its own id (<RequestId>#step<n>, a step of a
 * script that answered later in a bracket of its own); IsMcpStep for an empty RequestId.
 */
bool IsStepOf(const FGuid& TransactionId, const FString& RequestId);

/**
 * The request answers in a later bracket (a script waiting on a step that answers later): that bracket's Begin goes on
 * from where this one began, so its reply describes every step the request left. Called inside the request's bracket.
 */
void Park(const FString& RequestId);

/**
 * Most handlers reply inside their scoped transaction, so what it leaves was unknown when Describe ran. The reply of
 * the request being recorded, sent while a transaction is open, is held (true) and sent by Flush once the handler has
 * returned and the transaction closed; a second reply to that request is dropped, as the first would have won.
 */
bool Hold(const FString& RequestId, TFunction<void()> Send);

/** Sends the held reply, if any. Called first on every exit of the request, while its captures still run. */
void Flush();
} // namespace McpUndoLedger
