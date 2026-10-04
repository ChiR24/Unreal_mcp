#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AnimationAuthoring/McpAutomationBridge_AnimationAuthoringSupport.h"

namespace McpAnimationAuthoring {

TSharedPtr<FJsonObject> HandleSequenceSettingsActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("add_sync_marker"))
    {
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        FString MarkerName = GetJsonStringField(Params, TEXT("markerName"), TEXT(""));
        int32 Frame = static_cast<int32>(GetJsonNumberField(Params, TEXT("frame"), 0));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

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
        FrameRate = Sequence->GetSamplingFrameRate().AsDecimal();
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
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        bool bEnableRootMotion = GetJsonBoolField(Params, TEXT("enableRootMotion"), true);
        FString RootMotionRootLock = GetJsonStringField(Params, TEXT("rootMotionRootLock"), TEXT("RefPose"));
        bool bForceRootLock = GetJsonBoolField(Params, TEXT("forceRootLock"), false);
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

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
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        FString AdditiveAnimType = GetJsonStringField(Params, TEXT("additiveAnimType"), TEXT("NoAdditive"));
        FString BasePoseType = GetJsonStringField(Params, TEXT("basePoseType"), TEXT("RefPose"));
        FString BasePoseAnimation = GetJsonStringField(Params, TEXT("basePoseAnimation"), TEXT(""));
        int32 BasePoseFrame = static_cast<int32>(GetJsonNumberField(Params, TEXT("basePoseFrame"), 0));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

        UAnimSequence* Sequence = LoadAnimSequenceFromPath(AssetPath);
        if (!Sequence)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation sequence: %s"), *AssetPath), TEXT("SEQUENCE_NOT_FOUND"));
        }
        // A base pose animation that does not load was skipped and the call
        // still reported success; refuse it before anything changes.
        UAnimSequence* BaseAnim = nullptr;
        if (!BasePoseAnimation.IsEmpty())
        {
            BaseAnim = LoadAnimSequenceFromPath(NormalizeAnimPath(BasePoseAnimation));
            if (!BaseAnim)
            {
                ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load base pose animation: %s"), *BasePoseAnimation), TEXT("ANIMATION_NOT_FOUND"));
            }
        }

        // An unknown name used to turn additive OFF and still answer success.
        const bool bLocal = AdditiveAnimType == TEXT("LocalSpaceAdditive");
        const bool bMesh = AdditiveAnimType == TEXT("MeshSpaceAdditive");
        const bool bFrame = BasePoseType == TEXT("AnimationFrame") || BasePoseType == TEXT("AnimFrame");
        const bool bScaled = BasePoseType == TEXT("AnimationScaled") || BasePoseType == TEXT("AnimScaled");
        if ((!bLocal && !bMesh && AdditiveAnimType != TEXT("NoAdditive")) ||
            (!bFrame && !bScaled && BasePoseType != TEXT("RefPose")))
        {
            ANIM_ERROR_RESPONSE(TEXT("additiveAnimType must be NoAdditive, LocalSpaceAdditive or MeshSpaceAdditive, and basePoseType RefPose, AnimScaled or AnimFrame"), TEXT("INVALID_ARGUMENT"));
        }
        Sequence->Modify();
        Sequence->AdditiveAnimType = bLocal ? AAT_LocalSpaceBase : (bMesh ? AAT_RotationOffsetMeshSpace : AAT_None);
        Sequence->RefPoseType = bFrame ? ABPT_AnimFrame : (bScaled ? ABPT_AnimScaled : ABPT_RefPose);
        if (bFrame)
        {
            Sequence->RefFrameIndex = BasePoseFrame;
        }
        if (BaseAnim)
        {
            Sequence->RefPoseSeq = BaseAnim;
        }
        // Only the change event for these properties rebuilds the compressed data as deltas; without it the clip
        // played its absolute pose on top of the base until the editor restarted.
        FPropertyChangedEvent AdditiveChanged(FindFProperty<FProperty>(UAnimSequence::StaticClass(), GET_MEMBER_NAME_CHECKED(UAnimSequence, AdditiveAnimType)));
        Sequence->PostEditChangeProperty(AdditiveChanged);

        SaveAnimAsset(Sequence, bSave);

        ANIM_SUCCESS_RESPONSE(TEXT("Additive settings updated"));
        McpHandlerUtils::AddVerification(Response, Sequence);
        return Response;
    }

    // ===== 10.2 Animation Montages =====
    return nullptr;
}

} // namespace McpAnimationAuthoring
