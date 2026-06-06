#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AnimationAuthoringSupport.h"

#if WITH_EDITOR
namespace McpAnimationAuthoring {

TSharedPtr<FJsonObject> HandleSequenceEventActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("set_curve_key"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString CurveName = GetStringFieldAnimAuth(Params, TEXT("curveName"), TEXT(""));
        int32 Frame = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("frame"), 0));
        float Value = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("value"), 0.0));
        bool bCreateIfMissing = GetBoolFieldAnimAuth(Params, TEXT("createIfMissing"), true);
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

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
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        // UE 5.1-5.2 API - FAnimationCurveIdentifier takes FSmartName
        IAnimationDataController& Controller = Sequence->GetController();
        FSmartName SmartCurveName;
        SmartCurveName.DisplayName = FName(*CurveName);
        FAnimationCurveIdentifier CurveId(SmartCurveName, ERawCurveTrackTypes::RCT_Float);
#endif

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        // Find or create curve
        const FFloatCurve* ExistingCurve = Sequence->GetDataModel()->FindFloatCurve(CurveId);
        if (!ExistingCurve && bCreateIfMissing)
        {
            Controller.AddCurve(CurveId, AACF_DefaultCurve);
        }

        // Set key value
        float FrameTime = static_cast<float>(Frame) / Sequence->GetSamplingFrameRate().AsDecimal();
        Controller.SetCurveKey(CurveId, FRichCurveKey(FrameTime, Value));
#endif

        SaveAnimAsset(Sequence, bSave);

        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Curve key set at frame %d"), Frame));
        McpHandlerUtils::AddVerification(Response, Sequence);
        return Response;
    }

if (SubAction == TEXT("add_notify"))
{
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
    FString NotifyClass = GetStringFieldAnimAuth(Params, TEXT("notifyClass"), TEXT(""));
    int32 Frame = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("frame"), 0));
    int32 TrackIndex = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("trackIndex"), 0));
    FString NotifyName = GetStringFieldAnimAuth(Params, TEXT("notifyName"), TEXT(""));
    bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

    if (NotifyClass.IsEmpty() && NotifyName.IsEmpty())
    {
        ANIM_ERROR_RESPONSE(TEXT("At least one of notifyClass or notifyName is required"), TEXT("MISSING_NOTIFY_PARAMS"));
    }

    // Resolve notify class BEFORE modifying the asset
    UClass* ResolvedNotifyClass = nullptr;
    if (!NotifyClass.IsEmpty())
    {
        FString FullClassName = NotifyClass;
        if (!FullClassName.StartsWith(TEXT("AnimNotify_")))
        {
            FullClassName = TEXT("AnimNotify_") + NotifyClass;
        }

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        ResolvedNotifyClass = FindFirstObject<UClass>(*FullClassName, EFindFirstObjectOptions::None);
#else
        ResolvedNotifyClass = ResolveClassByName(FullClassName);
#endif
        if (!ResolvedNotifyClass)
        {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
            ResolvedNotifyClass = FindFirstObject<UClass>(*NotifyClass, EFindFirstObjectOptions::None);
#else
            ResolvedNotifyClass = ResolveClassByName(NotifyClass);
#endif
        }

        if (ResolvedNotifyClass && ResolvedNotifyClass->HasAnyClassFlags(CLASS_Abstract))
        {
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("Cannot create AnimNotify: '%s' is an abstract class. Use a concrete subclass like AnimNotify_PlaySound or create a custom AnimNotify blueprint."), *FullClassName),
                TEXT("ABSTRACT_CLASS_ERROR")
            );
        }

        if (!ResolvedNotifyClass)
        {
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("AnimNotify class '%s' not found. Use a concrete subclass like AnimNotify_PlaySound or a custom AnimNotify blueprint."), *NotifyClass),
                TEXT("CLASS_NOT_FOUND")
            );
        }
    }

    UAnimSequenceBase* AnimAsset = Cast<UAnimSequenceBase>(StaticLoadObject(UAnimSequenceBase::StaticClass(), nullptr, *AssetPath));
    if (!AnimAsset)
    {
        ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation asset: %s"), *AssetPath), TEXT("ASSET_NOT_FOUND"));
    }

    // Calculate time from frame
    float FrameRate = 30.0f;
#if ENGINE_MAJOR_VERSION >= 5
    if (UAnimSequence* Seq = Cast<UAnimSequence>(AnimAsset))
    {
        FrameRate = Seq->GetSamplingFrameRate().AsDecimal();
    }
#endif
    float TriggerTime = static_cast<float>(Frame) / FrameRate;

    FAnimNotifyEvent& NotifyEvent = AnimAsset->Notifies.AddDefaulted_GetRef();
    NotifyEvent.SetTime(TriggerTime);
    NotifyEvent.TrackIndex = TrackIndex;

    if (!NotifyName.IsEmpty())
    {
        NotifyEvent.NotifyName = FName(*NotifyName);
    }

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

        ANIM_SUCCESS_RESPONSE(TEXT("Notify added"));
        McpHandlerUtils::AddVerification(Response, AnimAsset);
        return Response;
}

if (SubAction == TEXT("add_notify_state"))
{
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
    FString NotifyClass = GetStringFieldAnimAuth(Params, TEXT("notifyClass"), TEXT(""));
    int32 StartFrame = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("startFrame"), 0));
    int32 EndFrame = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("endFrame"), 10));
    int32 TrackIndex = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("trackIndex"), 0));
    FString NotifyName = GetStringFieldAnimAuth(Params, TEXT("notifyName"), TEXT(""));
    bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

    if (EndFrame < StartFrame)
    {
        ANIM_ERROR_RESPONSE(TEXT("endFrame must be greater than or equal to startFrame"), TEXT("INVALID_FRAME_RANGE"));
    }

    if (NotifyClass.IsEmpty() && NotifyName.IsEmpty())
    {
        ANIM_ERROR_RESPONSE(TEXT("At least one of notifyClass or notifyName is required"), TEXT("MISSING_NOTIFY_PARAMS"));
    }

    // Resolve notify state class BEFORE modifying the asset
    UClass* ResolvedNotifyStateClass = nullptr;
    if (!NotifyClass.IsEmpty())
    {
        FString FullClassName = NotifyClass;
        if (!FullClassName.StartsWith(TEXT("AnimNotifyState_")))
        {
            FullClassName = TEXT("AnimNotifyState_") + NotifyClass;
        }

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        ResolvedNotifyStateClass = FindFirstObject<UClass>(*FullClassName, EFindFirstObjectOptions::None);
#else
        ResolvedNotifyStateClass = ResolveClassByName(FullClassName);
#endif
        if (!ResolvedNotifyStateClass)
        {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
            ResolvedNotifyStateClass = FindFirstObject<UClass>(*NotifyClass, EFindFirstObjectOptions::None);
#else
            ResolvedNotifyStateClass = ResolveClassByName(NotifyClass);
#endif
        }

        if (ResolvedNotifyStateClass && ResolvedNotifyStateClass->HasAnyClassFlags(CLASS_Abstract))
        {
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("Cannot create AnimNotifyState: '%s' is an abstract class. Use a concrete subclass like AnimNotifyState_PlayMontageNotify or create a custom AnimNotifyState blueprint."), *FullClassName),
                TEXT("ABSTRACT_CLASS_ERROR")
            );
        }

        if (!ResolvedNotifyStateClass)
        {
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("AnimNotifyState class '%s' not found. Use a concrete subclass like AnimNotifyState_PlayMontageNotify or a custom AnimNotifyState blueprint."), *NotifyClass),
                TEXT("CLASS_NOT_FOUND")
            );
        }
    }

    UAnimSequenceBase* AnimAsset = Cast<UAnimSequenceBase>(StaticLoadObject(UAnimSequenceBase::StaticClass(), nullptr, *AssetPath));
    if (!AnimAsset)
    {
        ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation asset: %s"), *AssetPath), TEXT("ASSET_NOT_FOUND"));
    }

    float FrameRate = 30.0f;
#if ENGINE_MAJOR_VERSION >= 5
    if (UAnimSequence* Seq = Cast<UAnimSequence>(AnimAsset))
    {
        FrameRate = Seq->GetSamplingFrameRate().AsDecimal();
    }
#endif
    float StartTime = static_cast<float>(StartFrame) / FrameRate;
    float EndTime = static_cast<float>(EndFrame) / FrameRate;
    float Duration = EndTime - StartTime;

    FAnimNotifyEvent& NotifyEvent = AnimAsset->Notifies.AddDefaulted_GetRef();
    NotifyEvent.SetTime(StartTime);
    NotifyEvent.SetDuration(Duration);
    NotifyEvent.TrackIndex = TrackIndex;

    if (!NotifyName.IsEmpty())
    {
        NotifyEvent.NotifyName = FName(*NotifyName);
    }

    if (ResolvedNotifyStateClass)
    {
        UAnimNotifyState* NewNotifyState = NewObject<UAnimNotifyState>(AnimAsset, ResolvedNotifyStateClass);
        if (!NewNotifyState)
        {
            AnimAsset->Notifies.Pop();
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("Failed to create AnimNotifyState instance of class '%s'"), *NotifyClass),
                TEXT("INSTANTIATION_FAILED")
            );
        }
        NotifyEvent.NotifyStateClass = NewNotifyState;
    }

        AnimAsset->RefreshCacheData();

        SaveAnimAsset(AnimAsset, bSave);

        ANIM_SUCCESS_RESPONSE(TEXT("Notify state added"));
        McpHandlerUtils::AddVerification(Response, AnimAsset);
        return Response;
    }
    return nullptr;
}

} // namespace McpAnimationAuthoring
#endif // WITH_EDITOR
