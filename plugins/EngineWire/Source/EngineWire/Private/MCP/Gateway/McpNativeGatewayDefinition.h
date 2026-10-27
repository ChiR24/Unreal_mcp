// McpNativeGatewayDefinition.h — static 'unreal' gateway tool definition

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Builds the static, non-canonical 'unreal' gateway tool definition returned by
 * tools/list when native gateway mode is enabled. The gateway is NOT registered
 * in FMcpToolRegistry, so it never enters the canonical 23-tool parity set.
 */
TSharedPtr<FJsonObject> BuildUnrealGatewayToolDefinition();
