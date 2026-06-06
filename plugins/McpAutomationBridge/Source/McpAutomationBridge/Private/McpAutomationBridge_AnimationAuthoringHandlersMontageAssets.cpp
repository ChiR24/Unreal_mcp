#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AnimationAuthoringSupport.h"

#if WITH_EDITOR
namespace McpAnimationAuthoring {

TSharedPtr<FJsonObject> HandleMontageAssetActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("create_montage"))
    {
    FString Name = GetStringFieldAnimAuth(Params, TEXT("name"), TEXT(""));
    FString Path = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("path"), TEXT("/Game/Animations")));
    FString SkeletonPath = GetStringFieldAnimAuth(Params, TEXT("skeletonPath"), TEXT(""));
    FString SlotName = GetStringFieldAnimAuth(Params, TEXT("slotName"), TEXT("DefaultSlot"));
    bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

    if (Name.IsEmpty())
    {
        ANIM_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
    }

    USkeleton* Skeleton = nullptr;
    if (!SkeletonPath.IsEmpty())
    {
        Skeleton = LoadSkeletonFromPathAnim(SkeletonPath);
        if (!Skeleton)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load skeleton: %s"), *SkeletonPath), TEXT("SKELETON_NOT_FOUND"));
        }
    }

    // Create package and asset directly to avoid UI dialogs
        FString PackagePath = Path / Name;
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
        }

        UAnimMontageFactory* Factory = NewObject<UAnimMontageFactory>();
        Factory->TargetSkeleton = Skeleton;
        UAnimMontage* NewMontage = Cast<UAnimMontage>(
            Factory->FactoryCreateNew(UAnimMontage::StaticClass(), Package,
                                      FName(*Name), RF_Public | RF_Standalone,
                                      nullptr, GWarn));
        if (!NewMontage)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create montage"), TEXT("CREATE_FAILED"));
        }

        // Add default slot
        if (!SlotName.IsEmpty())
        {
            FSlotAnimationTrack& SlotTrack = NewMontage->SlotAnimTracks.AddDefaulted_GetRef();
            SlotTrack.SlotName = FName(*SlotName);
        }

        SaveAnimAsset(NewMontage, bSave);

        FString FullPath = Path / Name;
        Response->SetStringField(TEXT("assetPath"), FullPath);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Montage '%s' created"), *Name));
        McpHandlerUtils::AddVerification(Response, NewMontage);
        return Response;
    }

    if (SubAction == TEXT("add_montage_section"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString SectionName = GetStringFieldAnimAuth(Params, TEXT("sectionName"), TEXT(""));
        float StartTime = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("startTime"), 0.0));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

        if (SectionName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("sectionName is required"), TEXT("MISSING_SECTION_NAME"));
        }

        UAnimMontage* Montage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *AssetPath));
        if (!Montage)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load montage: %s"), *AssetPath), TEXT("MONTAGE_NOT_FOUND"));
        }

        // Add new section
        int32 SectionIndex = Montage->AddAnimCompositeSection(FName(*SectionName), StartTime);

        SaveAnimAsset(Montage, bSave);

        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Section '%s' added at index %d"), *SectionName, SectionIndex));
        McpHandlerUtils::AddVerification(Response, Montage);
        return Response;
    }

    if (SubAction == TEXT("add_montage_slot"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString AnimationPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("animationPath"), TEXT("")));
        FString SlotName = GetStringFieldAnimAuth(Params, TEXT("slotName"), TEXT("DefaultSlot"));
        float StartTime = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("startTime"), 0.0));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

        UAnimMontage* Montage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *AssetPath));
        if (!Montage)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load montage: %s"), *AssetPath), TEXT("MONTAGE_NOT_FOUND"));
        }

        UAnimSequence* Animation = LoadAnimSequenceFromPath(AnimationPath);
        if (!Animation)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation: %s"), *AnimationPath), TEXT("ANIMATION_NOT_FOUND"));
        }

        // Find or create slot track
        FSlotAnimationTrack* SlotTrack = nullptr;
        for (FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
        {
            if (Track.SlotName == FName(*SlotName))
            {
                SlotTrack = &Track;
                break;
            }
        }

        if (!SlotTrack)
        {
            SlotTrack = &Montage->SlotAnimTracks.AddDefaulted_GetRef();
            SlotTrack->SlotName = FName(*SlotName);
        }

        // Add animation to slot track
        FAnimSegment& Segment = SlotTrack->AnimTrack.AnimSegments.AddDefaulted_GetRef();
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        Segment.SetAnimReference(Animation);
#else
        // UE 5.0: Direct member access
        Segment.AnimReference = Animation;
#endif
        Segment.StartPos = StartTime;
        Segment.AnimStartTime = 0.0f;
        Segment.AnimEndTime = Animation->GetPlayLength();
        Segment.AnimPlayRate = 1.0f;
        Segment.LoopingCount = 1;

        SaveAnimAsset(Montage, bSave);

        ANIM_SUCCESS_RESPONSE(TEXT("Animation added to montage slot"));
        McpHandlerUtils::AddVerification(Response, Montage);
        return Response;
    }

    if (SubAction == TEXT("set_section_timing"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString SectionName = GetStringFieldAnimAuth(Params, TEXT("sectionName"), TEXT(""));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

        if (SectionName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("sectionName is required"), TEXT("MISSING_SECTION_NAME"));
        }

        UAnimMontage* Montage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *AssetPath));
        if (!Montage)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load montage: %s"), *AssetPath), TEXT("MONTAGE_NOT_FOUND"));
        }

        int32 SectionIndex = Montage->GetSectionIndex(FName(*SectionName));
        if (SectionIndex == INDEX_NONE)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Section not found: %s"), *SectionName), TEXT("SECTION_NOT_FOUND"));
        }

        // Update section timing if startTime is provided
        if (Params->HasField(TEXT("startTime")))
        {
            float StartTime = static_cast<float>(GetJsonNumberField(Params, TEXT("startTime")));
            FCompositeSection& Section = Montage->CompositeSections[SectionIndex];
            Section.SetTime(StartTime);
        }

        SaveAnimAsset(Montage, bSave);

        ANIM_SUCCESS_RESPONSE(TEXT("Section timing updated"));
        McpHandlerUtils::AddVerification(Response, Montage);
        return Response;
    }
    return nullptr;
}

} // namespace McpAnimationAuthoring
#endif // WITH_EDITOR
