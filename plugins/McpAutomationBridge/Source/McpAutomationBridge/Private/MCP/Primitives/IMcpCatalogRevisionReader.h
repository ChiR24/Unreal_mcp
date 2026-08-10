// IMcpCatalogRevisionReader.h
// Task 36 primitive C1 (native mirror): explicit-session catalog state revision
// reader. Native counterpart of
// src/server/mcp-primitives/catalog-revision-reader.ts. Metadata/contract only:
// NO transport wiring and NO session/lifecycle edits (Task 37 wires native
// session ids; Task 34 consumes this read side). It declares only the
// per-session, read-only revision contract; the revisioned WRITE side is
// FMcpSessionConfigureStore
// (Private/MCP/DynamicTools/McpSessionConfigureStore.h).
#pragma once

#include "CoreMinimal.h"

/**
 * Read-only, per-session catalog state revision source. The revision advances
 * exactly once per effective visibility mutation batch for a session, and never
 * for a no-op, a rejected protected mutation, or a limit/preference-only
 * (non-visibility) change.
 *
 * The session id is REQUIRED with no default and no global fallback and there is
 * no no-arg overload, so one session's revision can never be read as another's.
 * An unknown session reads the pristine baseline (0).
 */
class IMcpCatalogRevisionReader
{
public:
	virtual ~IMcpCatalogRevisionReader() = default;

	/** Current catalog state revision for SessionId (0 = pristine baseline). */
	virtual uint64 GetCatalogStateRevision(const FString& SessionId) const = 0;
};
