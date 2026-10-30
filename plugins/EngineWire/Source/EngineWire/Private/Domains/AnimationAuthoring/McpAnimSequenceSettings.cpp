#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AnimationAuthoring/McpAnimSupport.h"
#include "Foundation/McpScopedEditorTransaction.h"

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

        // A sequence with no sampled frames reports a 0 rate (the time was divided by zero), and a marker past the end
        // never syncs.
        const double Rate = Sequence->GetSamplingFrameRate().AsDecimal();
        const double FrameRate = Rate > KINDA_SMALL_NUMBER ? Rate : 30.0;
        const float Time = static_cast<float>(Frame / FrameRate);
        if (Frame < 0 || Time > Sequence->GetPlayLength() + KINDA_SMALL_NUMBER)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("frame must lie in 0..%d (the sequence's last frame); nothing was added."),
                FMath::RoundToInt(Sequence->GetPlayLength() * FrameRate)), TEXT("INVALID_ARGUMENT"));
        }
        {
            const FMcpAssetEdit Edit(TEXT("Add Sync Marker"), {Sequence}, Response);
            FAnimSyncMarker NewMarker;
            NewMarker.MarkerName = FName(*MarkerName);
            NewMarker.Time = Time;
            Sequence->AuthoredSyncMarkers.Add(NewMarker);
            Sequence->RefreshSyncMarkerDataFromAuthored();
        }
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

        {
            const FMcpAssetEdit Edit(TEXT("Set Root Motion Settings"), {Sequence}, Response);
            Sequence->bEnableRootMotion = bEnableRootMotion;
            Sequence->bForceRootLock = bForceRootLock;
            Sequence->RootMotionRootLock = RootMotionRootLock == TEXT("AnimFirstFrame") ? ERootMotionRootLock::AnimFirstFrame
                : RootMotionRootLock == TEXT("Zero") ? ERootMotionRootLock::Zero : ERootMotionRootLock::RefPose;
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
        {
            const FMcpAssetEdit Edit(TEXT("Set Additive Settings"), {Sequence}, Response);
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
        }
        SaveAnimAsset(Sequence, bSave);

        ANIM_SUCCESS_RESPONSE(TEXT("Additive settings updated"));
        McpHandlerUtils::AddVerification(Response, Sequence);
        return Response;
    }

    if (SubAction == TEXT("set_play_rate"))
    {
        const FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        double Rate = 0.0;
        // 0 freezes the clip on its first frame wherever it plays; a negative rate plays it backwards.
        if (!Params->TryGetNumberField(TEXT("rateScale"), Rate) || Rate == 0.0 || FMath::Abs(Rate) > 10.0)
        {
            ANIM_ERROR_RESPONSE(TEXT("rateScale is how fast the clip plays: 2 twice as fast, 0.5 half as fast, -1 backwards (not 0, at most 10 either way)"), TEXT("INVALID_ARGUMENT"));
        }
        UAnimSequence* Sequence = LoadAnimSequenceFromPath(AssetPath);
        if (!Sequence)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation sequence: %s"), *AssetPath), TEXT("SEQUENCE_NOT_FOUND"));
        }
        const float Before = Sequence->RateScale;
        {
            const FMcpAssetEdit Edit(TEXT("Set Play Rate"), {Sequence}, Response);
            Sequence->RateScale = static_cast<float>(Rate);
            Sequence->PostEditChange();
        }
        const bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
        Response->SetBoolField(TEXT("saved"), SaveAnimAsset(Sequence, bSave) && bSave);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("%s plays at %gx (was %gx): %.2f s a loop"), *Sequence->GetName(), Sequence->RateScale, Before,
                                              Sequence->GetPlayLength() / FMath::Abs(Sequence->RateScale)));
        Response->SetNumberField(TEXT("rateScale"), Sequence->RateScale);
        Response->SetNumberField(TEXT("previousRateScale"), Before);
        McpHandlerUtils::AddVerification(Response, Sequence);
        return Response;
    }

    // ===== 10.2 Animation Montages =====
    return nullptr;
}

} // namespace McpAnimationAuthoring
