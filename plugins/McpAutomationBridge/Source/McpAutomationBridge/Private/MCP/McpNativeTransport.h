#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Dom/JsonValue.h"
#include "MCP/McpDynamicToolManager.h"
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

	/** Clean up requests that have exceeded the timeout. Called from Tick. */
	void CleanupStaleRequests();

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
		FString ToolName;
		FString SessionId;  // for touching ActiveSessions during long-running calls
		FCriticalSection WriteMutex;  // protects socket writes from GameThread
		std::atomic<bool> bMarkedForRemoval{false};  // set by failed writes, checked by CleanupStaleRequests
	};

	/** Persistent SSE notification stream (GET /mcp). */
	struct FNotificationStream
	{
		FSocket* Socket = nullptr;
		FString SessionId;
		FString StreamId;
		double StartTime = 0.0;
		double LastKeepaliveTime = 0.0;
		FCriticalSection WriteMutex;
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
		const TSharedPtr<FJsonValue>& Id, FString& OutSessionId);
	FString HandleToolsList(const TSharedPtr<FJsonValue>& Id);
	void HandleToolsCall(const TSharedPtr<FJsonObject>& Params,
		const TSharedPtr<FJsonValue>& Id, FSocket* ClientSocket,
		const FString& SessionId, const FString& CorsOrigin);

	// Session validation
	ESessionValidationResult ValidateSession(const FString& SessionId, FString& OutError);
	static int32 GetSessionValidationStatusCode(ESessionValidationResult Result);
	void TouchSession(const FString& SessionId);

	void OnToolsListChanged();
	void BroadcastToolsListChanged();

	// Persistent notification stream helpers (GET /mcp)
	void HandleGetMcp(FSocket* ClientSocket, const FString& SessionId,
		const FString& CorsOrigin);
	static bool WriteNotificationEvent(FNotificationStream& Stream, const FString& EventData);
	static bool WriteNotificationKeepalive(FNotificationStream& Stream);
	void CloseNotificationStream(TSharedPtr<FNotificationStream> Stream);

	UMcpAutomationBridgeSubsystem* Subsystem;
	FMcpDynamicToolManager ToolManager;
	int32 ListenPort = 0;

	// Server identity & instructions (loaded from server-info.json + settings)
	FString ServerName = TEXT("unreal-mcp");
	FString ServerVersion = TEXT("0.6.0");
	FString BaseInstructions;
	FString UserInstructions;

	// Bind configuration
	FString ListenHost = TEXT("127.0.0.1");
	bool bAllowNonLoopback = false;

	// Socket infrastructure
	FSocket* ListenSocket = nullptr;
	FRunnableThread* Thread = nullptr;
	FEvent* StopEvent = nullptr;
	FEvent* BindCompleteEvent = nullptr;
	std::atomic<bool> bStopping{false};
	std::atomic<bool> bBindSuccess{false};
	std::atomic<int32> ActiveConnectionCount{0};
	std::atomic<int32> PendingAsyncWrites{0};  // tracks in-flight SSE progress/complete writes
	static constexpr int32 MaxConcurrentConnections = 16;

	// Session state (multi-session, with activity tracking)
	TMap<FString, double> ActiveSessions;  // SessionId → LastActivityTime
	mutable FCriticalSection SessionMutex;

	static constexpr double SessionTimeoutSeconds = 3600.0;  // 1 hour

	// Active SSE streaming connections (RequestId → connection)
	TMap<FString, TSharedPtr<FSSEConnection>> SSEConnections;
	mutable FCriticalSection SSEConnectionsMutex;

	static constexpr double RequestTimeoutSeconds = 300.0;  // 5 minutes

	// Persistent notification streams (GET /mcp — StreamId → stream)
	TMap<FString, TSharedPtr<FNotificationStream>> NotificationStreams;
	mutable FCriticalSection NotificationStreamsMutex;

	static constexpr int32 MaxNotificationStreamsPerSession = 4;
	static constexpr double NotificationStreamTimeoutSeconds = 3600.0;  // 1 hour
	static constexpr double KeepaliveIntervalSeconds = 30.0;
};
