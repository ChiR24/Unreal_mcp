#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"

class UAnimSequence;
class USkeleton;

namespace McpAnimationHandlers {
#if WITH_EDITOR
int32 ApplyProceduralBoneTracks(UAnimSequence *NewSequence,
                                USkeleton *TargetSkeleton,
                                const TArray<TSharedPtr<FJsonValue>> &Tracks,
                                int32 NumFrames);
#endif
} // namespace McpAnimationHandlers
