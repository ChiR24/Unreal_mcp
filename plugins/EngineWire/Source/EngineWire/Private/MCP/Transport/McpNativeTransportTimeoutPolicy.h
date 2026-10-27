#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace McpNativeTransportTimeoutPolicy {
double ResolveToolCallTimeoutSeconds(
    const FString &ToolName, const TSharedPtr<FJsonObject> &Arguments,
    int32 MaxMovieRenderTimeoutMs,
    int32 MaxMovieRenderCancellationWaitMs);
}
