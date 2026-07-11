// McpNativeTransportCancellation.cpp — notifications/cancelled handling

#include "MCP/Transport/McpNativeTransportPrivate.h"

void FMcpNativeTransport::HandleCancelledNotification(
	const TSharedPtr<FJsonObject>& Params)
{
	// Extract the requestId (string or number) the client wants to cancel.
	const TSharedPtr<FJsonValue>* Found =
		Params.IsValid() ? Params->Values.Find(TEXT("requestId")) : nullptr;
	if (!Found || !Found->IsValid())
	{
		return;
	}
	const FString ClientIdKey = McpJsonRpcIdKey(*Found);
	if (ClientIdKey.IsEmpty())
	{
		return;  // unsupported id type
	}

	// Correlate the client's requestId to the inflight SSE connection, mark it
	// cancelled, and stop further progress writes. Lock order: SSEConnectionsMutex
	// then CancelledRequestsMutex (see McpNativeTransport.h lock-order doc).
	FString InternalRequestId;
	bool bFound = false;
	{
		FScopeLock Lock(&SSEConnectionsMutex);
		for (const auto& [Key, Conn] : SSEConnections)
		{
			if (Conn.IsValid() && Conn->ClientRequestIdKey == ClientIdKey)
			{
				InternalRequestId = Key;
				bFound = true;
				// Set the atomic flag under SSEConnectionsMutex (the same lock
				// CompletePendingRequest takes to remove this conn) so the late
				// response is suppressed race-safely — see FSSEConnection::bCancelled.
				Conn->bCancelled.store(true);
				Conn->bMarkedForRemoval.store(true);
				break;
			}
		}
	}
	if (!bFound)
	{
		return;  // not in-flight; nothing to cancel (or already completed)
	}

	{
		FScopeLock Lock(&CancelledRequestsMutex);
		CancelledInternalRequestIds.Add(InternalRequestId);
		CancelledClientIdToInternal.FindOrAdd(ClientIdKey) = InternalRequestId;
	}

	// Cancel the underlying automation request (idempotent if already gone).
	if (Subsystem)
	{
		Subsystem->CancelAutomationRequest(InternalRequestId);
	}
}
