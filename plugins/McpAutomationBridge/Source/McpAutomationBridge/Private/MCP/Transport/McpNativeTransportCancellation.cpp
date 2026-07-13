// McpNativeTransportCancellation.cpp — notifications/cancelled handling

#include "MCP/Transport/McpNativeTransportPrivate.h"

namespace
{
// Cap on cancellation marker bookkeeping (client id -> internal id + internal
// id set). Bounds the maps so a client that floods notifications/cancelled
// cannot grow them without limit.
constexpr int32 MaxCancelledMarkers = 64;
}

void FMcpNativeTransport::HandleCancelledNotification(
	const TSharedPtr<FJsonObject>& Params, const FString& CallerSessionId)
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
	// The match requires BOTH the client id key AND the caller's session id, so
	// one session cannot cancel another session's in-flight request.
	FString InternalRequestId;
	bool bFound = false;
	{
		FScopeLock Lock(&SSEConnectionsMutex);
		for (const auto& [Key, Conn] : SSEConnections)
		{
			if (Conn.IsValid()
				&& Conn->ClientRequestIdKey == ClientIdKey
				&& Conn->SessionId == CallerSessionId)
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
		CancelledMarkerOrder.Add(ClientIdKey);
		// Bound the marker maps: drop the oldest (insertion order) entries while
		// over the cap so they cannot grow without limit. Only CancelledRequestsMutex
		// is held here, so the documented lock order is preserved.
		while (CancelledMarkerOrder.Num() > MaxCancelledMarkers)
		{
			const FString Oldest = CancelledMarkerOrder[0];
			CancelledMarkerOrder.RemoveAt(0);
			if (const FString* InternalId = CancelledClientIdToInternal.Find(Oldest))
			{
				CancelledInternalRequestIds.Remove(*InternalId);
				CancelledClientIdToInternal.Remove(Oldest);
			}
		}
	}

	// Cancel the underlying automation request (idempotent if already gone).
	if (Subsystem)
	{
		Subsystem->CancelAutomationRequest(InternalRequestId);
	}
}
