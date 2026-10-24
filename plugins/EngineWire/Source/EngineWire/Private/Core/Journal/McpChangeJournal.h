// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FMcpBridgeWebSocket;
class UEngineWireSubsystem;

// What changed in the editor since a cursor: level actors added, deleted or modified (with the properties edited) and
// assets created, removed, renamed, modified or saved, each marked by who changed it. An agent listed the level again
// every turn to learn what had moved, and never heard what the user did in the editor between two of its calls.
//
// Who: mcp inside a request through this server; user inside an editor transaction no request opened (every edit made
// in the editor's UI opens one, and so does its undo and redo); editor for the rest. What the engine does to actors and
// objects on its own (opening a level, compiling, derived data) is not kept. Assets and levels created, removed, renamed,
// saved or opened are, by whoever: saving, importing or opening a level is no transaction, so it reads as editor.
namespace McpChangeJournal
{
/** Binds the engine's change events (subsystem Initialize). */
void Start();
/** Unbinds them (subsystem Deinitialize). */
void Stop();

/** Whether a request through this server is running now (game thread): what changes now is this server's doing. */
bool InRequest();

/** A request through this server starts (game thread, before its handler); Action names it in the rows it changes. */
void BeginRequest(const FString& RequestId, const FString& Action);
/** The request ended: what changes from now is not its doing. */
void EndRequest();

/** One warning in Result when the user changed something in the editor since the previous request (once per change). */
void AddUserChangeWarning(const FString& RequestId, const TSharedPtr<FJsonObject>& Result);

/** The asset packages the running request RequestId changed so far (made, edited, saved, renamed); none for another. */
void AssetsChangedBy(const FString& RequestId, TArray<FString>& OutPackages);

/** inspect changes_since: what changed after the cursor in since (or the latest changes kept), and the next cursor. */
bool HandleChangesSince(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                        TSharedPtr<FMcpBridgeWebSocket> Socket);

/** The latest change's number, on any thread: it moves whenever a change is kept (0 before the first). */
uint64 LatestSeq();
/** The cursor changes_since would return now, on any thread, and its number; empty while the journal is stopped. */
FString LatestCursor(uint64* OutSeq = nullptr);
} // namespace McpChangeJournal
