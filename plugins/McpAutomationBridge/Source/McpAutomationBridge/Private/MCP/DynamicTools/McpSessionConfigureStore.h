// McpSessionConfigureStore.h
// Task 36 (native mirror): explicit-session, policy-bounded, revisioned configure
// overlay. Native counterpart of
// src/server/mcp-primitives/session-configure-store.ts and the WRITE side of the
// C1 read contract MCP/Primitives/IMcpCatalogRevisionReader.h. Metadata/logic
// only: NO transport wiring and NO session/lifecycle edits (Task 37 supplies
// native session ids; Task 34 consumes the read side). Each session gets an
// INDEPENDENT overlay cloned lazily from an immutable seed; the global dynamic
// tool manager is never referenced or mutated.
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "MCP/Primitives/IMcpCatalogRevisionReader.h"

/**
 * Per-session configure overlay. Visibility mutations reproduce the same
 * protected/core/no-op rules as the global manager but on this session's own
 * maps, and advance the session revision exactly once per effective batch;
 * bounded limit/preference changes never advance it.
 */
class FMcpSessionConfigureStore : public IMcpCatalogRevisionReader
{
public:
	struct FSeedEntry
	{
		FString Name;
		FString Category;
	};

	/** Capture the pristine, immutable seed every session overlay is cloned from. */
	void SeedFrom(const TArray<FSeedEntry>& Entries);

	/** C1 read contract: current revision for SessionId (0 = pristine baseline). */
	virtual uint64 GetCatalogStateRevision(const FString& SessionId) const override;

	bool HasSession(const FString& SessionId) const;
	bool ClearSession(const FString& SessionId);

	// Visibility mutations: revisioned (+1 per effective batch, never on a no-op
	// or a rejected protected mutation).
	TSharedPtr<FJsonObject> EnableTools(const FString& SessionId, const TArray<FString>& ToolNames);
	TSharedPtr<FJsonObject> DisableTools(const FString& SessionId, const TArray<FString>& ToolNames);
	TSharedPtr<FJsonObject> DisableCategory(const FString& SessionId, const FString& Category);
	TSharedPtr<FJsonObject> Reset(const FString& SessionId);

	// Non-visibility: bounded, and never advance the revision.
	bool SetLimit(const FString& SessionId, const FString& Key, int64 Value);
	bool SetPreference(const FString& SessionId, const FString& Key, const FString& Value);

	bool IsToolEnabled(const FString& SessionId, const FString& ToolName) const;
	TSharedPtr<FJsonObject> GetStatus(const FString& SessionId) const;

private:
	struct FToolState
	{
		FString Name;
		FString Category;
		bool bEnabled = true;
	};

	struct FCategoryState
	{
		FString Name;
		bool bEnabled = true;
	};

	struct FOverlay
	{
		TMap<FString, FToolState> ToolStates;
		TMap<FString, FCategoryState> CategoryStates;
		TMap<FString, int64> Limits;
		TMap<FString, FString> Preferences;
		uint64 CatalogStateRevision = 0;
	};

	TArray<FSeedEntry> Seed;

	/** Overlays and the lock are mutable so const reads can lazily reseed a session. */
	mutable TMap<FString, FOverlay> Overlays;
	mutable FCriticalSection StateMutex;

	static bool IsProtectedTool(const FString& Name);
	static bool IsProtectedCategory(const FString& Name);
	static FString Fingerprint(const FOverlay& Overlay);
	FOverlay& OverlayFor_NoLock(const FString& SessionId) const;
	bool IsToolEnabled_NoLock(const FOverlay& Overlay, const FString& ToolName) const;
};
