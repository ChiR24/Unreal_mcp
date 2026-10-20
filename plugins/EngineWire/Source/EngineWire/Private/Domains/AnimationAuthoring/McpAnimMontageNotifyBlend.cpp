#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AnimationAuthoring/McpAnimSupport.h"

namespace McpAnimationAuthoring {

namespace
{
// Sets only what the call sends: a blendTime-only call used to reset the curve
// to Linear, and a blendOption-only call reset the time to 0.25 s.
bool ApplyMcpMontageBlend(FAlphaBlend& Blend, const TSharedPtr<FJsonObject>& Params, FString& OutError)
{
    // A call naming neither saved the montage and answered "updated"; a negative time is no blend.
    if (!Params->HasField(TEXT("blendOption")) && !Params->HasField(TEXT("blendTime")))
    {
        OutError = TEXT("Nothing to set: send blendTime (seconds, 0 or more) or blendOption (Linear, Cubic or Sinusoidal).");
        return false;
    }
    if (GetJsonNumberField(Params, TEXT("blendTime"), 0.0) < 0.0)
    {
        OutError = TEXT("blendTime must be 0 or more seconds.");
        return false;
    }
    if (Params->HasField(TEXT("blendOption")))
    {
        const FString Option = GetJsonStringField(Params, TEXT("blendOption"), TEXT(""));
        if (Option.Equals(TEXT("Linear"), ESearchCase::IgnoreCase))
        {
            Blend.SetBlendOption(EAlphaBlendOption::Linear);
        }
        else if (Option.Equals(TEXT("Cubic"), ESearchCase::IgnoreCase))
        {
            Blend.SetBlendOption(EAlphaBlendOption::Cubic);
        }
        else if (Option.Equals(TEXT("Sinusoidal"), ESearchCase::IgnoreCase))
        {
            Blend.SetBlendOption(EAlphaBlendOption::Sinusoidal);
        }
        else
        {
            OutError = FString::Printf(TEXT("Unknown blendOption '%s'; use Linear, Cubic or Sinusoidal"), *Option);
            return false;
        }
    }
    if (Params->HasField(TEXT("blendTime")))
    {
        Blend.SetBlendTime(static_cast<float>(GetJsonNumberField(Params, TEXT("blendTime"), 0.0)));
    }
    return true;
}
}

TSharedPtr<FJsonObject> HandleMontageNotifyBlendActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
if (SubAction == TEXT("add_montage_notify"))
{
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
    FString NotifyClass = GetJsonStringField(Params, TEXT("notifyClass"), TEXT(""));
    float Time = static_cast<float>(GetJsonNumberField(Params, TEXT("time"), 0.0));
    int32 TrackIndex = static_cast<int32>(GetJsonNumberField(Params, TEXT("trackIndex"), 0));
    FString NotifyName = GetJsonStringField(Params, TEXT("notifyName"), TEXT(""));
    bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

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

#if ENGINE_MINOR_VERSION >= 1
        ResolvedNotifyClass = FindFirstObject<UClass>(*FullClassName, EFindFirstObjectOptions::None);
#else
        ResolvedNotifyClass = ResolveClassByName(FullClassName);
#endif
        if (!ResolvedNotifyClass)
        {
#if ENGINE_MINOR_VERSION >= 1
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
    // A notify past the end never fired, and one on a negative track sat on no track at all.
    if (Time < 0.0f || Time > Montage->GetPlayLength() + KINDA_SMALL_NUMBER || TrackIndex < 0)
    {
        ANIM_ERROR_RESPONSE(FString::Printf(TEXT("time must lie in 0..%.3f s (the montage's length) and trackIndex be 0 or more; nothing was added."),
            Montage->GetPlayLength()), TEXT("INVALID_ARGUMENT"));
    }

    {
        while (!Montage->AnimNotifyTracks.IsValidIndex(TrackIndex))
        {
            Montage->AnimNotifyTracks.Add(
                FAnimNotifyTrack(*FString::FromInt(Montage->AnimNotifyTracks.Num() + 1), FLinearColor::White)
            );
        }
    }

    FAnimNotifyEvent& NotifyEvent = Montage->Notifies.AddDefaulted_GetRef();
    NotifyEvent.Link(Montage, Time);
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
        SaveAnimAssetAndReport(Montage, bSave, Response);

        ANIM_SUCCESS_RESPONSE(TEXT("Montage notify added"));
        McpHandlerUtils::AddVerification(Response, Montage);
        return Response;
}

    if (SubAction == TEXT("set_blend_in"))
    {
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

        UAnimMontage* Montage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *AssetPath));
        if (!Montage)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load montage: %s"), *AssetPath), TEXT("MONTAGE_NOT_FOUND"));
        }

        FString BlendError;
        if (!ApplyMcpMontageBlend(Montage->BlendIn, Params, BlendError))
        {
            ANIM_ERROR_RESPONSE(BlendError, TEXT("INVALID_ARGUMENT"));
        }

        SaveAnimAssetAndReport(Montage, bSave, Response);

        ANIM_SUCCESS_RESPONSE(TEXT("Blend in settings updated"));
        McpHandlerUtils::AddVerification(Response, Montage);
        return Response;
    }

    if (SubAction == TEXT("set_blend_out"))
    {
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

        UAnimMontage* Montage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *AssetPath));
        if (!Montage)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load montage: %s"), *AssetPath), TEXT("MONTAGE_NOT_FOUND"));
        }

        FString BlendError;
        if (!ApplyMcpMontageBlend(Montage->BlendOut, Params, BlendError))
        {
            ANIM_ERROR_RESPONSE(BlendError, TEXT("INVALID_ARGUMENT"));
        }

        SaveAnimAssetAndReport(Montage, bSave, Response);

        ANIM_SUCCESS_RESPONSE(TEXT("Blend out settings updated"));
        McpHandlerUtils::AddVerification(Response, Montage);
        return Response;
    }

    if (SubAction == TEXT("link_sections"))
    {
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        FString FromSection = GetJsonStringField(Params, TEXT("fromSection"), TEXT(""));
        FString ToSection = GetJsonStringField(Params, TEXT("toSection"), TEXT(""));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

        if (FromSection.IsEmpty() || ToSection.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("fromSection and toSection are required"), TEXT("MISSING_SECTIONS"));
        }

        UAnimMontage* Montage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *AssetPath));
        if (!Montage)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load montage: %s"), *AssetPath), TEXT("MONTAGE_NOT_FOUND"));
        }

        // A section the montage lacks used to be skipped behind "Linked".
        const int32 FromSectionIndex = Montage->GetSectionIndex(FName(*FromSection));
        if (FromSectionIndex == INDEX_NONE || Montage->GetSectionIndex(FName(*ToSection)) == INDEX_NONE)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Section '%s' is not in this montage (sections: %s); nothing was linked."),
                *(FromSectionIndex == INDEX_NONE ? FromSection : ToSection), *McpMontageSectionNames(Montage)), TEXT("SECTION_NOT_FOUND"));
        }
        Montage->CompositeSections[FromSectionIndex].NextSectionName = FName(*ToSection);

        SaveAnimAssetAndReport(Montage, bSave, Response);

        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Linked '%s' to '%s'"), *FromSection, *ToSection));
        McpHandlerUtils::AddVerification(Response, Montage);
        return Response;
    }

    // ===== 10.3 Blend Spaces =====
    return nullptr;
}

} // namespace McpAnimationAuthoring
