#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// Task 42 live editor-state revisions.
//
// Monotonic counters for the mutable editor state a client can pin a precondition
// against: the active selection, the open level/map, the asset registry, and the
// dirty-package set. Unlike the Task 39 catalog revision - a static build hash the
// gateway thread can safely compare - these change DURING a session, so the
// authoritative read and comparison must happen on the GAME THREAD immediately
// before the mutation. A transport-thread cache would be stale by dispatch time,
// which is exactly the race this exists to close.
//
// The registry is a plain counter store: editor/asset delegates call Advance()
// when state changes (wired separately, on the game thread), and the pre-dispatch
// gate calls CheckPreconditions() with the client's expected values. It holds no
// editor references and reads no editor state itself, so the comparison logic is
// unit-testable without a live world.
enum class EMcpStateKind : uint8
{
	Selection,
	Level,
	AssetRegistry,
	Package
};

using FMcpExpectedRevisions = TMap<EMcpStateKind, int64>;

struct FMcpLiveStateRevisionSnapshot
{
	int64 Selection = 1;
	int64 Level = 1;
	int64 AssetRegistry = 1;
	int64 Package = 1;

	int64 Max() const;
	TSharedRef<FJsonObject> ToJson() const;
};

struct FMcpExpectedRevisionsParseResult
{
	bool bSuccess = true;
	FMcpExpectedRevisions Revisions;
	FString ErrorCode;
	FString Field;
	FString Pointer;
	FString Message;
};

class FMcpLiveStateRevisions
{
public:
	/** Process-wide registry; the counters outlive any one session or reconnect. */
	static FMcpLiveStateRevisions& Get();

	/** Bump one state's revision. Called from editor/asset delegates on the game thread. */
	void Advance(EMcpStateKind Kind);

	/** Current revision for one state. Starts at McpInitialStateRevision. */
	int64 Current(EMcpStateKind Kind) const;

	FMcpLiveStateRevisionSnapshot Snapshot() const;

	/**
	 * Compare a client's expected revisions against the live values. Returns true
	 * when every expected key matches (an absent key is not pinned, so it never
	 * refuses); on a mismatch returns false and fills OutKind/OutExpected/OutCurrent
	 * with the FIRST stale state, so the refusal names a concrete current reference.
	 * Pure and side-effect free: it never mutates and never touches the editor.
	 */
	bool CheckPreconditions(
		const TMap<EMcpStateKind, int64>& Expected,
		EMcpStateKind& OutKind,
		int64& OutExpected,
		int64& OutCurrent) const;

	/** Reset all counters. Automation-test isolation only. */
	void Reset();

	/** The wire/URI token for a state, so receipts and options share one spelling. */
	static const TCHAR* KeyFor(EMcpStateKind Kind);

	/** Parse a wire token back to its state kind; false for an unknown key. */
	static bool KindFor(const FString& Key, EMcpStateKind& OutKind);

	static FMcpExpectedRevisionsParseResult ParseExpectedRevisions(
		const TSharedPtr<FJsonValue>& Field);

	/** Every wire token, so a refusal can name exactly what is pinnable. */
	static const TArray<FString>& AllKeys();

	static const TCHAR* StaleStateErrorCode();

	static constexpr int64 McpInitialStateRevision = 1;

private:
	mutable FCriticalSection Mutex;
	TMap<EMcpStateKind, int64> Revisions;
};
