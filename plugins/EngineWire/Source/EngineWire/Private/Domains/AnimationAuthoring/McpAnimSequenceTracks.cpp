#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AnimationAuthoring/McpAnimSupport.h"
#include "Domains/Animation/Assets/McpAnimProceduralTracks.h"

namespace McpAnimationAuthoring {

namespace {
// What a key leaves out stays the bone's own at that frame: its track's key, or the skeleton's reference pose for a
// bone with no track yet. A location left out read as (0,0,0) and put the bone on its parent, in an additive clip too.
FTransform BoneKeyBase(const UAnimSequence* Sequence, FName Bone, int32 Frame)
{
#if ENGINE_MINOR_VERSION >= 2
    const IAnimationDataModel* Model = Sequence->GetDataModel();
    if (Model && Model->IsValidBoneTrackName(Bone))
    {
        // The read answers identity where the track holds no key, and a track add_bone_track just made holds none: the
        // omitted rotation and scale became identity instead of the bone's own pose. Only a key that is there counts.
        // ponytail: a real key of exactly identity reads as the reference pose; read the track's keys if that matters.
        const FTransform Keyed = Model->GetBoneTrackTransform(Bone, FFrameNumber(Frame));
        if (!Keyed.Equals(FTransform::Identity, 0.0))
        {
            return Keyed;
        }
    }
#else
    // 5.0 and 5.1 have no transform read; a track holds one key per frame, or a single key for a constant one.
    const FBoneAnimationTrack* Track = Sequence->GetDataModel() ? Sequence->GetDataModel()->FindBoneTrackByName(Bone) : nullptr;
    const FRawAnimSequenceTrack* Raw = Track ? &Track->InternalTrackData : nullptr;
    if (Raw && Raw->PosKeys.Num() > 0 && Raw->RotKeys.Num() > 0)
    {
        const auto At = [Frame](int32 Num) { return FMath::Clamp(Frame, 0, Num - 1); };
        const FVector Scale = Raw->ScaleKeys.Num() > 0 ? FVector(Raw->ScaleKeys[At(Raw->ScaleKeys.Num())]) : FVector::OneVector;
        return FTransform(FQuat(Raw->RotKeys[At(Raw->RotKeys.Num())]), FVector(Raw->PosKeys[At(Raw->PosKeys.Num())]), Scale);
    }
#endif
    const USkeleton* Skeleton = Sequence->GetSkeleton();
    const int32 Index = Skeleton ? Skeleton->GetReferenceSkeleton().FindBoneIndex(Bone) : INDEX_NONE;
    return Index != INDEX_NONE ? Skeleton->GetReferenceSkeleton().GetRefBonePose()[Index] : FTransform::Identity;
}
} // namespace

TSharedPtr<FJsonObject> HandleSequenceTrackActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("add_bone_track"))
    {
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        FString BoneName = GetJsonStringField(Params, TEXT("boneName"), TEXT(""));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

        if (BoneName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("boneName is required"), TEXT("MISSING_BONE_NAME"));
        }

        UAnimSequence* Sequence = LoadAnimSequenceOrFail(AssetPath, Response);
        if (!Sequence)
        {
            return Response;
        }

        // Validate that the bone exists in the skeleton before trying to add a track
        USkeleton* Skeleton = Sequence->GetSkeleton();
        if (!Skeleton)
        {
            ANIM_ERROR_RESPONSE(TEXT("Animation sequence has no skeleton reference"), TEXT("NO_SKELETON"));
        }

        FName BoneFName(*BoneName);

        // Check if the bone exists in the skeleton's reference skeleton
        const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
        int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneFName);
        if (BoneIndex == INDEX_NONE)
        {
            // Provide helpful error message with available bone names
            TArray<FString> AvailableBones;
            const int32 NumBones = FMath::Min(RefSkeleton.GetNum(), 10); // Limit to first 10 bones
            for (int32 i = 0; i < NumBones; ++i)
            {
                AvailableBones.Add(RefSkeleton.GetBoneName(i).ToString());
            }
            FString BoneList = FString::Join(AvailableBones, TEXT(", "));
            if (RefSkeleton.GetNum() > 10)
            {
                BoneList += FString::Printf(TEXT(" ... and %d more"), RefSkeleton.GetNum() - 10);
            }

            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("Bone '%s' not found in skeleton. Available bones: %s"), *BoneName, *BoneList),
                TEXT("BONE_NOT_FOUND_IN_SKELETON")
            );
        }

        if (!Sequence->GetController().GetModel())
        {
            ANIM_ERROR_RESPONSE(
                TEXT("Animation data model is not available - cannot add bone track"),
                TEXT("MODEL_NOT_AVAILABLE")
            );
        }
        bool bAlreadyHadTrack = false;
        if (!McpAnimationHandlers::McpEnsureBoneTrack(Sequence, BoneFName, bAlreadyHadTrack))
        {
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("Failed to add bone track '%s' - the bone may not be valid for this animation"), *BoneName),
                TEXT("BONE_TRACK_ADD_FAILED")
            );
        }

        // A track the sequence already had answered "added" and the sequence was written again.
        Response->SetBoolField(TEXT("alreadyExists"), bAlreadyHadTrack);
        SaveAnimAssetAndReport(Sequence, bSave && !bAlreadyHadTrack, Response);

        ANIM_SUCCESS_RESPONSE(bAlreadyHadTrack
            ? FString::Printf(TEXT("The sequence already has a track for bone '%s'; nothing was changed"), *BoneName)
            : FString::Printf(TEXT("Bone track '%s' added"), *BoneName));
        McpHandlerUtils::AddVerification(Response, Sequence);
        return Response;
    }

    if (SubAction == TEXT("set_bone_key"))
    {
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        FString BoneName = GetJsonStringField(Params, TEXT("boneName"), TEXT(""));
        int32 Frame = static_cast<int32>(GetJsonNumberField(Params, TEXT("frame"), 0));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

        if (BoneName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("boneName is required"), TEXT("MISSING_BONE_NAME"));
        }

        UAnimSequence* Sequence = LoadAnimSequenceOrFail(AssetPath, Response);
        if (!Sequence)
        {
            return Response;
        }

        // A sequence of N frames has N + 1 keys, 0 to N: the last key (N) used to be refused, so no clip could be keyed
        // to end anywhere but at its reference pose.
        int32 TotalFrames = Sequence->GetDataModel()->GetNumberOfFrames();
        if (Frame < 0 || Frame > TotalFrames)
        {
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("Frame %d is out of range: the animation's keys are frames 0 to %d"), Frame, TotalFrames),
                TEXT("FRAME_OUT_OF_RANGE")
            );
        }

        // Build transform key
        const FTransform Base = BoneKeyBase(Sequence, FName(*BoneName), Frame);
        FVector Location = ExtractVectorField(Params, TEXT("location"), Base.GetLocation());
        // The contract types rotation as [pitch, yaw, roll] and the gateway
        // hands objects over as that array; GetObjectField read null for it, so
        // every key was written with identity rotation and reported success.
        FQuat Rotation = Params->HasField(TEXT("rotation"))
            ? ExtractRotatorField(Params, TEXT("rotation"), FRotator::ZeroRotator).Quaternion() : Base.GetRotation();
        FVector Scale = ExtractVectorField(Params, TEXT("scale"), Base.GetScale3D());

        IAnimationDataController& Controller = Sequence->GetController();
        FName BoneFName(*BoneName);
        if (!Controller.GetModel())
        {
            ANIM_ERROR_RESPONSE(
                TEXT("Animation data model is not available - cannot set bone key"),
                TEXT("MODEL_NOT_AVAILABLE")
            );
        }
        bool bHadTrack = false;
        if (!McpAnimationHandlers::McpEnsureBoneTrack(Sequence, BoneFName, bHadTrack))
        {
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("Failed to create missing bone track '%s' before keying"), *BoneName),
                TEXT("BONE_TRACK_ADD_FAILED")
            );
        }
#if ENGINE_MINOR_VERSION >= 1
        // UpdateBoneTrackKeys preserves other frames; SetBoneTrackKeys would replace the entire track
        FInt32Range KeyRange(Frame, Frame + 1);
        if (!Controller.UpdateBoneTrackKeys(BoneFName, KeyRange, {Location}, {Rotation}, {Scale}))
        {
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("Failed to set bone key at frame %d"), Frame),
                TEXT("BONE_KEY_SET_FAILED")
            );
        }
#else
        // UE 5.0: SetBoneTrackKeys replaces the entire track. UpdateBoneTrackKeys exists on 5.0 but writes only inside
        // the keys a track already holds, and a track AddNewRawTrack just made holds none.
        Controller.SetBoneTrackKeys(BoneFName, {Location}, {Rotation}, {Scale});
#endif

        SaveAnimAsset(Sequence, bSave);

        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Bone key set at frame %d"), Frame));
        McpHandlerUtils::AddVerification(Response, Sequence);
        return Response;
    }
    return nullptr;
}

} // namespace McpAnimationAuthoring
