// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpFabBridgeCallback.h"
#include "McpFabBridgeDispatch.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Misc/Guid.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpFabState, Log, All);

// ---------------------------------------------------------------------------
// Callback surface. Page script is untrusted input: it is Fab's code rather
// than ours, and it reaches these functions directly.
// ---------------------------------------------------------------------------

void UMcpFabBridgeCallback::Expect(
	const FString& RequestId, TFunction<void(bool, const FString&)> InCompletion)
{
	PendingId = RequestId;
	Completion = MoveTemp(InCompletion);
}

void UMcpFabBridgeCallback::Settle(
	const FString& RequestId, bool bSuccess, const FString& Payload)
{
	// One-shot and correlated. Without the id check, a page that replied twice --
	// or a stale reply from a previous operation -- would settle whichever
	// request happened to be armed, which matters as soon as two Fab operations
	// can be in flight.
	if (PendingId.IsEmpty() || RequestId != PendingId)
	{
		UE_LOG(LogMcpFabState, Warning, TEXT("Discarding page reply for unexpected request id."));
		return;
	}
	if (Payload.Len() > MaxPayloadChars)
	{
		UE_LOG(LogMcpFabState, Warning,
			TEXT("Discarding oversized page reply (%d chars)."), Payload.Len());
		Finish(false, TEXT("{\"error\":\"PAYLOAD_TOO_LARGE\"}"));
		return;
	}
	Finish(bSuccess, Payload);
}

void UMcpFabBridgeCallback::Finish(bool bSuccess, const FString& Payload)
{
	PendingId.Empty();
	TFunction<void(bool, const FString&)> Local = MoveTemp(Completion);
	Completion = nullptr;
	if (Local) { Local(bSuccess, Payload); }
}

void UMcpFabBridgeCallback::Abandon(const FString& Reason)
{
	if (!PendingId.IsEmpty())
	{
		Finish(false, Reason);
	}
}

void UMcpFabBridgeCallback::OnResult(const FString& RequestId, const FString& Payload)
{
	Settle(RequestId, true, Payload);
}

void UMcpFabBridgeCallback::OnError(const FString& RequestId, const FString& Message)
{
	Settle(RequestId, false, Message);
}
