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
 * BuildScript is handed the armed request id, so the id embedded in the script
 * and the id being awaited cannot drift apart.
 *
 * Returns false without invoking OnComplete when the page cannot be reached or
 * another call is outstanding; OutError and OutErrorCode say which.
 */
bool Dispatch(
	TFunctionRef<FString(const FString& RequestId)> BuildScript,
	TFunction<void(bool, const FString&)> OnComplete,
	FString& OutError,
	FString& OutErrorCode);
}
