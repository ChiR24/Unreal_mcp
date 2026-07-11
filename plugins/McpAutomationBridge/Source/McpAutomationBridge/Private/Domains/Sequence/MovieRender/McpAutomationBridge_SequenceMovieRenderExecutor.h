#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Templates/SubclassOf.h"

#if MCP_HAS_MOVIE_RENDER_PIPELINE

class UMoviePipelineExecutorBase;

namespace McpSequenceMovieRender {
struct FRenderWaitState;

TSubclassOf<UMoviePipelineExecutorBase> ResolveExecutorClass(
    const TSharedPtr<FJsonObject> &Payload, FString &OutMessage,
    FString &OutCode);
void AttachOutputCapture(UMoviePipelineExecutorBase *Executor,
                         TSharedRef<FRenderWaitState> State);
}

#endif
