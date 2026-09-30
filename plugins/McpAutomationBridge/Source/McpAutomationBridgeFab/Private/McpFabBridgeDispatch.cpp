// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpFabBridgeDispatch.h"

#include "McpFabBridgeCallback.h"
#include "McpFabPageReadiness.h"

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

// How long an unanswered call keeps the slot.
//
// Every operation's script settles as soon as Fab's API answers -- the long
// waits elsewhere are the asset-registry poll on this side, not the page -- so
// a reply that has not arrived by now is not late, it is never coming. Without
// this, a single page error or a tab closed mid-call would make every later
// Fab request wait on a slot that never frees, until the editor restarted.
//
// Set below the MCP client's default per-request timeout (120s) so the abandon
// fires and tells the caller before the client gives up on its own; a window
// longer than the client timeout would abandon nothing the caller still hears.
constexpr double AbandonAfterSeconds = 90.0;

// A request whose page is not ready, or whose turn on the one callback has not come, waits on a ticker
// rather than fail: the first call after the tab auto-opens used to answer PAGE_NAVIGATING, and a search
// during an add answered ALREADY_IN_FLIGHT. The game thread is never blocked by the wait.
constexpr float WaiterTickSeconds = 0.5f;
// How long a request waits for a page that is not on fab.com yet.
constexpr double PageBudgetSeconds = 15.0;
// How long it waits for its turn on the one page callback.
constexpr double SlotBudgetSeconds = 60.0;
// A page that is still not on fab.com after this long is nudged: its own bootstrap only tries once.
constexpr double KickAfterSeconds = 2.0;
// A page on fab.com that still reports loading after this long is treated as loaded.
constexpr double LoadingGraceSeconds = 6.0;

struct FWaiter
{
	FString RequestId;
	FString Script;
	TFunction<void(bool, const FString&)> OnComplete;
	double PageWaited = 0.0;
	double SlotWaited = 0.0;
	double LoadingFor = 0.0;
	bool bKicked = false;
};

/** In arrival order; only the front is served, so requests keep their order. */
TArray<FWaiter> GWaiters;
bool bWaiterTickerActive = false;

// Wraps an operation script in an origin guard that repairs a stalled page.
//
// Fab's tab boots from a local file that asks the editor for the real URL and
// replaces itself with it. That bootstrap is one-shot: it tries once, retries
// once 500ms later, and both attempts are skipped entirely if window.ue is not
// bound yet, after which nothing ever tries again and the tab sits on file://
// for the life of the editor. Every fetch from that origin is cross-origin to
// fab.com and fails, which looked like a broken bridge rather than a page that
// never finished loading.
//
// So the guard finishes the bootstrap the page abandoned, using the page's own
// geturl binding, and reports NAVIGATING so the caller retries rather than
// reading a silent empty result. The reply is sent before the navigation is
// scheduled, because location.replace tears down the JS context and would take
// the pending answer with it. The dispatcher only runs a script once the page is
// on fab.com, so this is the safety net for a page that navigates away meanwhile.
FString WrapWithOriginGuard(const FString& RequestId, const FString& Inner)
{
	return FString::Printf(TEXT(R"JS(
(function () {
  var __id = "%s";
  // Fab's own editor entry point is the APEX domain (FabBrowserApi::GetUrl
  // answers https://fab.com/plugins/ue5), while this guard originally
  // accepted only the www host. Apex and www are the same site, so the
  // guard treated a perfectly healthy page as broken: it reported
  // PAGE_NAVIGATING, replaced the location with a URL that landed on apex
  // again, and every following call timed out after the full abandonment
  // window because the reply bindings were torn down by that navigation.
  // Net effect: the entire Fab bridge was unusable while looking signed in.
  var __origin = String(location.origin);
  if (__origin !== "https://www.fab.com" && __origin !== "https://fab.com") {
    try { window.ue.mcpfab.onerror(__id, JSON.stringify({ error: "PAGE_NAVIGATING", origin: __origin })); } catch (e) {}
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

/** True while an earlier request still holds the one page callback and has not yet been given up on. */
bool SlotBusy()
{
	return GCallback != nullptr && !GCallback->IsSettled() &&
		(FPlatformTime::Seconds() - GDispatchedAt) < AbandonAfterSeconds;
}

/** Arms the reply, starts the abandonment timer and runs the script. The slot must be free or stale. */
void Run(FWaiter Waiter)
{
	UMcpFabBridgeCallback* Callback = EnsureCallback();
	if (!Callback->IsSettled())
	{
		UE_LOG(LogMcpFabDispatch, Warning,
			TEXT("Abandoning a Fab request unanswered after %.0f seconds; the page never replied."),
			FPlatformTime::Seconds() - GDispatchedAt);
		Callback->Abandon(FailurePayload(TEXT("ABANDONED"), TEXT("An earlier Fab request never got an answer from the page.")));
	}
	GDispatchedAt = FPlatformTime::Seconds();
	TSharedRef<FTSTicker::FDelegateHandle> TickerHandle = MakeShared<FTSTicker::FDelegateHandle>();

	// Armed actively, not just checked on the next call. Freeing the slot
	// lazily still leaves the CURRENT caller waiting on a page that has stopped
	// answering, so it waits out its own client timeout with no explanation --
	// which is exactly what a silent script exit produced. The id guard means a
	// late timer cannot settle a request that started after it. The handle is
	// kept so a fast reply can cancel the timer instead of letting it fire a
	// no-op later.
	*TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([RequestId = Waiter.RequestId, TickerHandle](float) -> bool
		{
			if (GCallback != nullptr && GCallback->IsAwaiting(RequestId))
			{
				UE_LOG(LogMcpFabDispatch, Warning,
					TEXT("No reply from the Fab page after %.0f seconds; failing the request."),
					AbandonAfterSeconds);
				GCallback->Abandon(FailurePayload(TEXT("PAGE_TIMED_OUT"),
					TEXT("The Fab page did not answer within 90 seconds; it may have closed or hit an error. Retry.")));
			}
			FTSTicker::GetCoreTicker().RemoveTicker(*TickerHandle);
			return false;
		}),
		static_cast<float>(AbandonAfterSeconds));

	// A settled request no longer needs the abandonment timer: it either got its
	// answer or was abandoned, and the timer would only fire a no-op later.
	Callback->Expect(
		Waiter.RequestId,
		[TickerHandle, OnComplete = MoveTemp(Waiter.OnComplete)](bool bSuccess, const FString& Payload) mutable
		{
			FTSTicker::GetCoreTicker().RemoveTicker(*TickerHandle);
			OnComplete(bSuccess, Payload);
		});

	FString Diagnostic;
	if (!McpFabBrowserSession::RunScriptWithCallback(Waiter.Script, Callback, Diagnostic))
	{
		// No reply can arrive for a script that was never dispatched: settle it now rather than
		// hold the slot for the whole abandonment window.
		Callback->Abandon(FailurePayload(TEXT("FAB_NOT_READY"), Diagnostic));
	}
}

/** Fails a waiter that will never get its turn, telling its caller why. */
void FailWaiter(int32 Index, const FString& Payload)
{
	TFunction<void(bool, const FString&)> Done = MoveTemp(GWaiters[Index].OnComplete);
	GWaiters.RemoveAt(Index);
	Done(false, Payload);
}

bool TickWaiters(float Delta)
{
	const bool bSlotFree = !SlotBusy();
	for (int32 Index = GWaiters.Num() - 1; Index >= 0; --Index)
	{
		GWaiters[Index].SlotWaited += Delta;
		if (GWaiters[Index].SlotWaited >= SlotBudgetSeconds && !(Index == 0 && bSlotFree))
		{
			FailWaiter(Index, FailurePayload(TEXT("PAGE_BUSY"),
				FString::Printf(TEXT("Another Fab request held the page for %.0f seconds and this one never got its turn. Retry."), SlotBudgetSeconds),
				SlotBudgetSeconds));
		}
	}
	if (GWaiters.Num() > 0 && bSlotFree)
	{
		FWaiter& Front = GWaiters[0];
		const McpFabPageReadiness::FPageState Page = McpFabPageReadiness::Probe();
		if (!Page.bTabFound)
		{
			FailWaiter(0, FailurePayload(TEXT("FAB_NOT_READY"), Page.Diagnostic));
		}
		else if (Page.bUrlIsFab && (!Page.bLoading || Front.LoadingFor >= LoadingGraceSeconds))
		{
			FWaiter Ready = MoveTemp(Front);
			GWaiters.RemoveAt(0);
			Run(MoveTemp(Ready));
		}
		else
		{
			Front.PageWaited += Delta;
			if (Page.bUrlIsFab)
			{
				Front.LoadingFor += Delta;
			}
			else if (!Front.bKicked && Front.PageWaited >= KickAfterSeconds)
			{
				// Only the origin guard runs: it navigates a page that is off fab.com and does nothing
				// to one that has just arrived, so this can never act for a request nobody is awaiting.
				Front.bKicked = true;
				FString Ignored;
				McpFabBrowserSession::RunScriptWithCallback(
					WrapWithOriginGuard(TEXT("repair"), FString()), EnsureCallback(), Ignored);
			}
			if (Front.PageWaited >= PageBudgetSeconds)
			{
				FailWaiter(0, FailurePayload(TEXT("PAGE_NOT_READY"), DescribePageNotReady(PageBudgetSeconds), PageBudgetSeconds));
			}
		}
	}
	bWaiterTickerActive = GWaiters.Num() > 0;
	return bWaiterTickerActive;
}
} // namespace

FString FailurePayload(const TCHAR* Code, const FString& Message, double WaitedSeconds)
{
	FString Safe = Message.Replace(TEXT("\\"), TEXT("/")).Replace(TEXT("\""), TEXT("'"));
	Safe = Safe.Replace(TEXT("\r"), TEXT(" ")).Replace(TEXT("\n"), TEXT(" "));
	FString Json = FString::Printf(TEXT("{\"error\":\"%s\",\"message\":\"%s\""), Code, *Safe);
	if (WaitedSeconds >= 0.0)
	{
		Json += FString::Printf(TEXT(",\"waitedSeconds\":%.0f"), WaitedSeconds);
	}
	return Json + TEXT("}");
}

FString DescribePageNotReady(double WaitedSeconds)
{
	return FString::Printf(
		TEXT("The Fab tab was not on fab.com after %.0f seconds. It was opened and pointed there; if it shows a sign-in page, sign in and retry."),
		WaitedSeconds);
}

bool Dispatch(
	TFunctionRef<FString(const FString& RequestId)> BuildScript,
	TFunction<void(bool, const FString&)> OnComplete,
	FString& OutError,
	FString& OutErrorCode)
{
	EnsureCallback();
	const McpFabPageReadiness::FPageState Page = McpFabPageReadiness::Probe();
	if (!Page.bTabFound)
	{
		OutErrorCode = TEXT("FAB_NOT_READY");
		OutError = Page.Diagnostic;
		return false;
	}

	FWaiter Waiter;
	Waiter.RequestId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Waiter.Script = WrapWithOriginGuard(Waiter.RequestId, BuildScript(Waiter.RequestId));
	Waiter.OnComplete = MoveTemp(OnComplete);
	if (GWaiters.Num() == 0 && !SlotBusy() && Page.bUrlIsFab && !Page.bLoading)
	{
		Run(MoveTemp(Waiter));
		return true;
	}
	GWaiters.Add(MoveTemp(Waiter));
	if (!bWaiterTickerActive)
	{
		bWaiterTickerActive = true;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TickWaiters), WaiterTickSeconds);
	}
	return true;
}
} // namespace McpFabBridgeDispatch
