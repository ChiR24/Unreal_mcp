#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Dom/JsonValue.h"
#include "MCP/DynamicTools/McpDynamicToolManager.h"
#include "Async/Future.h"
#include <atomic>

class UMcpAutomationBridgeSubsystem;
class FSocket;
class FRunnableThread;
class FEvent;
class ISocketSubsystem;

/**
 * Native MCP Streamable HTTP transport with SSE streaming.
 * Raw socket HTTP server speaking JSON-RPC 2.0 (MCP protocol 2025-03-26).
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

	// Dedicated-thread keepalive (immune to GameThread stalls).
	void RunKeepaliveLoop();
	void SweepNotificationKeepalives();

	// FRunnable interface
	virtual bool Init() override { return true; }
	virtual uint32 Run() override;
	virtual void Stop() override;

private:
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
		int32 ContentLength = 0;
	};

	/** Active SSE streaming connection for a tools/call request. */
	struct FSSEConnection
	{
		FSocket* Socket = nullptr;
		TSharedPtr<FJsonValue> JsonRpcId;
		double StartTime = 0.0;
		double TimeoutSeconds = 300.0;
		FString ToolName;
		FString SessionId;  // for touching ActiveSessions during long-running calls
		FCriticalSection WriteMutex;  // protects socket writes from GameThread
		std::atomic<bool> bMarkedForRemoval{false};  // set by failed writes, checked by CleanupStaleRequests
		std::atomic<bool> bProgressWritePending{false};
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
	bool SendSSEHeaders(FSocket* Socket, const FString& SessionId,
		const FString& CorsOrigin = FString());
	static bool WriteSSEEvent(FSSEConnection& Conn, const FString& EventData);

	// JSON-RPC method handlers (return response body string)
	FString HandleInitialize(const TSharedPtr<FJsonObject>& Params,
		const TSharedPtr<FJsonValue>& Id, FString& OutSessionId,
		const FString& ConnectionRemoteAddr);
	FString HandleToolsList(const TSharedPtr<FJsonValue>& Id);
	void HandleToolsCall(const TSharedPtr<FJsonObject>& Params,
		const TSharedPtr<FJsonValue>& Id, FSocket* ClientSocket,
		const FString& SessionId, const FString& CorsOrigin);
	bool TryHandleLocalToolCall(
		const FString& ToolName, const TSharedPtr<FJsonObject>& Arguments,
		const TSharedPtr<FJsonValue>& Id, FSocket* ClientSocket,
		const FString& SessionId, const FString& CorsOrigin);

	// Session validation
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
		bool& bOutSessionActive);
	void CloseSessionConnections(const FString& SessionId);

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

	UMcpAutomationBridgeSubsystem* Subsystem;
	FMcpDynamicToolManager ToolManager;
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
	static constexpr double SessionRateWindowSeconds = 60.0;
	static constexpr int32 MaxClientRequestsPerMinute = 600;
	static constexpr int32 MaxClientToolCallsPerMinute = 120;

	// Active SSE streaming connections (RequestId → connection)
	TMap<FString, TSharedPtr<FSSEConnection>> SSEConnections;
	mutable FCriticalSection SSEConnectionsMutex;

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
	//   * CloseSessionConnections is the only function that takes multiple
	//     collection mutexes (Log → Notification → SSE). It does not take
	//     SessionMutex and is safe because it operates after the session has
	//     already been removed from ActiveSessions by the caller.

	static constexpr int32 MaxNotificationStreamsPerSession = 4;
	static constexpr int32 MaxTotalNotificationStreams = 16;
	static constexpr int32 MaxPendingNotificationWrites =
		MaxTotalNotificationStreams * 4;
	static constexpr double NotificationStreamTimeoutSeconds = 3600.0;  // 1 hour
	static constexpr double KeepaliveIntervalSeconds = 30.0;
};
