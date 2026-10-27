#include "Domains/Skeleton/Bones/McpSkeletonBoneEdits.h"

#include "Animation/Skeleton.h"
#include "ReferenceSkeleton.h"
#include "Runtime/Launch/Resources/Version.h"
#include "UObject/UnrealType.h"

namespace McpSkeletonBoneEdits
{
namespace
{
using FRetargetModes = TMap<FName, EBoneTranslationRetargetingMode::Type>;

FRetargetModes SnapshotModes(const USkeleton* Skeleton)
{
    FRetargetModes Modes;
    const FReferenceSkeleton& Ref = Skeleton->GetReferenceSkeleton();
    for (int32 Index = 0; Index < Ref.GetRawBoneNum(); ++Index)
    {
        Modes.Add(Ref.GetRawRefBoneInfo()[Index].Name, Skeleton->GetBoneTranslationRetargetingMode(Index));
    }
    return Modes;
}

// BoneTree is protected, so it is resized through reflection. The engine resizes it only when it merges bones in from a
// mesh: create_skeleton and add_bone leave it short, and a removal or reorder would leave modes on other bones.
void SyncBoneTree(USkeleton* Skeleton, const FRetargetModes& Modes)
{
    FArrayProperty* TreeProperty = FindFProperty<FArrayProperty>(USkeleton::StaticClass(), TEXT("BoneTree"));
    if (!TreeProperty)
    {
        return;
    }
    const FReferenceSkeleton& Ref = Skeleton->GetReferenceSkeleton();
    FScriptArrayHelper(TreeProperty, TreeProperty->ContainerPtrToValuePtr<void>(Skeleton)).Resize(Ref.GetRawBoneNum());
    for (int32 Index = 0; Index < Ref.GetRawBoneNum(); ++Index)
    {
        const EBoneTranslationRetargetingMode::Type* Mode = Modes.Find(Ref.GetRawRefBoneInfo()[Index].Name);
        Skeleton->SetBoneTranslationRetargetingMode(Index, Mode ? *Mode : EBoneTranslationRetargetingMode::Animation);
    }
}

#if !(ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
// A raw bone to add back: its parent by name (NAME_None for a root) and its local reference pose.
struct FTailBone
{
    FMeshBoneInfo Info;
    FName Parent;
    FTransform Pose;
};

FTailBone ReadTailBone(const FReferenceSkeleton& Ref, int32 Index)
{
    const int32 Parent = Ref.GetRawParentIndex(Index);
    return {Ref.GetRawRefBoneInfo()[Index], Parent == INDEX_NONE ? NAME_None : Ref.GetRawRefBoneInfo()[Parent].Name, Ref.GetRawRefBonePose()[Index]};
}

FTransform ComponentSpacePose(const FReferenceSkeleton& Ref, int32 BoneIndex)
{
    FTransform Pose = FTransform::Identity;
    for (int32 Index = BoneIndex; Index != INDEX_NONE; Index = Ref.GetRawParentIndex(Index))
    {
        Pose *= Ref.GetRawRefBonePose()[Index];
    }
    return Pose;
}

// Drops every raw bone from FirstIndex on and adds Tail back in order, each bone after its parent.
void RebuildTail(USkeleton* Skeleton, int32 FirstIndex, const TArray<FTailBone>& Tail)
{
    // The skeleton is not const, only its getter. USkeleton::RemoveBonesFromSkeleton would also tell every loaded
    // animation the dropped bones are gone, and they would strip the tracks of the bones added back below.
    FReferenceSkeleton& Ref = const_cast<FReferenceSkeleton&>(Skeleton->GetReferenceSkeleton());
    TArray<FName> Dropped;
    for (int32 Index = FirstIndex; Index < Ref.GetRawBoneNum(); ++Index)
    {
        Dropped.Add(Ref.GetRawRefBoneInfo()[Index].Name);
    }
    // No skeleton: rebuilt without virtual bones and without notifying anyone of the half-built hierarchy; the
    // modifier rebuilds it with both once the tail is back.
    Ref.RemoveBonesByName(nullptr, Dropped);
    FReferenceSkeletonModifier Modifier(Skeleton);
    for (const FTailBone& Bone : Tail)
    {
        FMeshBoneInfo Info = Bone.Info;
        Info.ParentIndex = Modifier.FindBoneIndex(Bone.Parent);
        // 5.0-5.2's Add asserts a single root, so no skeleton there has a second root to add back.
        Modifier.Add(Info, Bone.Pose);
    }
}
#endif
}

void RemoveBone(USkeleton* Skeleton, FName Bone, bool bRemoveChildren)
{
    const FRetargetModes Modes = SnapshotModes(Skeleton);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
    FReferenceSkeletonModifier(Skeleton).Remove(Bone, bRemoveChildren);
#else
    // Every bone after Bone but its dropped branch; kept direct children move to its parent with their component-space
    // pose, ChildLocal * BoneLocal (child then parent), as 5.3's Remove does.
    const FReferenceSkeleton& Ref = Skeleton->GetReferenceSkeleton();
    const int32 BoneIndex = Ref.FindRawBoneIndex(Bone);
    const FTailBone Removed = ReadTailBone(Ref, BoneIndex);
    TArray<FTailBone> Tail;
    for (int32 Index = BoneIndex + 1; Index < Ref.GetRawBoneNum(); ++Index)
    {
        if (bRemoveChildren && Ref.BoneIsChildOf(Index, BoneIndex))
        {
            continue;
        }
        FTailBone& Kept = Tail.Add_GetRef(ReadTailBone(Ref, Index));
        if (Ref.GetRawParentIndex(Index) == BoneIndex)
        {
            Kept.Parent = Removed.Parent;
            Kept.Pose = Kept.Pose * Removed.Pose;
        }
    }
    RebuildTail(Skeleton, BoneIndex, Tail);
#endif
    SyncBoneTree(Skeleton, Modes);
}

int32 SetBoneParent(USkeleton* Skeleton, FName Bone, FName Parent)
{
    const FRetargetModes Modes = SnapshotModes(Skeleton);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
    FReferenceSkeletonModifier(Skeleton).SetParent(Bone, Parent);
#else
    // Every bone after Bone outside its branch, then the branch, Bone first and under Parent, so each bone still follows
    // its parent. 5.3's SetParent re-sorts the whole skeleton instead: the indices differ, the parents and poses do not.
    const FReferenceSkeleton& Ref = Skeleton->GetReferenceSkeleton();
    const int32 BoneIndex = Ref.FindRawBoneIndex(Bone);
    TArray<FTailBone> Tail;
    TArray<FTailBone> Branch;
    for (int32 Index = BoneIndex; Index < Ref.GetRawBoneNum(); ++Index)
    {
        (Index == BoneIndex || Ref.BoneIsChildOf(Index, BoneIndex) ? Branch : Tail).Add(ReadTailBone(Ref, Index));
    }
    Branch[0].Parent = Parent;
    Branch[0].Pose = ComponentSpacePose(Ref, BoneIndex).GetRelativeTransform(ComponentSpacePose(Ref, Ref.FindRawBoneIndex(Parent)));
    Tail.Append(Branch);
    RebuildTail(Skeleton, BoneIndex, Tail);
#endif
    SyncBoneTree(Skeleton, Modes);
    return Skeleton->GetReferenceSkeleton().FindRawBoneIndex(Bone);
}

void FitBoneTree(USkeleton* Skeleton)
{
    SyncBoneTree(Skeleton, SnapshotModes(Skeleton));
}
}
