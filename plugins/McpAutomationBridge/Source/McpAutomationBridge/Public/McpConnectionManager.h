#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Templates/SharedPointer.h"
#include "Misc/ScopeLock.h"

class FMcpBridgeWebSocket;
class UMcpAutomationBridgeSettings;

/**
 * Delegate for handling incoming automation requests.
 * Params: RequestId, Action, Payload, RequestingSocket
 */
DECLARE_DELEGATE_FourParams(FMcpMessageReceivedCallback, const FString&, const FString&, const TSharedPtr<FJsonObject>&, TSharedPtr<FMcpBridgeWebSocket>);

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

	// Request tracking helpers
	int32 GetActiveSocketCount() const;
	void RegisterRequestSocket(const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket);

	// Telemetry helpers
	void StartRequestTelemetry(const FString& RequestId, const FString& Action);
	void RecordAutomationTelemetry(const FString& RequestId, bool bSuccess, const FString& Message, const FString& ErrorCode);

	bool Tick(float DeltaTime);

private:
	void AttemptConnection();
	void ForceReconnect(const FString& Reason, float ReconnectDelayOverride = -1.0f);

	void HandleConnected(TSharedPtr<FMcpBridgeWebSocket> Socket);
	void HandleClientConnected(TSharedPtr<FMcpBridgeWebSocket> ClientSocket);
	void HandleConnectionError(TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& Error);
	void HandleServerConnectionError(const FString& Error);
	void HandleClosed(TSharedPtr<FMcpBridgeWebSocket> Socket, int32 StatusCode, const FString& Reason, bool bWasClean);
	void HandleMessage(TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& Message);
	void HandleHeartbeat(TSharedPtr<FMcpBridgeWebSocket> Socket);

	void EmitAutomationTelemetrySummaryIfNeeded(double NowSeconds);
	bool UpdateRateLimit(FMcpBridgeWebSocket* SocketPtr, bool bIncrementMessage, bool bIncrementAutomation, FString& OutReason);

private:
	TArray<TSharedPtr<FMcpBridgeWebSocket>> ActiveSockets;
	TMap<FString, TSharedPtr<FMcpBridgeWebSocket>> PendingRequestsToSockets;
	TSet<FMcpBridgeWebSocket*> AuthenticatedSockets;
	FTSTicker::FDelegateHandle TickerHandle;
	FMcpMessageReceivedCallback OnMessageReceived;

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
	bool bEnvListenPortsSet = false;
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
};
