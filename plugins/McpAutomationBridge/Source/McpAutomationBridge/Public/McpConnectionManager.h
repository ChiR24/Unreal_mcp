#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Templates/SharedPointer.h"
#include "Misc/ScopeLock.h"
#include "Foundation/McpCapabilityPrincipal.h"

class FMcpBridgeWebSocket;
class UMcpAutomationBridgeSettings;

/**
 * Delegate for handling incoming automation requests.
 * Params: RequestId, Action, Payload, RequestingSocket
 */
DECLARE_DELEGATE_FourParams(FMcpMessageReceivedCallback, const FString&, const FString&, const TSharedPtr<FJsonObject>&, TSharedPtr<FMcpBridgeWebSocket>);

/**
 * Delegate for handling inbound cancel_request frames from the TS bridge.
 * Params: RequestId (the automation request id to cancel)
 */
DECLARE_DELEGATE_OneParam(FMcpRequestCancelledCallback, const FString&);

/**
 * Manages WebSocket connections for the MCP Automation Bridge.
 * Handles listening, connecting, reconnecting, heartbeats, and message dispatching.
 */
class MCPAUTOMATIONBRIDGE_API FMcpConnectionManager : public TSharedFromThis<FMcpConnectionManager>
{
public:
	FMcpConnectionManager();
	~FMcpConnectionManager();

	void Initialize(const UMcpAutomationBridgeSettings* Settings);
	void Start();
	void Stop();

	bool IsConnected() const;
	bool IsBridgeActive() const { return bBridgeAvailable; }
	bool IsReconnectPending() const { return TimeUntilReconnect > 0.0f; }

    bool SendRawMessage(const FString& Message);
    bool SendRawMessageToSocket(
        TSharedPtr<FMcpBridgeWebSocket> TargetSocket,
        const FString& Message);
    bool SendRawMessageToLogSubscribers(const FString& Message);
    void SendAutomationResponse(TSharedPtr<FMcpBridgeWebSocket> TargetSocket, const FString& RequestId, bool bSuccess, const FString& Message, const TSharedPtr<FJsonObject>& Result, const FString& ErrorCode);
    void SendControlMessage(const TSharedPtr<FJsonObject>& Message);

    /**
     * Send a progress update message to extend request timeout during long operations.
     * Used for heartbeat/keepalive to prevent timeouts while UE is actively working.
     *
     * @param RequestId The request ID being tracked
     * @param Percent Optional progress percent (0-100)
     * @param Message Optional status message
     * @param bStillWorking True if operation is still in progress (prevents stale detection)
     */
    void SendProgressUpdate(const FString& RequestId, float Percent = -1.0f, const FString& Message = TEXT(""), bool bStillWorking = true);

	void SetOnMessageReceived(FMcpMessageReceivedCallback InCallback);
	void SetOnAutomationRequestCancelled(FMcpRequestCancelledCallback InCallback);

	// Request tracking helpers
	int32 GetActiveSocketCount() const;
	void RegisterRequestSocket(const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket);

	/**
	 * Returns whether the requesting socket is permitted to cancel RequestId.
	 * True when the request is NOT tracked (legacy/untracked path — forwarded
	 * for backward compatibility) or when it IS tracked AND owned by Socket.
	 * False when the request is tracked and owned by a DIFFERENT socket, which
	 * prevents one authenticated client from cancelling another client's
	 * in-flight request. Reads PendingRequestsToSockets under PendingRequestsMutex.
	 */
	bool IsCancelAllowedForSocket(TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& RequestId) const;
	void SetLogSubscription(TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSubscribed);
	bool HasLogSubscribers() const;

	// Telemetry helpers
	void StartRequestTelemetry(const FString& RequestId, const FString& Action);
	void RecordAutomationTelemetry(const FString& RequestId, bool bSuccess, const FString& Message, const FString& ErrorCode);

	bool Tick(float DeltaTime);

private:
	// Allowed access for the focused cancel-scoping automation test.
	friend class FMcpCancelScopeTest;

	void AttemptConnection();
	void ForceReconnect(const FString& Reason, float ReconnectDelayOverride = -1.0f);

	void HandleConnected(TSharedPtr<FMcpBridgeWebSocket> Socket);
	void HandleClientConnected(TSharedPtr<FMcpBridgeWebSocket> ClientSocket);
	void HandleConnectionError(TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& Error);
	void HandleServerConnectionError(const FString& Error);
	void HandleClosed(TSharedPtr<FMcpBridgeWebSocket> Socket, int32 StatusCode, const FString& Reason, bool bWasClean);
	void HandleMessage(TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& Message);
	void HandleCancelRequest(TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& RequestId);
	void HandleHeartbeat(TSharedPtr<FMcpBridgeWebSocket> Socket);

	void EmitAutomationTelemetrySummaryIfNeeded(double NowSeconds);
	bool UpdateRateLimit(FMcpBridgeWebSocket* SocketPtr, bool bIncrementMessage, bool bIncrementAutomation, FString& OutReason);

	// Resolve the socket's capability principal from the presented bridge_hello
	// token and bind it in SocketPrincipals. Returns false when the connection
	// must be refused, which is the ONLY authentication verdict for bridge_hello.
	bool AuthenticateSocketPrincipal(FMcpBridgeWebSocket* SocketPtr, const FString& PresentedToken, bool bLegacyTokenMatch);

	// Build and send the bridge_ack, including the additive, secret-free authority
	// descriptor for the principal just bound to this socket.
	void SendBridgeAck(TSharedPtr<FMcpBridgeWebSocket> Socket, FMcpBridgeWebSocket* SocketPtr);

	// Drop the socket's principal. Takes AuthSocketsMutex itself so every teardown
	// site keeps its AuthenticatedSockets scope to a single guarded statement.
	void ForgetSocketPrincipal(FMcpBridgeWebSocket* SocketPtr);

	// Pre-queue security gate for one automation_request. Returns false when the
	// request was refused and a typed automation_response has already been sent,
	// so the caller must not map or dispatch it.
	bool AuthorizeAutomationRequest(TSharedPtr<FMcpBridgeWebSocket> Socket, const TSharedPtr<FJsonObject>& RootObj);

	// Copy of the socket's bound principal, or an unauthenticated principal when
	// none is bound. Taken by value under AuthSocketsMutex so callers never hold
	// a pointer into the map across a teardown.
	FMcpCapabilityPrincipal GetSocketPrincipal(FMcpBridgeWebSocket* SocketPtr);

private:
	TArray<TSharedPtr<FMcpBridgeWebSocket>> ActiveSockets;
	TMap<FString, TSharedPtr<FMcpBridgeWebSocket>> PendingRequestsToSockets;
	TSet<FMcpBridgeWebSocket*> AuthenticatedSockets;
	// Bound at bridge_hello, cleared on teardown, guarded by AuthSocketsMutex. The
	// plugin is the sole authority: the socket's principal is re-consulted before
	// every request is enqueued; the presented token is never stored here.
	TMap<FMcpBridgeWebSocket*, FMcpCapabilityPrincipal> SocketPrincipals;
	TSet<FMcpBridgeWebSocket*> LogSubscriberSockets;
	FTSTicker::FDelegateHandle TickerHandle;
	FMcpMessageReceivedCallback OnMessageReceived;
	FMcpRequestCancelledCallback OnAutomationRequestCancelled;

	// Configuration
	FString EnvListenHost;
	FString EnvListenPorts;
	FString EndpointUrl;
	FString CapabilityToken;
	FString ServerName;
	FString ServerVersion;
	FString ActiveSessionId;
	FString TlsCertificatePath;
	FString TlsPrivateKeyPath;

	int32 ClientPort = 0;
	float AutoReconnectDelaySeconds = 5.0f;
	float HeartbeatTimeoutSeconds = 0.0f;

	bool bRequireCapabilityToken = false;
	bool bEnableTls = false;
	bool bHeartbeatTrackingEnabled = false;

	// State
	bool bBridgeAvailable = false;
	bool bReconnectEnabled = true;
	float TimeUntilReconnect = 0.0f;
	double LastHeartbeatTimestamp = 0.0;
	int32 MaxMessagesPerMinute = 0;
	int32 MaxAutomationRequestsPerMinute = 0;

	// Telemetry
	struct FAutomationRequestTelemetry
	{
		FString Action;
		double StartTimeSeconds = 0.0;
	};

	struct FAutomationActionStats
	{
		int32 SuccessCount = 0;
		int32 FailureCount = 0;
		double TotalSuccessDurationSeconds = 0.0;
		double TotalFailureDurationSeconds = 0.0;
		double LastDurationSeconds = 0.0;
		double LastUpdatedSeconds = 0.0;
	};

	struct FSocketRateState
	{
		double WindowStartSeconds = 0.0;
		int32 MessageCount = 0;
		int32 AutomationRequestCount = 0;
	};

	TMap<FString, FAutomationRequestTelemetry> ActiveRequestTelemetry;
	TMap<FString, FAutomationActionStats> AutomationActionTelemetry;
	TMap<FMcpBridgeWebSocket*, FSocketRateState> SocketRateLimits;
	double TelemetrySummaryIntervalSeconds = 120.0;
	double LastTelemetrySummaryLogSeconds = 0.0;

  mutable FCriticalSection PendingRequestsMutex;
  mutable FCriticalSection RateLimitMutex;
  mutable FCriticalSection LogSubscribersMutex;

  // Guards every runtime AuthenticatedSockets access. AuthSocketsMutex is taken
  // alone and released before PendingRequestsMutex, RateLimitMutex,
  // LogSubscribersMutex, Socket->Close(), or delegate callbacks.
  mutable FCriticalSection AuthSocketsMutex;
};
