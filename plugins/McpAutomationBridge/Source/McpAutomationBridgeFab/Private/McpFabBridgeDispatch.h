// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

namespace McpFabBridgeDispatch
{
/**
 * Runs one script inside the signed-in Fab page and routes its reply back.
 *
 * The page exposes a single binding name -- window.ue.mcpfab -- so only one
 * call can be outstanding. Dispatching a second rebinds that name, and the
 * first reply then reaches an object that never armed its request id and is
 * discarded by the correlation check. Each operation used to keep a callback
 * of its own, which meant every operation judged itself idle while another was
 * mid-flight: the id check still refused to hand back the wrong data, but the
 * earlier caller silently lost its answer and waited forever. One callback and
 * one slot, shared by every operation, is what makes the in-flight guard mean
 * what it says.
 *
 * BuildScript is handed the request id, so the id embedded in the script and
 * the id being awaited cannot drift apart. It runs before Dispatch returns.
 *
 * A request never fails for being early. When the tab exists but its page is not
 * on fab.com yet (a tab that has only just opened, or a bootstrap that stalled),
 * or an earlier request still holds the page, the request waits on a ticker --
 * never on the game thread -- and runs when it can, answering the caller as if
 * it had run at once. Only when the wait outlasts its budget is OnComplete called
 * with a failure payload, {error: PAGE_NOT_READY | PAGE_BUSY, message, waitedSeconds}.
 *
 * Returns false without invoking OnComplete only when there is no Fab tab at all
 * and none could be opened; OutError and OutErrorCode say so (FAB_NOT_READY).
 */
bool Dispatch(
	TFunctionRef<FString(const FString& RequestId)> BuildScript,
	TFunction<void(bool, const FString&)> OnComplete,
	FString& OutError,
	FString& OutErrorCode);

/** The sentence a caller gets when the page was still not on fab.com after WaitedSeconds. */
FString DescribePageNotReady(double WaitedSeconds);

/**
 * The reply an operation's completion receives when the dispatcher, not the page, fails a request:
 * {"error": Code, "message": Message[, "waitedSeconds": N]} with the message made safe for a JSON string.
 */
FString FailurePayload(const TCHAR* Code, const FString& Message, double WaitedSeconds = -1.0);
}
