#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"

class UAnimSequence;
class UFactory;
class USkeleton;

namespace McpAnimationHandlers {
// The AssetClass asset at Path/Name: the existing one (bOutExisting) or a new one from Factory. Null, with
// OutCode/OutError set, when an asset of another type holds the name, it cannot load, or the factory fails.
UObject *CreateOrReuseAnimAsset(UClass *AssetClass, UFactory *Factory, const FString &Path,
                                const FString &Name, bool &bOutExisting, FString &OutCode,
                                FString &OutError);
// Sets Sequence's length to NumFrames at FrameRate fps.
void SetAnimSequenceFrames(UAnimSequence *Sequence, int32 NumFrames, int32 FrameRate);
// Gives Sequence a track for Bone unless it has one (bOutHad), as each engine adds one: AddBoneCurve from 5.2,
// AddBoneTrack on 5.1, AddNewRawTrack on 5.0. True when the sequence holds the track afterwards.
bool McpEnsureBoneTrack(UAnimSequence *Sequence, FName Bone, bool &bOutHad);
int32 ApplyProceduralBoneTracks(UAnimSequence *NewSequence,
                                USkeleton *TargetSkeleton,
                                const TArray<TSharedPtr<FJsonValue>> &Tracks,
                                int32 NumFrames);
} // namespace McpAnimationHandlers
