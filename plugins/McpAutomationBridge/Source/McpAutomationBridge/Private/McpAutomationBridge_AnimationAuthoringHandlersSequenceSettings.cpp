#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AnimationAuthoringSupport.h"

#if WITH_EDITOR
namespace McpAnimationAuthoring {

TSharedPtr<FJsonObject> HandleSequenceSettingsActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("add_sync_marker"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString MarkerName = GetStringFieldAnimAuth(Params, TEXT("markerName"), TEXT(""));
        int32 Frame = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("frame"), 0));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

        if (MarkerName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("markerName is required"), TEXT("MISSING_MARKER_NAME"));
        }

        UAnimSequence* Sequence = LoadAnimSequenceFromPath(AssetPath);
        if (!Sequence)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation sequence: %s"), *AssetPath), TEXT("SEQUENCE_NOT_FOUND"));
        }

        // Calculate time from frame
        float FrameRate = 30.0f;
#if ENGINE_MAJOR_VERSION >= 5
        FrameRate = Sequence->GetSamplingFrameRate().AsDecimal();
#endif
        float Time = static_cast<float>(Frame) / FrameRate;

        // Add sync marker
        FAnimSyncMarker NewMarker;
        NewMarker.MarkerName = FName(*MarkerName);
        NewMarker.Time = Time;

        Sequence->AuthoredSyncMarkers.Add(NewMarker);
        Sequence->RefreshSyncMarkerDataFromAuthored();

        SaveAnimAsset(Sequence, bSave);

        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Sync marker '%s' added"), *MarkerName));
        McpHandlerUtils::AddVerification(Response, Sequence);
        return Response;
    }

    if (SubAction == TEXT("set_root_motion_settings"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        bool bEnableRootMotion = GetBoolFieldAnimAuth(Params, TEXT("enableRootMotion"), true);
        FString RootMotionRootLock = GetStringFieldAnimAuth(Params, TEXT("rootMotionRootLock"), TEXT("RefPose"));
        bool bForceRootLock = GetBoolFieldAnimAuth(Params, TEXT("forceRootLock"), false);
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

        UAnimSequence* Sequence = LoadAnimSequenceFromPath(AssetPath);
        if (!Sequence)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation sequence: %s"), *AssetPath), TEXT("SEQUENCE_NOT_FOUND"));
        }

        Sequence->bEnableRootMotion = bEnableRootMotion;
        Sequence->bForceRootLock = bForceRootLock;

        // Set root motion lock type
        if (RootMotionRootLock == TEXT("AnimFirstFrame"))
        {
            Sequence->RootMotionRootLock = ERootMotionRootLock::AnimFirstFrame;
        }
        else if (RootMotionRootLock == TEXT("Zero"))
        {
            Sequence->RootMotionRootLock = ERootMotionRootLock::Zero;
        }
        else
        {
            Sequence->RootMotionRootLock = ERootMotionRootLock::RefPose;
        }

        SaveAnimAsset(Sequence, bSave);

        ANIM_SUCCESS_RESPONSE(TEXT("Root motion settings updated"));
        McpHandlerUtils::AddVerification(Response, Sequence);
        return Response;
    }

    if (SubAction == TEXT("set_additive_settings"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString AdditiveAnimType = GetStringFieldAnimAuth(Params, TEXT("additiveAnimType"), TEXT("NoAdditive"));
        FString BasePoseType = GetStringFieldAnimAuth(Params, TEXT("basePoseType"), TEXT("RefPose"));
        FString BasePoseAnimation = GetStringFieldAnimAuth(Params, TEXT("basePoseAnimation"), TEXT(""));
        int32 BasePoseFrame = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("basePoseFrame"), 0));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

        UAnimSequence* Sequence = LoadAnimSequenceFromPath(AssetPath);
        if (!Sequence)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation sequence: %s"), *AssetPath), TEXT("SEQUENCE_NOT_FOUND"));
        }

        // Set additive anim type
        if (AdditiveAnimType == TEXT("LocalSpaceAdditive"))
        {
            Sequence->AdditiveAnimType = AAT_LocalSpaceBase;
        }
        else if (AdditiveAnimType == TEXT("MeshSpaceAdditive"))
        {
            Sequence->AdditiveAnimType = AAT_RotationOffsetMeshSpace;
        }
        else
        {
            Sequence->AdditiveAnimType = AAT_None;
        }

        // Set base pose type
        if (BasePoseType == TEXT("AnimationFrame"))
        {
            Sequence->RefPoseType = ABPT_AnimFrame;
            Sequence->RefFrameIndex = BasePoseFrame;
        }
        else if (BasePoseType == TEXT("AnimationScaled"))
        {
            Sequence->RefPoseType = ABPT_AnimScaled;
        }
        else
        {
            Sequence->RefPoseType = ABPT_RefPose;
        }

        // Set base pose animation if provided
        if (!BasePoseAnimation.IsEmpty())
        {
            UAnimSequence* BaseAnim = LoadAnimSequenceFromPath(BasePoseAnimation);
            if (BaseAnim)
            {
                Sequence->RefPoseSeq = BaseAnim;
            }
        }

        SaveAnimAsset(Sequence, bSave);

        ANIM_SUCCESS_RESPONSE(TEXT("Additive settings updated"));
        McpHandlerUtils::AddVerification(Response, Sequence);
        return Response;
    }

    // ===== 10.2 Animation Montages =====
    return nullptr;
}

} // namespace McpAnimationAuthoring
#endif // WITH_EDITOR
