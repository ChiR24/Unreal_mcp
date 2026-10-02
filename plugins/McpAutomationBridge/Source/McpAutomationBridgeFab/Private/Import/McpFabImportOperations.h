// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"
#include "McpFabProvider.h"

/**
 * The background imports this editor session started through the Fab adapter.
 *
 * An add replies as soon as Fab accepts the download, because the download and the import that follows
 * take minutes and hold the game thread -- far past any client's request timeout. What happens
 * afterwards is kept here, one record per add, so a separate status read can say whether the import is
 * queued, resolving, downloading, importing, or has finished or failed.
 *
 * One import runs at a time: the registry diff that decides success cannot tell two apart. An add made
 * while another runs is parked here as `queued` with the closure that starts it, and started, oldest
 * first, when the running one finishes.
 *
 * Every caller runs on the game thread (the page's reply, the watcher's ticker and the status read
 * alike), so the store takes no lock.
 */
namespace McpFabImportOperations
{
/** Registers a new add and returns its operation id. It is `resolving` until the page answers. */
FString Begin(const FString& ListingId, const FString& OptionsKey);

/** The OptionsKey an operation was begun with; empty for an unknown id. */
FString OptionsKeyOf(const FString& OperationId);

/** Parks a just-begun add behind the running import; Launch starts it when its turn comes. */
void Enqueue(const FString& OperationId, TFunction<void()> Launch);

/** How many adds are waiting behind the running one. */
int32 QueuedCount();

/** The page accepted the download; Accepted carries what it chose (format, tier, file, size). */
void Accept(const FString& OperationId, const FMcpFabAddResult& Accepted);

/** Live count of new assets, so the `importing` phase shows progress. */
void SetAssetsSoFar(const FString& OperationId, int32 Count);

/** Keeps one scrubbed error line Fab logged while the import ran. */
void AddFabError(const FString& OperationId, const FString& Line);

/** Records that the adapter switched Interchange's mesh combining off for this import. */
void SetMeshesSeparated(const FString& OperationId, bool bSeparated);

/** The import ended, either way. Outcome replaces the stored result; an ErrorCode marks it failed. */
void Finish(const FString& OperationId, const FMcpFabAddResult& Outcome);

/** Replaces the stored result of an import that has ended with a later one (the follow-up saves); nothing else changes. */
void Amend(const FString& OperationId, const FMcpFabAddResult& Outcome);

/**
 * Stops one import where Fab gives a way to (McpFabImportCancel.cpp). A queued add is dropped at once; a
 * running one is told to stop, and the watcher ends it as CANCELLED on its next tick. False says why not.
 */
bool RequestCancel(const FString& OperationId, FString& OutMessage, FString& OutErrorCode);

/** True when a cancel was accepted for this import; the watcher reads it to end the import as CANCELLED. */
bool IsCancelRequested(const FString& OperationId);

/** The operation running now (resolving, downloading or importing), if any. There is at most one. */
bool FindRunning(const FString& CacheLocation, FMcpFabImportStatus& OutStatus);

/** An operation for this listing that is queued or running, so a repeated add can join it. */
bool FindOpenByListing(const FString& ListingId, const FString& CacheLocation, FMcpFabImportStatus& OutStatus);

/** An operation by id, else the newest one for a listing id. CacheLocation is Fab's download cache. */
bool Find(const FString& Key, const FString& CacheLocation, FMcpFabImportStatus& OutStatus);

/** The running operation, then every queued one, in the order they will start. */
void ListQueue(const FString& CacheLocation, TArray<FMcpFabImportStatus>& OutQueue);
} // namespace McpFabImportOperations
