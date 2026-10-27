#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AnimationAuthoring/McpAnimSupport.h"

namespace McpAnimationAuthoring {

TSharedPtr<FJsonObject> HandleSequenceEventActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("set_curve_key"))
    {
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        FString CurveName = GetJsonStringField(Params, TEXT("curveName"), TEXT(""));
        int32 Frame = static_cast<int32>(GetJsonNumberField(Params, TEXT("frame"), 0));
        float Value = static_cast<float>(GetJsonNumberField(Params, TEXT("value"), 0.0));
        bool bCreateIfMissing = GetJsonBoolField(Params, TEXT("createIfMissing"), true);
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

        if (CurveName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("curveName is required"), TEXT("MISSING_CURVE_NAME"));
        }

        UAnimSequence* Sequence = LoadAnimSequenceFromPath(AssetPath);
        if (!Sequence)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation sequence: %s"), *AssetPath), TEXT("SEQUENCE_NOT_FOUND"));
        }

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
        // UE 5.3+ API - FAnimationCurveIdentifier takes FName directly
        IAnimationDataController& Controller = Sequence->GetController();
        FAnimationCurveIdentifier CurveId(FName(*CurveName), ERawCurveTrackTypes::RCT_Float);
#else
        // UE 5.0-5.2: a curve is a smart name the skeleton must know. A name with no UID was refused by AddCurve,
        // and 5.0 wrote no key at all, each answering "Curve key set".
        IAnimationDataController& Controller = Sequence->GetController();
        FSmartName SmartCurveName;
        SmartCurveName.DisplayName = FName(*CurveName);
        if (USkeleton* CurveSkeleton = Sequence->GetSkeleton())
        {
            CurveSkeleton->VerifySmartName(USkeleton::AnimCurveMappingName, SmartCurveName);
        }
        FAnimationCurveIdentifier CurveId(SmartCurveName, ERawCurveTrackTypes::RCT_Float);
#endif

        // A curve the sequence lacks with createIfMissing false, and a key the controller refused, answered "set".
        const bool bHadCurve = Sequence->GetDataModel()->FindFloatCurve(CurveId) != nullptr;
        if (!bHadCurve && !bCreateIfMissing)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("The sequence has no curve '%s'; createIfMissing true adds it"), *CurveName), TEXT("CURVE_NOT_FOUND"));
        }
        if (!bHadCurve && !Controller.AddCurve(CurveId, AACF_DefaultCurve))
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Curve '%s' could not be added to the sequence"), *CurveName), TEXT("CURVE_ADD_FAILED"));
        }
        const float FrameTime = static_cast<float>(Frame) / Sequence->GetSamplingFrameRate().AsDecimal();
        if (!Controller.SetCurveKey(CurveId, FRichCurveKey(FrameTime, Value)))
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("The key at frame %d on curve '%s' was not set"), Frame, *CurveName), TEXT("CURVE_KEY_FAILED"));
        }
        const FFloatCurve* Curve = Sequence->GetDataModel()->FindFloatCurve(CurveId);
        Response->SetStringField(TEXT("curveName"), CurveName);
        Response->SetNumberField(TEXT("frame"), Frame);
        Response->SetNumberField(TEXT("value"), Curve ? Curve->FloatCurve.Eval(FrameTime) : Value);
        Response->SetBoolField(TEXT("curveCreated"), !bHadCurve);
        SaveAnimAssetAndReport(Sequence, bSave, Response);

        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Curve key set at frame %d"), Frame));
        McpHandlerUtils::AddVerification(Response, Sequence);
        return Response;
    }

    if (SubAction == TEXT("add_notify"))
    {
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        FString NotifyClass = GetJsonStringField(Params, TEXT("notifyClass"), TEXT(""));
        int32 Frame = static_cast<int32>(GetJsonNumberField(Params, TEXT("frame"), 0));
        int32 TrackIndex = static_cast<int32>(GetJsonNumberField(Params, TEXT("trackIndex"), 0));
        FString NotifyName = GetJsonStringField(Params, TEXT("notifyName"), TEXT(""));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

        // The contract requires one of notifyClass / notifyName, not both:
        // a class alone names the event after itself (dogfood #82).
        if (NotifyClass.IsEmpty() && NotifyName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("At least one of notifyClass or notifyName is required"), TEXT("MISSING_NOTIFY_PARAMS"));
        }

        // Resolve notify class BEFORE modifying the asset
        UClass* ResolvedNotifyClass = nullptr;
        if (!NotifyClass.IsEmpty())
        {
            TArray<FString> Tried;
            ResolvedNotifyClass = ResolveNotifyClassByName(NotifyClass, TEXT("AnimNotify_"), UAnimNotify::StaticClass(), Tried);
            if (!ResolvedNotifyClass)
            {
                ANIM_ERROR_RESPONSE(
                    FString::Printf(TEXT("AnimNotify class '%s' not found (tried %s). Use a concrete subclass like AnimNotify_PlaySound or a custom AnimNotify blueprint class path."), *NotifyClass, *FString::Join(Tried, TEXT(", "))),
                    TEXT("CLASS_NOT_FOUND")
                );
            }
            if (ResolvedNotifyClass->HasAnyClassFlags(CLASS_Abstract))
            {
                ANIM_ERROR_RESPONSE(
                    FString::Printf(TEXT("Cannot create AnimNotify: '%s' is an abstract class. Use a concrete subclass like AnimNotify_PlaySound or create a custom AnimNotify blueprint."), *ResolvedNotifyClass->GetName()),
                    TEXT("ABSTRACT_CLASS_ERROR")
                );
            }
            if (NotifyName.IsEmpty())
            {
                NotifyName = NotifyNameFromClass(ResolvedNotifyClass, TEXT("AnimNotify_"));
            }
        }

        UAnimSequenceBase* AnimAsset = Cast<UAnimSequenceBase>(StaticLoadObject(UAnimSequenceBase::StaticClass(), nullptr, *AssetPath));
        if (!AnimAsset)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation asset: %s"), *AssetPath), TEXT("ASSET_NOT_FOUND"));
        }

        // Calculate time from frame
        float FrameRate = 30.0f;
        if (UAnimSequence* Seq = Cast<UAnimSequence>(AnimAsset))
        {
            FrameRate = Seq->GetSamplingFrameRate().AsDecimal();
        }
        if (FrameRate <= KINDA_SMALL_NUMBER)
        {
            // A sequence without sampled frames reports a 0 rate; fall back to 30 fps
            // instead of dividing by zero (notify time read back as 0, dogfood #81).
            FrameRate = 30.0f;
        }
        float TriggerTime = static_cast<float>(Frame) / FrameRate;
        if (!Params->HasField(TEXT("frame")) && Params->HasField(TEXT("time")))
        {
            // Runtime-style callers pass seconds; honour them when no frame is given.
            TriggerTime = static_cast<float>(GetJsonNumberField(Params, TEXT("time"), 0.0));
            Frame = FMath::RoundToInt(TriggerTime * FrameRate);
        }

        FAnimNotifyEvent& NotifyEvent = AnimAsset->Notifies.AddDefaulted_GetRef();
        // Link the event to the asset: SetTime alone leaves LinkedSequence null and
        // GetTime() reads back 0 (every notify landed at 0.00s, dogfood #81).
        NotifyEvent.Link(AnimAsset, TriggerTime);
        NotifyEvent.TrackIndex = TrackIndex;
        NotifyEvent.NotifyName = FName(*NotifyName);

        if (ResolvedNotifyClass)
        {
            UAnimNotify* NewNotify = NewObject<UAnimNotify>(AnimAsset, ResolvedNotifyClass);
            if (!NewNotify)
            {
                AnimAsset->Notifies.Pop();
                ANIM_ERROR_RESPONSE(
                    FString::Printf(TEXT("Failed to create AnimNotify instance of class '%s'"), *NotifyClass),
                    TEXT("INSTANTIATION_FAILED")
                );
            }
            NotifyEvent.Notify = NewNotify;
        }

        AnimAsset->RefreshCacheData();
        SaveAnimAsset(AnimAsset, bSave);

        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Notify '%s' added at %.3fs"), *NotifyName, TriggerTime));
        Response->SetStringField(TEXT("notifyName"), NotifyName);
        Response->SetStringField(TEXT("notifyClass"), ResolvedNotifyClass ? ResolvedNotifyClass->GetPathName() : FString());
        Response->SetNumberField(TEXT("time"), TriggerTime);
        Response->SetNumberField(TEXT("frame"), Frame);
        Response->SetNumberField(TEXT("frameRate"), FrameRate);
        Response->SetNumberField(TEXT("trackIndex"), TrackIndex);
        Response->SetNumberField(TEXT("notifyIndex"), AnimAsset->Notifies.Num() - 1);
        McpHandlerUtils::AddVerification(Response, AnimAsset);
        return Response;
    }

    if (SubAction == TEXT("add_notify_state"))
    {
        // Lives in McpAnimSequenceNotifyStates.cpp
        // with the shared notify-class resolver (250 pure-line ceiling).
        return HandleSequenceNotifyStateAction(Params, Response);
    }
    return nullptr;
}

} // namespace McpAnimationAuthoring
