// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"
#include "McpFabProvider.h"

/**
 * The background imports this editor session started through the Fab adapter.
 *
 * An add replies as soon as Fab accepts the download, because the download and the
 * import that follows take minutes and hold the game thread -- far past any client's
 * request timeout. What happens afterwards is kept here, one record per add, so a
 * separate status read can say whether the import is still resolving, downloading,
 * importing, or has finished or failed.
 *
 * Every caller runs on the game thread (the page's reply, the watcher's ticker and
 * the status read alike), so the store takes no lock.
 */
namespace McpFabImportOperations
{
/** Registers a new add and returns its operation id. It is `resolving` until the page answers. */
FString Begin(const FString& ListingId);

/** The page accepted the download; Accepted carries what it chose (format, tier, file, size). */
void Accept(const FString& OperationId, const FMcpFabAddResult& Accepted);

/** Live count of new assets, so the `importing` phase shows progress. */
void SetAssetsSoFar(const FString& OperationId, int32 Count);

/** Keeps one scrubbed error line Fab logged while the import ran. */
void AddFabError(const FString& OperationId, const FString& Line);

/** The import ended, either way. Outcome replaces the stored result; an ErrorCode marks it failed. */
void Finish(const FString& OperationId, const FMcpFabAddResult& Outcome);

/**
 * The operation still running, if any. There is at most one: the registry diff that decides
 * success cannot tell two imports apart, so a second add waits for the first.
 */
bool FindRunning(const FString& CacheLocation, FMcpFabImportStatus& OutStatus);

/** An operation by id, else the newest one for a listing id. CacheLocation is Fab's download cache. */
bool Find(const FString& Key, const FString& CacheLocation, FMcpFabImportStatus& OutStatus);
} // namespace McpFabImportOperations
