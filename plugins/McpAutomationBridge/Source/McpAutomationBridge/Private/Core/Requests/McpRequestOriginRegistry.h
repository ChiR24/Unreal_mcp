// McpRequestOriginRegistry.h — RequestId -> originating transport
//
// WHY THIS EXISTS. The subsystem's `CurrentRequestOrigin` member is only valid
// for the duration of ONE SYNCHRONOUS `ProcessAutomationRequest` dispatch: its
// ON_SCOPE_EXIT resets it to `WebSocket` the moment the handler function
// returns. A handler that DEFERS its reply (AsyncTask, timer, delegate,
// latent completion) therefore answers after the reset, and
// `SendAutomationResponse` resolved a native /mcp request onto the WebSocket
// delivery path, where it was dropped ("Failed to deliver automation_response
// to its originating socket"). The native caller then hung until the 300s SSE
// sweeper turned it into an untyped TIMEOUT — a five-minute stall on a read
// that the same editor answers over stdio in well under a second.
//
// The origin is a property of the REQUEST, not of the current stack frame, so
// it is recorded once at queue admission and read back by RequestId whenever
// the response is finally produced.
//
// This is deliberately NOT a "which transport currently has a pending request
// with this id" lookup. Such a lookup was removed once already because a
// colliding id would let one transport steal another's response. Here the
// admitting call states its own origin, so a collision can only ever overwrite
// an entry with the truth of the later admission — it can never route a
// response to a transport that did not ask for it.

#pragma once

#include "CoreMinimal.h"
#include "McpAutomationBridgeSubsystem.h"

class FMcpRequestOriginRegistry
{
public:
	/** Process-wide instance; the subsystem may be torn down and recreated. */
	static FMcpRequestOriginRegistry& Get();

	/** Record the transport that admitted `RequestId`. */
	void Record(const FString& RequestId, ERequestOrigin Origin);

	/** True when `RequestId` was recorded; `OutOrigin` then names its transport. */
	bool Resolve(const FString& RequestId, ERequestOrigin& OutOrigin) const;

	/** Drop the entry once the response has been routed (or the request died). */
	void Forget(const FString& RequestId);

	void Reset();
	int32 Num() const;

	// A handler that never answers AND is never cancelled would otherwise leak
	// one entry for the life of the editor, so the oldest recording is evicted
	// past this bound. It is far above the 64-deep automation queue, so eviction
	// cannot reach a request that is still legitimately in flight.
	static constexpr int32 MaxTrackedRequests = 512;

private:
	mutable FCriticalSection Mutex;
	TMap<FString, ERequestOrigin> Origins;
	TArray<FString> InsertionOrder;
};
