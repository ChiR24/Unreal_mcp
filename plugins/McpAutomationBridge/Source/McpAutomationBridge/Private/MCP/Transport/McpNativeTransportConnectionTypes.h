// McpNativeTransportConnectionTypes.h — connection/stream state owned by the transport
//
// These three structs are the per-connection state the native transport tracks:
// one parsed request, one in-flight tools/call SSE stream, one persistent
// notification stream. They live at file scope (not nested in
// FMcpNativeTransport) so the transport's own translation units can name them
// from file-scope helpers, and so the transport header stays within the plugin's
// 250 pure-line ceiling.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include <atomic>

class FSocket;

/** Parsed HTTP request (minimal — only POST/DELETE /mcp). */
struct FParsedHttpRequest
{
	FString Method;      // "GET", "POST", or "DELETE"
	FString Path;        // "/mcp"
	FString Body;
	FString SessionId;   // from Mcp-Session-Id header
	FString Accept;      // from Accept header
	FString CapabilityToken;  // from X-MCP-Capability-Token header
	FString Origin;      // from Origin header for browser CORS validation
	FString ProtocolVersion;  // from MCP-Protocol-Version header (post-initialize)
	int32 ContentLength = 0;
};

/** Active SSE streaming connection for a tools/call request. */
struct FSSEConnection
{
	FSocket* Socket = nullptr;
	TSharedPtr<FJsonValue> JsonRpcId;
	double StartTime = 0.0;
	double TimeoutSeconds = 300.0;
	// TimeoutSeconds is an IDLE budget, not a total one. It used to be measured
	// from StartTime, so a healthy 16-minute Fab import was killed at 300s and
	// reported as a failure after the work had already succeeded -- the caller
	// was told to retry something that was done. CleanupStaleRequests runs on
	// the game thread and heartbeats live requests, so a handler that truly
	// wedges the editor stops the heartbeat and still trips the idle budget.
	double LastProgressTime = 0.0;
	// Hard ceiling so an async handler that never calls back cannot hang on
	// forever just because the editor keeps ticking.
	double MaxLifetimeSeconds = 3600.0;
	// Highest progress reported so far, so heartbeats and late writers can
	// never send a lower value than the client has already seen.
	float LastProgressPercent = 0.0f;
	FString ToolName;
	FString SessionId;  // for touching ActiveSessions during long-running calls
	FCriticalSection WriteMutex;  // protects socket writes from GameThread
	std::atomic<bool> bMarkedForRemoval{false};  // set by failed writes, checked by CleanupStaleRequests
	// Set by HandleCancelledNotification UNDER SSEConnectionsMutex when the
	// client cancels this in-flight request via notifications/cancelled. Read
	// by CompletePendingRequest (under/after SSEConnectionsMutex) to suppress
	// the late response race-safely: because both paths serialize on
	// SSEConnectionsMutex, the flag is the happens-before boundary, closing
	// the window where the completion could check CancelledInternalRequestIds
	// before HandleCancelledNotification populated it.
	std::atomic<bool> bCancelled{false};
	std::atomic<bool> bProgressWritePending{false};
	// Client-supplied _meta.progressToken, echoed verbatim (type-preserving)
	// in notifications/progress so the client can correlate streamed progress.
	TSharedPtr<FJsonValue> ProgressToken;
	bool bHasProgressToken = false;
	// Canonical key of JsonRpcId (the client's tools/call id) used to
	// correlate notifications/cancelled requestId -> this inflight request.
	FString ClientRequestIdKey;
	// Canonical capability this call resolved to, plus its declared output
	// schema. Set only for gateway execute calls; CompletePendingRequest uses
	// them to validate the handler result and build the semantic receipt.
	FString CapabilityId;
	FString CorrelationId;
	// Task 39: external MCP request id (canonicalized num:/str:), client
	// idempotency key, and request start time, threaded onto the completed
	// receipt so an async success/error carries the same join keys as TS.
	FString RequestId;
	FString IdempotencyId;
	// Set only when this request claimed a fresh idempotency slot; the completion
	// funnel settles that slot (Complete on success, Abandon otherwise).
	FString IdempotencySlot;
	double RequestStartSeconds = 0.0;
	TSharedPtr<FJsonObject> OutputSchema;
};

/** Persistent SSE notification stream (GET /mcp). */
struct FNotificationStream
{
	FSocket* Socket = nullptr;
	FString SessionId;
	FString StreamId;
	double StartTime = 0.0;
	// Non-atomic: written once in HandleGetMcp before the stream is published into
	// NotificationStreams (that publish establishes the happens-before), then
	// read/written only by the keepalive thread (RunKeepaliveLoop).
	double LastKeepaliveTime = 0.0;
	FCriticalSection WriteMutex;
	std::atomic<bool> bReady{false};
	std::atomic<bool> bMarkedForRemoval{false};
};
