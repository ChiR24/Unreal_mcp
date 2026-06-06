#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AnimationAuthoringSupport.h"

#if WITH_EDITOR
namespace McpAnimationAuthoring {

TSharedPtr<FJsonObject> HandleSequenceTrackActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("add_bone_track"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString BoneName = GetStringFieldAnimAuth(Params, TEXT("boneName"), TEXT(""));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

        if (BoneName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("boneName is required"), TEXT("MISSING_BONE_NAME"));
        }

        UAnimSequence* Sequence = LoadAnimSequenceFromPath(AssetPath);
        if (!Sequence)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation sequence: %s"), *AssetPath), TEXT("SEQUENCE_NOT_FOUND"));
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

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        // UE 5.1+ uses IAnimationDataController with IsValidBoneTrackName and AddBoneCurve
        IAnimationDataController& Controller = Sequence->GetController();

        // Validate the controller model is available
        if (!Controller.GetModel())
        {
            ANIM_ERROR_RESPONSE(
                TEXT("Animation data model is not available - cannot add bone track"),
                TEXT("MODEL_NOT_AVAILABLE")
            );
        }

        // Use IsValidBoneTrackName (non-deprecated) instead of GetBoneTrackIndexByName (deprecated since 5.2)
        if (!Controller.GetModel()->IsValidBoneTrackName(BoneFName))
        {
            // AddBoneCurve returns bool - check the result
            const bool bAdded = Controller.AddBoneCurve(BoneFName);
            if (!bAdded)
            {
                ANIM_ERROR_RESPONSE(
                    FString::Printf(TEXT("AddBoneCurve failed for bone '%s' - the bone may not be valid for this animation"), *BoneName),
                    TEXT("BONE_TRACK_ADD_FAILED")
                );
            }

            if (!Controller.GetModel()->IsValidBoneTrackName(BoneFName))
            {
                ANIM_ERROR_RESPONSE(
                    FString::Printf(TEXT("Bone track '%s' was not found after AddBoneCurve succeeded - internal inconsistency"), *BoneName),
                    TEXT("BONE_TRACK_ADD_FAILED")
                );
            }
        }
#elif ENGINE_MAJOR_VERSION >= 5
        // UE 5.0 approach - uses FindBoneTrackByName which returns a pointer
        IAnimationDataController& Controller = Sequence->GetController();

        const FBoneAnimationTrack* Track = Controller.GetModel()->FindBoneTrackByName(BoneFName);
        if (Track == nullptr)
        {
            // UE 5.0 doesn't have AddBoneCurve - use AddNewRawTrack directly on the sequence
            // AddNewRawTrack is deprecated in UE 5.1+ but needed for UE 5.0 compatibility
            PRAGMA_DISABLE_DEPRECATION_WARNINGS
            FRawAnimSequenceTrack NewTrack;
            Sequence->AddNewRawTrack(BoneFName, &NewTrack);
            PRAGMA_ENABLE_DEPRECATION_WARNINGS

            // Verify the bone track was actually added
            const FBoneAnimationTrack* AddedTrack = Controller.GetModel()->FindBoneTrackByName(BoneFName);
            if (AddedTrack == nullptr)
            {
                ANIM_ERROR_RESPONSE(
                    FString::Printf(TEXT("Failed to add bone track '%s' - internal error"), *BoneName),
                    TEXT("BONE_TRACK_ADD_FAILED")
                );
            }
        }
#else
        // UE4 approach
        int32 TrackIndex = Sequence->GetRawAnimationData().FindBoneTrackByName(BoneFName);
        if (TrackIndex == INDEX_NONE)
        {
            // Add raw track
            // AddNewRawTrack is deprecated in UE 5.1+ but needed for UE 4.x compatibility
            PRAGMA_DISABLE_DEPRECATION_WARNINGS
            FRawAnimSequenceTrack NewTrack;
            Sequence->AddNewRawTrack(BoneFName, &NewTrack);
            PRAGMA_ENABLE_DEPRECATION_WARNINGS
        }
#endif

        SaveAnimAsset(Sequence, bSave);

        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Bone track '%s' added"), *BoneName));
        McpHandlerUtils::AddVerification(Response, Sequence);
        return Response;
    }

    if (SubAction == TEXT("set_bone_key"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString BoneName = GetStringFieldAnimAuth(Params, TEXT("boneName"), TEXT(""));
        int32 Frame = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("frame"), 0));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

        TSharedPtr<FJsonObject> LocationObj = Params->HasField(TEXT("location")) ? Params->GetObjectField(TEXT("location")) : nullptr;
        TSharedPtr<FJsonObject> RotationObj = Params->HasField(TEXT("rotation")) ? Params->GetObjectField(TEXT("rotation")) : nullptr;
        TSharedPtr<FJsonObject> ScaleObj = Params->HasField(TEXT("scale")) ? Params->GetObjectField(TEXT("scale")) : nullptr;

        if (BoneName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("boneName is required"), TEXT("MISSING_BONE_NAME"));
        }

        UAnimSequence* Sequence = LoadAnimSequenceFromPath(AssetPath);
        if (!Sequence)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation sequence: %s"), *AssetPath), TEXT("SEQUENCE_NOT_FOUND"));
        }

        // Build transform key
        FVector Location = LocationObj.IsValid() ? GetVectorFromJsonAnim(LocationObj) : FVector::ZeroVector;
        FQuat Rotation = RotationObj.IsValid() ? GetRotatorFromJsonAnim(RotationObj).Quaternion() : FQuat::Identity;
        FVector Scale = ScaleObj.IsValid() ? GetVectorFromJsonAnim(ScaleObj) : FVector::OneVector;

        int32 TotalFrames = Sequence->GetDataModel()->GetNumberOfFrames();
        if (Frame < 0 || Frame >= TotalFrames)
        {
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("Frame %d is out of range (animation has %d frames)"), Frame, TotalFrames),
                TEXT("FRAME_OUT_OF_RANGE")
            );
        }

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        // UE 5.1+ API
        IAnimationDataController& Controller = Sequence->GetController();
        FName BoneFName(*BoneName);

        if (!Controller.GetModel())
        {
            ANIM_ERROR_RESPONSE(
                TEXT("Animation data model is not available - cannot set bone key"),
                TEXT("MODEL_NOT_AVAILABLE")
            );
        }

        // Use IsValidBoneTrackName (non-deprecated) instead of GetBoneTrackIndexByName (deprecated since 5.2)
        if (!Controller.GetModel()->IsValidBoneTrackName(BoneFName))
        {
            const bool bAdded = Controller.AddBoneCurve(BoneFName);
            if (!bAdded)
            {
                ANIM_ERROR_RESPONSE(
                    FString::Printf(TEXT("Failed to create missing bone track '%s' before keying"), *BoneName),
                    TEXT("BONE_TRACK_ADD_FAILED")
                );
            }

            // Verify the track was actually created after AddBoneCurve succeeded
            if (!Controller.GetModel()->IsValidBoneTrackName(BoneFName))
            {
                ANIM_ERROR_RESPONSE(
                    FString::Printf(TEXT("Bone track '%s' not found in animation sequence after AddBoneCurve. Add the track first using add_bone_track."), *BoneName),
                    TEXT("BONE_TRACK_NOT_FOUND")
                );
            }
        }

        // UpdateBoneTrackKeys preserves other frames; SetBoneTrackKeys would replace the entire track
        FInt32Range KeyRange(Frame, Frame + 1);
        if (!Controller.UpdateBoneTrackKeys(BoneFName, KeyRange, {Location}, {Rotation}, {Scale}))
        {
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("Failed to set bone key at frame %d"), Frame),
                TEXT("BONE_KEY_SET_FAILED")
            );
        }
#elif ENGINE_MAJOR_VERSION >= 5
        // UE 5.0 API - uses FindBoneTrackByName which returns a pointer
        IAnimationDataController& Controller = Sequence->GetController();
        FName BoneFName(*BoneName);

        const FBoneAnimationTrack* Track = Controller.GetModel()->FindBoneTrackByName(BoneFName);
        if (Track == nullptr)
        {
            // UE 5.0 doesn't have AddBoneCurve - use AddNewRawTrack
            // AddNewRawTrack is deprecated in UE 5.1+ but needed for UE 5.0 compatibility
            PRAGMA_DISABLE_DEPRECATION_WARNINGS
            FRawAnimSequenceTrack NewTrack;
            Sequence->AddNewRawTrack(BoneFName, &NewTrack);
            PRAGMA_ENABLE_DEPRECATION_WARNINGS
        }

        // Verify bone track exists before setting keys
        const FBoneAnimationTrack* VerifiedTrack = Controller.GetModel()->FindBoneTrackByName(BoneFName);
        if (VerifiedTrack == nullptr)
        {
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("Bone track '%s' not found in animation sequence. Add the track first using add_bone_track."), *BoneName),
                TEXT("BONE_TRACK_NOT_FOUND")
            );
        }

        // UE 5.0 fallback: SetBoneTrackKeys replaces the entire track (no UpdateBoneTrackKeys available)
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
#endif // WITH_EDITOR
