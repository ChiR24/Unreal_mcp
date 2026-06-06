#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AnimationAuthoringSupport.h"

#if WITH_EDITOR
namespace McpAnimationAuthoring {

TSharedPtr<FJsonObject> HandleMontageNotifyBlendActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
if (SubAction == TEXT("add_montage_notify"))
{
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
    FString NotifyClass = GetStringFieldAnimAuth(Params, TEXT("notifyClass"), TEXT(""));
    float Time = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("time"), 0.0));
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

    UAnimMontage* Montage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *AssetPath));
    if (!Montage)
    {
        ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load montage: %s"), *AssetPath), TEXT("MONTAGE_NOT_FOUND"));
    }

#if WITH_EDITOR
    if (TrackIndex >= 0)
    {
        while (!Montage->AnimNotifyTracks.IsValidIndex(TrackIndex))
        {
            Montage->AnimNotifyTracks.Add(
                FAnimNotifyTrack(*FString::FromInt(Montage->AnimNotifyTracks.Num() + 1), FLinearColor::White)
            );
        }
    }
#endif

    FAnimNotifyEvent& NotifyEvent = Montage->Notifies.AddDefaulted_GetRef();
    NotifyEvent.SetTime(Time);
    NotifyEvent.TrackIndex = TrackIndex;

    if (!NotifyName.IsEmpty())
    {
        NotifyEvent.NotifyName = FName(*NotifyName);
    }

    if (ResolvedNotifyClass)
    {
        UAnimNotify* NewNotify = NewObject<UAnimNotify>(Montage, ResolvedNotifyClass);
        if (!NewNotify)
        {
            Montage->Notifies.Pop();
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("Failed to create AnimNotify instance of class '%s'"), *NotifyClass),
                TEXT("INSTANTIATION_FAILED")
            );
        }
        NotifyEvent.Notify = NewNotify;
    }

        Montage->RefreshCacheData();
        SaveAnimAsset(Montage, bSave);

        ANIM_SUCCESS_RESPONSE(TEXT("Montage notify added"));
        McpHandlerUtils::AddVerification(Response, Montage);
        return Response;
}

    if (SubAction == TEXT("set_blend_in"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        float BlendTime = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("blendTime"), 0.25));
        FString BlendOption = GetStringFieldAnimAuth(Params, TEXT("blendOption"), TEXT("Linear"));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

        UAnimMontage* Montage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *AssetPath));
        if (!Montage)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load montage: %s"), *AssetPath), TEXT("MONTAGE_NOT_FOUND"));
        }

        Montage->BlendIn.SetBlendTime(BlendTime);

        // Set blend option
        if (BlendOption == TEXT("Cubic"))
        {
            Montage->BlendIn.SetBlendOption(EAlphaBlendOption::Cubic);
        }
        else if (BlendOption == TEXT("Sinusoidal"))
        {
            Montage->BlendIn.SetBlendOption(EAlphaBlendOption::Sinusoidal);
        }
        else
        {
            Montage->BlendIn.SetBlendOption(EAlphaBlendOption::Linear);
        }

        SaveAnimAsset(Montage, bSave);

        ANIM_SUCCESS_RESPONSE(TEXT("Blend in settings updated"));
        McpHandlerUtils::AddVerification(Response, Montage);
        return Response;
    }

    if (SubAction == TEXT("set_blend_out"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        float BlendTime = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("blendTime"), 0.25));
        FString BlendOption = GetStringFieldAnimAuth(Params, TEXT("blendOption"), TEXT("Linear"));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

        UAnimMontage* Montage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *AssetPath));
        if (!Montage)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load montage: %s"), *AssetPath), TEXT("MONTAGE_NOT_FOUND"));
        }

        Montage->BlendOut.SetBlendTime(BlendTime);

        // Set blend option
        if (BlendOption == TEXT("Cubic"))
        {
            Montage->BlendOut.SetBlendOption(EAlphaBlendOption::Cubic);
        }
        else if (BlendOption == TEXT("Sinusoidal"))
        {
            Montage->BlendOut.SetBlendOption(EAlphaBlendOption::Sinusoidal);
        }
        else
        {
            Montage->BlendOut.SetBlendOption(EAlphaBlendOption::Linear);
        }

        SaveAnimAsset(Montage, bSave);

        ANIM_SUCCESS_RESPONSE(TEXT("Blend out settings updated"));
        McpHandlerUtils::AddVerification(Response, Montage);
        return Response;
    }

    if (SubAction == TEXT("link_sections"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString FromSection = GetStringFieldAnimAuth(Params, TEXT("fromSection"), TEXT(""));
        FString ToSection = GetStringFieldAnimAuth(Params, TEXT("toSection"), TEXT(""));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

        if (FromSection.IsEmpty() || ToSection.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("fromSection and toSection are required"), TEXT("MISSING_SECTIONS"));
        }

        UAnimMontage* Montage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *AssetPath));
        if (!Montage)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load montage: %s"), *AssetPath), TEXT("MONTAGE_NOT_FOUND"));
        }

        // Set next section using section index-based API
        int32 FromSectionIndex = Montage->GetSectionIndex(FName(*FromSection));
        int32 ToSectionIndex = Montage->GetSectionIndex(FName(*ToSection));
        if (FromSectionIndex != INDEX_NONE && ToSectionIndex != INDEX_NONE)
        {
            Montage->CompositeSections[FromSectionIndex].NextSectionName = FName(*ToSection);
        }

        SaveAnimAsset(Montage, bSave);

        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Linked '%s' to '%s'"), *FromSection, *ToSection));
        McpHandlerUtils::AddVerification(Response, Montage);
        return Response;
    }

    // ===== 10.3 Blend Spaces =====
    return nullptr;
}

} // namespace McpAnimationAuthoring
#endif // WITH_EDITOR
