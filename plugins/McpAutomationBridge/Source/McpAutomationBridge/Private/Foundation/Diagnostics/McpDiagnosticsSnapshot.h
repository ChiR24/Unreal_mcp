#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Foundation/Diagnostics/McpDiagnosticsSnapshotSchema.h"

namespace McpDiagnosticsSchema
{
	// True once any request, refusal, handshake, disconnect or session was recorded.
	inline bool HasRecordedEvents(const FMcpDiagnosticsSnapshotState& State)
	{
		return State.Requests > 0 || State.Refusals > 0 || State.bHasRequest
			|| State.bHasHandshake || State.bHasDisconnect || State.bHasSession;
	}
}

// The state record lives in the schema namespace with its allowlist helpers;
// this class and its .cpp TUs reference it unqualified.
using McpDiagnosticsSchema::FMcpDiagnosticsSnapshotState;

class FJsonObject;

// Todo 9 (BB-005) lane 1 - plugin-only diagnostics snapshot store.
//
// The Unreal plugin is the SOLE writer of <Project>/Saved/MCP/diagnostics/
// current-session.json and previous-session.json (64 KiB max each). The
// TypeScript surface is read-only (src/automation/diagnostics-snapshot-reader.ts)
// and can never write these files.
//
// The store is a proper .h/.cpp singleton (function-local static in the .cpp,
// one FCriticalSection) mirroring FMcpTelemetryRegistry. The clock and the
// diagnostics root are injectable so the native automation test is exact; the
// root is cached during game-thread initialization and socket-thread hooks
// never resolve project paths or touch editor APIs.
//
// Recorded fields are typed and bounded (see McpDiagnosticsSnapshotSchema.h):
// instance attribution, counters, the last request's correlation id / canonical
// action / origin / queue depth / timestamps / terminal class, and bounded
// handshake, disconnect, and session summaries. The on-disk record physically
// cannot hold payload, code, path, capability/scoped token, principal identity,
// raw idempotency key, or raw session credential - there is no field or setter
// for them. A native session is recorded only as a truncated SHA-256 identity.
//
// Queue admission records memory only; PersistCurrent() atomically refreshes
// the on-disk current-session.json (same-directory fixed temp + rename) and is
// the call a hook makes immediately before mutation dispatch so a hard crash
// leaves the last pre-dispatch record. Startup rotation is crash-tolerant:
// valid temps are recovered only when their target is missing/invalid, then
// surviving fixed temps are removed; a valid non-empty current is promoted to
// previous (exactly one), then a fresh current is initialized. Corrupt or
// oversized files are ignored with one bounded typed warning naming the path
// only - no quarantine, no accumulation, no JSON slicing, no fsync.

class FMcpDiagnosticsSnapshot
{
public:
	static FMcpDiagnosticsSnapshot& Get();

	/** Seconds-resolution clock; defaults to UTC epoch seconds. */
	void SetClock(TFunction<double()> InClock);

	/** Test-only diagnostics root override; empty string clears it. */
	void SetRootOverride(const FString& InRoot);

	/** Clears memory state, cached root, and override. Keeps nothing secret. */
	void Reset();

	/** Game-thread init: cache the diagnostics root once. */
	void InitializeFromGameThread();

	/**
	 * Startup rotation (call after the commandlet early-return, before request
	 * acceptance): recover valid temps, promote a non-empty valid current to
	 * previous, then initialize a fresh current.
	 */
	void RotateOnStartup();

	/** Queue admission: memory counters only (no disk write). */
	void RecordAdmission(
		const FString& RequestId,
		const FString& CorrelationId,
		const FString& CanonicalAction,
		const FString& Origin,
		int32 QueueDepth);

	/** Immediate pre-dispatch refresh (queue depth + dispatch timestamp). */
	void RecordPreDispatch(const FString& RequestId, int32 QueueDepth);

	/** Queue refusal: refusal class + queue depth. */
	void RecordRefusal(const FString& RequestId, const FString& RefusalCode, int32 QueueDepth);

	/** Terminal result: success/failure code only, never a message. */
	void RecordTerminal(const FString& RequestId, const FString& TerminalClass);

	/** WebSocket handshake success/failure summary. */
	void RecordHandshake(bool bSuccess);

	/** WebSocket disconnect summary (bounded reason). */
	void RecordDisconnect(const FString& Reason);

	/** Native session created: stores only a truncated SHA-256 identity. */
	void RecordSessionCreated(const FString& RawNativeSession);

	/** Native session closed: bounded count update. */
	void RecordSessionClosed();

	/**
	 * Atomically refresh current-session.json from the in-memory record.
	 * Same-directory fixed temp + rename; never called on the socket thread.
	 */
	bool PersistCurrent();

	/** PersistCurrent() hopped onto the game thread; safe from any thread. */
	static void PersistCurrentAsync();

	/**
	 * Coalesced persist: only writes if bDirty is set AND at least
	 * CoalesceIntervalSeconds has elapsed since the last persist. Called
	 * from the game-thread response funnel instead of PersistCurrent() to
	 * avoid blocking disk I/O on every tool response.
	 */
	bool TryPersistCoalesced();

	/** Bounded read-only summary of the current record (presenters). */
	TSharedRef<FJsonObject> CurrentSummaryJson() const;

	/** Bounded read-only summary of the previous record, or empty. */
	TSharedRef<FJsonObject> PreviousSummaryJson() const;

	static int32 MaxSnapshotBytes() { return McpDiagnosticsSchema::MaxSnapshotBytes; }

private:
	FString DiagnosticsRoot() const;
	bool EnsureDiagnosticsDirectory() const;
	bool WriteFileAtomic(const FString& TargetName, const FString& TempName, const FString& Content) const;
	bool LoadAndValidateFile(const FString& FileName, FString& OutContent, FMcpDiagnosticsSnapshotState& OutState) const;
	void RecoverTempFor(const FString& TargetName, const FString& TempName);
	void RemoveSurvivingTemps();
	void InitializeFreshCurrent();
	void WarnOnce(const FString& FileName, const TCHAR* Reason, bool& bWarned) const;
	void EnsureInstanceAttributionLocked();
	double Now() const;

	mutable FCriticalSection Mutex;
	TFunction<double()> Clock;
	FString RootOverride;
	FString CachedRoot;
	bool bRootCached = false;

	FString InstanceId;
	int32 Pid = 0;
	FString StartTimeUtc;

	FMcpDiagnosticsSnapshotState State;
	FMcpDiagnosticsSnapshotState PreviousState;
	bool bHasPrevious = false;
	bool bDirty = false;
	double LastPersistTime = 0.0;
	static constexpr double CoalesceIntervalSeconds = 1.0;

	// Warn-once caches are set from const validation helpers, hence mutable.
	mutable bool bWarnedAboutCurrent = false;
	mutable bool bWarnedAboutPrevious = false;
};
