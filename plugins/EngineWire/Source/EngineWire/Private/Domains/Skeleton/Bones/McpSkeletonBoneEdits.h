#pragma once

#include "CoreMinimal.h"

class USkeleton;

// Bone removal and re-parenting on a skeleton, the same on every engine version. UE 5.3 and later have
// FReferenceSkeletonModifier::Remove and ::SetParent; UE 5.0-5.2 have neither, so there every bone from the edited one
// on is dropped and added back where it now belongs. Neither engine path touches USkeleton::BoneTree, which holds each
// bone's translation-retargeting mode by bone index, so both end by putting it back in step, mode by bone name.
namespace McpSkeletonBoneEdits
{
// Removes Bone and, when bRemoveChildren, its descendants; otherwise its direct children move up to its parent and
// keep their component-space reference pose.
void RemoveBone(USkeleton* Skeleton, FName Bone, bool bRemoveChildren);
// Moves Bone, with its descendants, under Parent, keeping its component-space reference pose; returns its new index.
// Both bones must exist, Parent must not be Bone or one of its descendants, nor already Bone's parent.
int32 SetBoneParent(USkeleton* Skeleton, FName Bone, FName Parent);
// Sizes BoneTree to the skeleton's bones after bones were only appended (create_skeleton, add_bone): each bone keeps
// its mode, a new one gets the default.
void FitBoneTree(USkeleton* Skeleton);
}
