// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpFabBrowserBridge.h"

#include "McpFabBridgeCallback.h"
#include "McpFabBridgeDispatch.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Guid.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpFabState, Log, All);

const TCHAR* McpFabBridgeStateToString(EMcpFabBridgeState State)
{
	switch (State)
	{
	case EMcpFabBridgeState::NotFound:         return TEXT("NOT_FOUND");
	case EMcpFabBridgeState::TabFound:         return TEXT("TAB_FOUND");
	case EMcpFabBridgeState::Navigating:       return TEXT("NAVIGATING");
	case EMcpFabBridgeState::FabOriginReady:   return TEXT("FAB_ORIGIN_READY");
	case EMcpFabBridgeState::FabBindingsReady: return TEXT("FAB_BINDINGS_READY");
	case EMcpFabBridgeState::Ready:            return TEXT("READY");
	default:                                   return TEXT("UNKNOWN");
	}
}

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
		PendingId.Empty();
		TFunction<void(bool, const FString&)> Local = MoveTemp(Completion);
		Completion = nullptr;
		if (Local) { Local(false, TEXT("{\"error\":\"PAYLOAD_TOO_LARGE\"}")); }
		return;
	}
	PendingId.Empty();
	TFunction<void(bool, const FString&)> Local = MoveTemp(Completion);
	Completion = nullptr;
	if (Local) { Local(bSuccess, Payload); }
}

void UMcpFabBridgeCallback::Abandon(const FString& Reason)
{
	if (PendingId.IsEmpty())
	{
		return;
	}
	PendingId.Empty();
	TFunction<void(bool, const FString&)> Local = MoveTemp(Completion);
	Completion = nullptr;
	if (Local) { Local(false, Reason); }
}

void UMcpFabBridgeCallback::OnResult(const FString& RequestId, const FString& Payload)
{
	Settle(RequestId, true, Payload);
}

void UMcpFabBridgeCallback::OnError(const FString& RequestId, const FString& Message)
{
	Settle(RequestId, false, Message);
}

// ---------------------------------------------------------------------------
// State.
// ---------------------------------------------------------------------------

namespace
{
/**
 * Readiness is asked of the page, not assumed from the widget existing.
 *
 * Reports only booleans and the origin. It never returns a token, a cookie or a
 * URL, so the reply is safe to log in full.
 */
FString BuildReadinessScript(const FString& RequestId)
{
	return FString::Printf(TEXT(R"JS(
(function () {
  var id = "%s";
  try {
    var ue = window.ue;
    var fab = ue ? ue.fab : null;
    window.ue.mcpfab.onresult(id, JSON.stringify({
      origin: String(location.origin),
      hasUe: !!ue,
      hasFab: !!fab,
      canAdd: !!(fab && typeof fab.addtoproject === "function")
    }));
  } catch (e) {
    try { window.ue.mcpfab.onerror(id, String(e).slice(0, 500)); } catch (_) {}
  }
})();
)JS"), *RequestId);
}

EMcpFabBridgeState ClassifyReadiness(const FString& Json, FString& OutDetail)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutDetail = TEXT("page reply was not JSON");
		return EMcpFabBridgeState::TabFound;
	}
	FString Origin;
	Root->TryGetStringField(TEXT("origin"), Origin);
	OutDetail = FString::Printf(TEXT("origin=%s"), *Origin);

	if (Origin != TEXT("https://www.fab.com"))
	{
		return EMcpFabBridgeState::Navigating;
	}
	bool bHasFab = false;
	Root->TryGetBoolField(TEXT("hasFab"), bHasFab);
	if (!bHasFab)
	{
		return EMcpFabBridgeState::FabOriginReady;
	}
	bool bCanAdd = false;
	Root->TryGetBoolField(TEXT("canAdd"), bCanAdd);
	return bCanAdd ? EMcpFabBridgeState::Ready : EMcpFabBridgeState::FabBindingsReady;
}
} // namespace

void McpFabBrowserBridge::QueryState(
	TFunction<void(EMcpFabBridgeState, const FString&)> Completion)
{
	FString Error;
	FString ErrorCode;
	const bool bDispatched = McpFabBridgeDispatch::Dispatch(
		[](const FString& RequestId) { return BuildReadinessScript(RequestId); },
		[Completion](bool bSuccess, const FString& Payload)
		{
			FString Detail;
			EMcpFabBridgeState State = EMcpFabBridgeState::TabFound;
			if (bSuccess)
			{
				State = ClassifyReadiness(Payload, Detail);
			}
			else
			{
				TSharedPtr<FJsonObject> ErrorJson;
				const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Payload);
				if (FJsonSerializer::Deserialize(Reader, ErrorJson) && ErrorJson.IsValid())
				{
					FString ErrorCode;
					ErrorJson->TryGetStringField(TEXT("error"), ErrorCode);
					if (ErrorCode == TEXT("PAGE_NAVIGATING"))
					{
						State = EMcpFabBridgeState::Navigating;
						ErrorJson->TryGetStringField(TEXT("origin"), Detail);
					}
				}
				if (State == EMcpFabBridgeState::TabFound)
				{
					Detail = TEXT("page script failed");
				}
			}
			Completion(State, Detail);
		},
		Error, ErrorCode);

	if (!bDispatched)
	{
		// A busy slot means the page is there and answering someone else, which
		// is a tab-found state; only a missing widget is NOT_FOUND.
		Completion(
			ErrorCode == TEXT("ALREADY_IN_FLIGHT")
				? EMcpFabBridgeState::TabFound
				: EMcpFabBridgeState::NotFound,
			Error);
	}
}

/** Reports readiness as a state, so a caller knows whether to wait or to ask. */
static FAutoConsoleCommand GMcpFabState(
	TEXT("Mcp.Fab.State"),
	TEXT("Reports whether Fab's page is ready to accept operations."),
	FConsoleCommandDelegate::CreateStatic([]()
	{
		McpFabBrowserBridge::QueryState([](EMcpFabBridgeState State, const FString& Detail)
		{
			UE_LOG(LogMcpFabState, Log, TEXT("Mcp.Fab.State: state=%s %s"),
				McpFabBridgeStateToString(State), *Detail);
		});
	}));
