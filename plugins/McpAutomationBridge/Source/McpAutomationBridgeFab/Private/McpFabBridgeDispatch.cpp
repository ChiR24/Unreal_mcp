// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpFabBridgeDispatch.h"

#include "McpFabBridgeCallback.h"

#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"
#include "Misc/Guid.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpFabDispatch, Log, All);

namespace McpFabBrowserSession
{
// Defined in McpFabBrowserSessionBridge.cpp: locates the widget and dispatches
// script against it. Script never originates outside this module.
bool RunScriptWithCallback(
	const FString& Script,
	UMcpFabBridgeCallback* Callback,
	FString& OutDiagnostic);
}

namespace McpFabBridgeDispatch
{
namespace
{
/** Rooted for the editor's lifetime; the page holds a reference to it. */
UMcpFabBridgeCallback* GCallback = nullptr;

/** When the outstanding call was dispatched, for the abandonment check below. */
double GDispatchedAt = 0.0;

/**
 * How long an unanswered call keeps the slot.
 *
 * Every operation's script settles as soon as Fab's API answers -- the long
 * waits elsewhere are the asset-registry poll on this side, not the page -- so
 * a reply that has not arrived by now is not late, it is never coming. Without
 * this, a single page error or a tab closed mid-call would make every later
 * Fab request return ALREADY_IN_FLIGHT until the editor restarted.
 *
 * Set below the MCP client's default per-request timeout (120s) so the abandon
 * fires and tells the caller before the client gives up on its own; a window
 * longer than the client timeout would abandon nothing the caller still hears.
 */
constexpr double AbandonAfterSeconds = 90.0;

/**
 * Wraps an operation script in an origin guard that repairs a stalled page.
 *
 * Fab's tab boots from a local file that asks the editor for the real URL and
 * replaces itself with it. That bootstrap is one-shot: it tries once, retries
 * once 500ms later, and both attempts are skipped entirely if window.ue is not
 * bound yet, after which nothing ever tries again and the tab sits on file://
 * for the life of the editor. Every fetch from that origin is cross-origin to
 * fab.com and fails, which looked like a broken bridge rather than a page that
 * never finished loading.
 *
 * So the guard finishes the bootstrap the page abandoned, using the page's own
 * geturl binding, and reports NAVIGATING so the caller retries rather than
 * reading a silent empty result. The reply is sent before the navigation is
 * scheduled, because location.replace tears down the JS context and would take
 * the pending answer with it.
 */
FString WrapWithOriginGuard(const FString& RequestId, const FString& Inner)
{
	return FString::Printf(TEXT(R"JS(
(function () {
  var __id = "%s";
  if (location.origin !== "https://www.fab.com") {
    try { window.ue.mcpfab.onerror(__id, JSON.stringify({ error: "PAGE_NAVIGATING", origin: String(location.origin) })); } catch (e) {}
    setTimeout(function () {
      // geturl is Fab's own binding and is the preferred target because it
      // carries whatever entry point Fab wants. It is also the thing that may
      // never have been bound -- which is how the page stalled in the first
      // place -- so falling back to the origin literal is what makes the
      // repair work in exactly the case that needs repairing.
      var home = "https://www.fab.com/";
      try {
        Promise.resolve(window.ue.fab.geturl())
          .then(function (u) { location.replace(u || home); })
          .catch(function () { location.replace(home); });
      } catch (e) { location.replace(home); }
    }, 50);
    return;
  }
  %s
})();
)JS"), *RequestId, *Inner);
}

UMcpFabBridgeCallback* EnsureCallback()
{
	if (GCallback == nullptr)
	{
		GCallback = NewObject<UMcpFabBridgeCallback>();
		GCallback->AddToRoot();
	}
	return GCallback;
}
} // namespace

bool Dispatch(
	TFunctionRef<FString(const FString& RequestId)> BuildScript,
	TFunction<void(bool, const FString&)> OnComplete,
	FString& OutError,
	FString& OutErrorCode)
{
	UMcpFabBridgeCallback* Callback = EnsureCallback();

	if (!Callback->IsSettled())
	{
		const double Elapsed = FPlatformTime::Seconds() - GDispatchedAt;
		if (Elapsed < AbandonAfterSeconds)
		{
			OutErrorCode = TEXT("ALREADY_IN_FLIGHT");
			OutError = FString::Printf(
				TEXT("Another Fab request is still outstanding; the page exposes one callback, so calls run one at a time. Retry in up to %d second(s)."),
				FMath::CeilToInt(AbandonAfterSeconds - Elapsed));
			return false;
		}
		UE_LOG(LogMcpFabDispatch, Warning,
			TEXT("Abandoning a Fab request unanswered after %.0f seconds; the page never replied."),
			Elapsed);
		Callback->Abandon(TEXT("{\"error\":\"ABANDONED\"}"));
	}

	const FString RequestId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	TSharedRef<FTSTicker::FDelegateHandle> TickerHandle = MakeShared<FTSTicker::FDelegateHandle>();
	GDispatchedAt = FPlatformTime::Seconds();

	// Armed actively, not just checked on the next call. Freeing the slot
	// lazily still leaves the CURRENT caller waiting on a page that has stopped
	// answering, so it waits out its own client timeout with no explanation --
	// which is exactly what a silent script exit produced. The id guard means a
	// late timer cannot settle a request that started after it. The handle is
	// kept so a fast reply can cancel the timer instead of letting it fire a
	// no-op later.
	*TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([RequestId, TickerHandle](float) -> bool
		{
			if (GCallback != nullptr && GCallback->IsAwaiting(RequestId))
			{
				UE_LOG(LogMcpFabDispatch, Warning,
					TEXT("No reply from the Fab page after %.0f seconds; failing the request."),
					AbandonAfterSeconds);
				GCallback->Abandon(TEXT("{\"error\":\"PAGE_TIMED_OUT\"}"));
			}
			FTSTicker::GetCoreTicker().RemoveTicker(*TickerHandle);
			return false;
		}),
		static_cast<float>(AbandonAfterSeconds));

	// A settled request no longer needs the abandonment timer: it either got its
	// answer or was abandoned, and the timer would only fire a no-op later.
	Callback->Expect(
		RequestId,
		[TickerHandle, OnComplete = MoveTemp(OnComplete)](bool bSuccess, const FString& Payload) mutable
		{
			FTSTicker::GetCoreTicker().RemoveTicker(*TickerHandle);
			OnComplete(bSuccess, Payload);
		});

	FString Diagnostic;
	if (!McpFabBrowserSession::RunScriptWithCallback(
			WrapWithOriginGuard(RequestId, BuildScript(RequestId)), Callback, Diagnostic))
	{
		// Disarm rather than wait: no reply can arrive for a script that was
		// never dispatched, and leaving the slot armed would block the next call
		// for the full abandonment window.
		Callback->Expect(FString(), nullptr);
		OutErrorCode = TEXT("FAB_NOT_READY");
		OutError = Diagnostic;
		return false;
	}
	return true;
}
} // namespace McpFabBridgeDispatch
