#pragma once

#include "CoreMinimal.h"
#include "Animation/MorphTarget.h"
#include "Rendering/SkeletalMeshLODModel.h"

class FJsonObject;
class USkeletalMesh;

namespace McpSkeletonHandlers
{
// Parses the `deltas` array shared by create_morph_target and
// set_morph_target_deltas. Accepted item shapes:
//   {vertexIndex|index: number,
//    positionDelta|delta|position: [x,y,z] or {x,y,z},
//    normalDelta|tangentDelta (optional): [x,y,z] or {x,y,z}}
// Returns false with OutError/OutErrorCode set: MISSING_PARAM when the array
// is absent, INVALID_ARGUMENT (naming the accepted shapes) for a bad item.
bool ParseMorphTargetDeltas(const TSharedPtr<FJsonObject>& Payload, TArray<FMorphTargetDelta>& OutDeltas, FString& OutError, FString& OutErrorCode);

// The LOD's section list UMorphTarget::PopulateDeltas needs to map vertices
// onto sections; empty when the LOD does not exist.
TArray<FSkelMeshSection> GetMorphTargetLodSections(USkeletalMesh* Mesh, int32 LODIndex);
}
