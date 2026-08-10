#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Dom/JsonValue.h"
#include "MCP/DynamicTools/McpDynamicToolManager.h"
#include "MCP/DynamicTools/McpSessionConfigureStore.h"
#include "MCP/Primitives/McpSubscriptionStore.h"
#include "MCP/Primitives/McpNotificationCoalescer.h"
#include "MCP/Primitives/McpTaskMethods.h"
#include "Async/Future.h"
#include <atomic>
#include "MCP/Transport/McpNativeTransportConnectionTypes.h"
#include "Foundation/McpCapabilityPrincipal.h"
#include "Foundation/McpLiveStateRevisions.h"

struct FMcpReceiptContext;

class UMcpAutomationBridgeSubsystem;
class FSocket;
class FRunnableThread;
class FEvent;
class ISocketSubsystem;

/**
 * Native MCP Streamable HTTP transport with SSE streaming.
 * Raw socket HTTP server speaking JSON-RPC 2.0 (MCP protocol 2025-11-25;
 * negotiates 2025-06-18 / 2025-03-26). Validates the MCP-Protocol-Version
 * header on every post-initialize request.
 * SSE streaming for tools/call — progress notifications + final result.
 * Runs alongside existing WebSocket transport — opt-in via bEnableNativeMCP setting.
 */
class FMcpNativeTransport : public FRunnable
{
public:
	explicit FMcpNativeTransport(UMcpAutomationBridgeSubsystem* InSubsystem);
	~FMcpNativeTransport();

	/** Start HTTP server on given host:port. Returns false on failure. */
	bool Start(int32 Port, const FString& PluginDir, bool bLoadAllTools = false,
		const FString& InUserInstructions = TEXT(""),
		const FString& InListenHost = TEXT("127.0.0.1"),
		bool bInAllowNonLoopback = false);

	/** Shut down HTTP server, stop accept thread, close all SSE connections. */
	void Shutdown();

	/** Status accessors for UI. */
	bool IsRunning() const { return Thread != nullptr && !bStopping.load(); }
	int32 GetListenPort() const { return ListenPort; }
	int32 GetActiveSessionCount() const;
	int32 GetEnabledToolCount() const { return ToolManager.GetEnabledToolNames().Num(); }
	int32 GetTotalToolCount() const;

	/**
	 * Complete a pending SSE request with the handler's result.
	 * Writes final JSON-RPC result as SSE event, then closes the connection.
	 * Called from Subsystem::SendAutomationResponse when Socket==nullptr.
	 * Returns true if a pending request was found and completed.
	 */
	bool CompletePendingRequest(const FString& RequestId, bool bSuccess,
		const FString& Message, const TSharedPtr<FJsonObject>& Result,
		const FString& ErrorCode);

	/** Check if a request ID belongs to an active SSE connection. */
	bool HasPendingRequest(const FString& RequestId) const;

	/** Extend timeout for a pending request (called on progress updates). */
	void TouchPendingRequest(const FString& RequestId);

	/** Stream progress notification via SSE to the client. */
	void SendSSEProgressUpdate(const FString& RequestId, float Percent,
		const FString& Message);

	/** Broadcast a JSON-RPC notification to persistent GET /mcp streams. */
	int32 BroadcastNotification(const FString& Method,
		const TSharedPtr<FJsonObject>& Params = nullptr);

	bool SetLogEventSubscriptionForRequest(
		const FString& RequestId, bool bSubscribed);

	bool HasLogEventSubscribers() const;

	int32 BroadcastLogEventNotification(
		const TSharedPtr<FJsonObject>& Params);

	/** Clean up requests that have exceeded the timeout. Called from Tick. */
	void CleanupStaleRequests();

	// ─── Cancellation (notifications/cancelled) ─────────────────────────────
	// Correlates the client's requestId to the inflight SSE connection, marks it
	// cancelled (so the late response is suppressed) and cancels the underlying
	// automation request. Safe to call for unknown/already-completed ids. The
	// CallerSessionId scopes the cancellation so one session cannot cancel
	// another session's in-flight request.
	void HandleCancelledNotification(const TSharedPtr<FJsonObject>& Params, const FString& CallerSessionId);

	// Dedicated-thread keepalive (immune to GameThread stalls).
	void RunKeepaliveLoop();
	void SweepNotificationKeepalives();

	// FRunnable interface
	virtual bool Init() override { return true; }
	virtual uint32 Run() override;
	virtual void Stop() override;

private:
	enum class ESessionValidationResult
	{
		Valid,
		Missing,
		Invalid
	};

	// Accept loop: handle one client connection (runs on ThreadPool)
	void HandleConnection(FSocket* ClientSocket);

	// Low-level socket helpers
	static bool SendAllBytes(FSocket* Socket, const uint8* Data, int32 Length);

	// HTTP parsing and response helpers
	bool ReadHttpRequest(FSocket* Socket, FParsedHttpRequest& OutRequest);
	bool IsCorsEnabled() const;
	bool IsAllowedCorsOrigin(const FString& Origin) const;
	void AppendCorsHeaders(FString& Response, const FString& Origin) const;
	bool SendHttpResponse(FSocket* Socket, int32 StatusCode,
		const FString& ContentType, const FString& Body,
		const TMap<FString, FString>& ExtraHeaders = {},
		const FString& CorsOrigin = FString());
	// Send a response then tear down the client socket (close + destroy). Returns SendHttpResponse result so callers branch on send success.
	bool SendAndClose(FSocket* ClientSocket, int32 StatusCode, const FString& ContentType, const FString& Body, const TMap<FString, FString>& ExtraHeaders = {}, const FString& CorsOrigin = FString());
	// Send a prebuilt JSON-RPC body and tear down the client socket. Shared by
	// the early-return error paths in HandleToolsCall so each stays a one-liner.
	void SendBodyAndClose(FSocket* ClientSocket, const FString& Body,
		int32 Status, const FString& CorsOrigin);
	bool SendSSEHeaders(FSocket* Socket, const FString& SessionId,
		const FString& CorsOrigin = FString());
	static bool WriteSSEEvent(FSSEConnection& Conn, const FString& EventData);

	// JSON-RPC method handlers (return response body string)
	FString HandleInitialize(const TSharedPtr<FJsonObject>& Params,
		const TSharedPtr<FJsonValue>& Id, FString& OutSessionId,
		const FString& ConnectionRemoteAddr);
	FString HandleToolsList(const TSharedPtr<FJsonValue>& Id, const FString& SessionId);
	void HandleToolsCall(const TSharedPtr<FJsonObject>& Params,
		const TSharedPtr<FJsonValue>& Id, FSocket* ClientSocket,
		const FString& SessionId, const FString& CorsOrigin);
	bool TryHandleLocalToolCall(
		const FString& ToolName, const TSharedPtr<FJsonObject>& Arguments,
		const TSharedPtr<FJsonValue>& Id, FSocket* ClientSocket,
		const FString& SessionId, const FString& CorsOrigin);

	// Route a tools/call whose name is the 'unreal' gateway tool.
	void HandleGatewayCall(
		const TSharedPtr<FJsonObject>& Params, const TSharedPtr<FJsonValue>& Id,
		FSocket* ClientSocket, const FString& SessionId, const FString& CorsOrigin,
		const TSharedPtr<FJsonValue>& ProgressToken = nullptr);

	// Gateway mode pre-dispatch: route 'unreal' or reject a direct canonical call.
	// Returns true when it handled (or rejected) the call so the caller returns.
	bool HandleGatewayModePreDispatch(
		const FString& ToolName, const TSharedPtr<FJsonObject>& Arguments,
		const TSharedPtr<FJsonValue>& Id, FSocket* ClientSocket,
		const FString& SessionId, const FString& CorsOrigin,
		const TSharedPtr<FJsonValue>& ProgressToken = nullptr);

	// Canonical execute operation of the 'unreal' gateway tool.
	void HandleGatewayExecute(
		const TSharedPtr<FJsonObject>& Params, const TSharedPtr<FJsonValue>& Id,
		FSocket* ClientSocket, const FString& SessionId, const FString& CorsOrigin,
		const TSharedPtr<FJsonValue>& ProgressToken);

	// Shared SSE streaming + subsystem queue path used by both canonical and
	// gateway execute calls. DispatchAction + Arguments are already resolved.
	// CapabilityId/OutputSchema are supplied only by gateway execute, so the
	// completion path can validate the result and emit a semantic receipt.
	void StreamToolCall(
		const FString& ToolName, const FString& DispatchAction,
		const TSharedPtr<FJsonObject>& Arguments, const TSharedPtr<FJsonValue>& Id,
		FSocket* ClientSocket, const FString& SessionId, const FString& CorsOrigin,
		const TSharedPtr<FJsonValue>& ProgressToken,
		const FString& CapabilityId,
		const TSharedPtr<FJsonObject>& OutputSchema,
		const FMcpReceiptContext& Context);

	// Session validation
	// Task 40 session principal. Bound once at initialize from the presented
	// capability token, then re-verified on every later request so a client can
	// never present one token at initialize and a different one afterwards.
	void BindSessionPrincipal(const FString& SessionId, const FString& PresentedToken);
	FMcpCapabilityPrincipal GetSessionPrincipal(const FString& SessionId);
	bool VerifySessionPrincipal(const FString& SessionId, const FString& PresentedToken);

	ESessionValidationResult ValidateSession(const FString& SessionId, FString& OutError);
	static int32 GetSessionValidationStatusCode(ESessionValidationResult Result);
	void TouchSession(const FString& SessionId);
	void MarkSessionInitializationComplete(const FString& SessionId);
	bool ConsumeSessionRequestBudget(
		const FString& SessionId, bool bToolCall, FString& OutError);
	bool ConsumeClientRequestBudgetLocked(
		const FString& ClientRateKey, bool bToolCall, FString& OutError);
	bool QueueAutomationRequestForSession(
		const FString& SessionId, const FString& RequestId,
		const FString& DispatchAction,
		const TSharedPtr<FJsonObject>& Arguments,
		bool& bOutSessionActive,
		const TMap<EMcpStateKind, int64>& ExpectedRevisions =
			TMap<EMcpStateKind, int64>());
	void CloseSessionConnections(const FString& SessionId);
	// Sessions holding an SSE call or a notification stream. MUST be called
	// WITHOUT SessionMutex held: its two collection-mutex scopes are sequential
	// (Notification, then SSE — same relative order as CloseSessionConnections),
	// and no path in this file takes a collection mutex while holding
	// SessionMutex.
	void CollectSessionsWithLiveConnections(TSet<FString>& OutSessionIds) const;

	// ─── MCP protocol-version negotiation (spec 2025-11-25 lifecycle) ───
	// Negotiate the initialize protocolVersion: echo a supported version, or
	// fall back to the latest supported version for any well-formed (non-empty
	// string) request version. Returns false (with OutError set) when the
	// field is missing, non-string, or empty -> caller emits JSON-RPC -32602.
	bool NegotiateInitializeProtocolVersion(
		const TSharedPtr<FJsonObject>& Params, FString& OutNegotiated,
		FString& OutError);
	// Resolve the MCP-Protocol-Version header for a post-initialize request.
	// Returns false when the header value is present but unsupported/invalid
	// (caller responds HTTP 400). When absent, derives from the negotiated
	// session version or McpDefaultProtocolVersion().
	bool ResolveRequestProtocolVersion(
		const FString& HeaderValue, const FString& SessionId,
		FString& OutVersion, FString& OutError);
	// Validate the protocol-version header for a POST/GET request; on failure
	// send HTTP 400 and close the socket. Returns false when the request was
	// rejected (caller must return). bJsonBody selects the 400 body format.
	bool GuardProtocolVersionHeader(
		FSocket* ClientSocket, const FParsedHttpRequest& Req,
		const TSharedPtr<FJsonValue>& Id, bool bJsonBody);

	void OnToolsListChanged();
	void BroadcastToolsListChanged();

	// Persistent notification stream helpers (GET /mcp)
	void HandleGetMcp(FSocket* ClientSocket, const FString& SessionId,
		const FString& CorsOrigin);
	static bool WriteNotificationEvent(FNotificationStream& Stream, const FString& EventData);
	static bool WriteNotificationKeepalive(FNotificationStream& Stream);
	void CloseNotificationStream(TSharedPtr<FNotificationStream> Stream);
	int32 QueueNotificationEventWrites(
		const TArray<TSharedPtr<FNotificationStream>>& Streams,
		const FString& NotificationJson);

	// ─── MCP primitives (Tasks 31-36 wiring) ────────────────────────────────
	// Dispatches resources/*, prompts/*, and completion/complete by delegating
	// to the pure Tasks 31-36 primitives, inserted before the ErrorMethodNotFound
	// fallback so an implemented primitive never 404s. Returns true when it
	// handled (or errored) the method. Static capability/project reads are served
	// safely; editor-state URIs return a typed RESOURCE_UNAVAILABLE rather than
	// reading editor APIs from the socket thread.
	bool HandlePrimitiveMethod(
		const FString& Method, const TSharedPtr<FJsonObject>& Params,
		const TSharedPtr<FJsonValue>& Id, FSocket* ClientSocket,
		const FString& SessionId, const FString& CorsOrigin);
	// Lazily construct the coalescer + wire the subscription release hook once.
	void InitializePrimitivesIfNeeded();
	// The single per-session primitive cleanup seam: drains a session's
	// subscriptions + coalescer pending. Invoked at every session-teardown moment.
	void ReleaseSessionPrimitives(const FString& SessionId);
	// URI-only resources/updated over the reused async notification writer.
	void SendResourceUpdatedNotification(const FString& SessionId, const FString& Uri);
	// Drain due coalesced notifications from the existing keepalive loop.
	void FlushDuePrimitiveNotifications();

	UMcpAutomationBridgeSubsystem* Subsystem;
	FMcpDynamicToolManager ToolManager;
	// Per-session MCP primitive state (Tasks 34/36). The coalescer holds
	// references to the store + configure store, so those are declared first.
	FMcpSubscriptionStore SubscriptionStore;
	FMcpSessionConfigureStore SessionConfigureStore;
	TUniquePtr<FMcpNotificationCoalescer> NotificationCoalescer;
	// Task 44: owns the bounded per-session task store AND the tasks/* handlers,
	// so routing costs the transport no method of its own.
	FMcpTaskSurface TaskSurface;
	mutable FCriticalSection PrimitiveStateMutex;
	int32 ListenPort = 0;

	// Server identity & instructions (loaded from server-info.json + settings)
	FString ServerName = TEXT("unreal-mcp");
	FString ServerVersion = TEXT("0.5.30");
	FString BaseInstructions;
	FString UserInstructions;

	// Bind configuration
	FString ListenHost = TEXT("127.0.0.1");
	bool bAllowNonLoopback = false;

	// Socket infrastructure
	FSocket* ListenSocket = nullptr;
	FRunnableThread* Thread = nullptr;
	TFuture<void> KeepaliveLoopFuture;  // background keepalive thread (joined in Shutdown)
	FEvent* StopEvent = nullptr;
	FEvent* BindCompleteEvent = nullptr;
	std::atomic<bool> bStopping{false};
	std::atomic<bool> bBindSuccess{false};
	std::atomic<int32> ActiveConnectionCount{0};
	std::atomic<int32> PendingAsyncWrites{0};  // tracks in-flight SSE progress/complete writes
	static constexpr int32 MaxConcurrentConnections = 32;
	static constexpr int32 MaxActiveSessions = 16;
	static constexpr int32 MaxPendingToolCalls = 16;
	static constexpr int32 MaxPendingToolCallsPerSession = 4;

	// Session state (multi-session, with activity tracking)
	TMap<FString, double> ActiveSessions;  // SessionId → LastActivityTime
	TMap<FString, FString> SessionProtocolVersions;  // SessionId → negotiated MCP-Protocol-Version
	// SessionId → bound principal. The presented token is never stored here.
	TMap<FString, FMcpCapabilityPrincipal> SessionPrincipals;
	struct FSessionRateState
	{
		double InitializationCompletedAt = 0.0;
		bool bHasClientActivity = false;
		FString ClientRateKey;
	};
	struct FClientRateState
	{
		double WindowStart = 0.0;
		double LastActivity = 0.0;
		int32 RequestCount = 0;
		int32 ToolCallCount = 0;
	};
	TMap<FString, FSessionRateState> SessionRateStates;
	TMap<FString, FClientRateState> ClientRateStates;
	mutable FCriticalSection SessionMutex;

	static constexpr double SessionTimeoutSeconds = 3600.0;  // 1 hour
	static constexpr double AbandonedSessionGraceSeconds = 5.0;
	// Second-tier reclaim threshold at the session cap. Eviction used to accept
	// ONLY never-used sessions, so once MaxActiveSessions clients had each made
	// a single request, initialize hard-failed for every new client until the
	// 1-hour inactivity timer reclaimed a slot — a client that exits without
	// DELETE (crash, Ctrl-C, container stop) held its slot for the full hour.
	// A session idle this long with no live SSE or notification stream has no
	// work to lose, so it is reclaimable well before the inactivity timeout.
	static constexpr double IdleSessionReclaimSeconds = 120.0;
	static constexpr double SessionRateWindowSeconds = 60.0;
	static constexpr int32 MaxClientRequestsPerMinute = 600;
	static constexpr int32 MaxClientToolCallsPerMinute = 120;

	// Active SSE streaming connections (RequestId → connection)
	TMap<FString, TSharedPtr<FSSEConnection>> SSEConnections;
	mutable FCriticalSection SSEConnectionsMutex;

	// Cancellation (notifications/cancelled): maps the client's JSON-RPC id key
	// to the internal SSE/automation request id, and tracks which internal ids
	// were cancelled so the late completion response is suppressed (not written
	// to the client). Guarded by CancelledRequestsMutex.
	TMap<FString, FString> CancelledClientIdToInternal;  // client id key -> internal request id
	TSet<FString> CancelledInternalRequestIds;            // internal ids to suppress
	// Insertion-order of client-id keys, bounded by a cap constant in
	// McpNativeTransportCancellation.cpp, so the two maps above can be evicted
	// oldest-first and never grow without limit.
	TArray<FString> CancelledMarkerOrder;
	mutable FCriticalSection CancelledRequestsMutex;

	// Persistent notification streams (GET /mcp — StreamId → stream)
	TMap<FString, TSharedPtr<FNotificationStream>> NotificationStreams;
	mutable FCriticalSection NotificationStreamsMutex;
	TSet<FString> LogEventSubscribedSessions;
	mutable FCriticalSection LogEventSubscriptionsMutex;

	// ─── Lock-order convention (audit before adding nested critical sections) ───
	//
	// The transport's global mutexes form a partial order. Nested acquisition is
	// only safe when it follows the documented order; violating it can deadlock
	// against concurrent paths that take the same locks in the opposite order.
	//
	//   1. LogEventSubscriptionsMutex
	//   2. SSEConnectionsMutex
	//   3. NotificationStreamsMutex
	//   4. SessionMutex
	//
	// Rules:
	//   * Never nest SessionMutex INSIDE SSEConnectionsMutex, NotificationStreamsMutex,
	//     or LogEventSubscriptionsMutex. Take SessionMutex first, then release it
	//     before acquiring the other lock (see HandleToolsCall / HandleGetMcp).
	//   * Per-connection WriteMutex (FSSEConnection::WriteMutex and
	//     FNotificationStream::WriteMutex) is taken ONLY after the corresponding
	//     collection mutex is held; it is never acquired before or instead of the
	//     collection mutex. See WriteSSEEvent, WriteNotificationEvent,
	//     WriteNotificationKeepalive.
	//   * CancelledRequestsMutex is taken ONLY after SSEConnectionsMutex and only
	//     ever before a per-connection WriteMutex. It guards the cancellation
	//     sets and is never held while acquiring SSEConnectionsMutex,
	//     NotificationStreamsMutex, or SessionMutex — so it cannot participate in
	//     a lock-order cycle. See HandleCancelledNotification / CompletePendingRequest.
	//   * CloseSessionConnections and CollectSessionsWithLiveConnections are the
	//     only functions that take multiple collection mutexes, always in
	//     sequential non-nested scopes in the same relative order (Notification
	//     → SSE); never combine them with SessionMutex. They are safe because
	//     they run after the session has already been removed from ActiveSessions
	//     by the caller, or (for the collector) before any session lock is taken.

	static constexpr int32 MaxNotificationStreamsPerSession = 4;
	static constexpr int32 MaxTotalNotificationStreams = 16;
	static constexpr int32 MaxPendingNotificationWrites =
		MaxTotalNotificationStreams * 4;
	static constexpr double NotificationStreamTimeoutSeconds = 3600.0;  // 1 hour
	static constexpr double KeepaliveIntervalSeconds = 30.0;
};
