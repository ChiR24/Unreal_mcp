#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AnimationAuthoring/McpAutomationBridge_AnimationAuthoringSupport.h"
#include "Domains/Animation/Assets/McpAutomationBridge_AnimationHandlersProceduralTracks.h"

namespace McpAnimationAuthoring {

TSharedPtr<FJsonObject> HandleMontageAssetActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("create_montage"))
    {
    FString Name = GetJsonStringField(Params, TEXT("name"), TEXT(""));
    FString Path = NormalizeAnimPath(GetJsonStringField(Params, TEXT("path"), TEXT("/Game/Animations")));
    FString SkeletonPath = GetJsonStringField(Params, TEXT("skeletonPath"), TEXT(""));
    FString SlotName = GetJsonStringField(Params, TEXT("slotName"), TEXT("DefaultSlot"));
    bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

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

    // The montage is built from animationPath when one is given: its skeleton
    // is the montage's, and it becomes the first segment of the slot.
    const FString AnimationPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("animationPath"), TEXT("")));
    UAnimSequence* SourceAnimation = nullptr;
    if (!AnimationPath.IsEmpty())
    {
        SourceAnimation = LoadAnimSequenceFromPath(AnimationPath);
        if (!SourceAnimation)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation: %s"), *AnimationPath), TEXT("ANIMATION_NOT_FOUND"));
        }
        // The factory asserts on a mismatch, which would take the editor down.
        if (Skeleton && SourceAnimation->GetSkeleton() != Skeleton)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("animationPath uses skeleton %s, not %s"),
                *GetPathNameSafe(SourceAnimation->GetSkeleton()), *SkeletonPath), TEXT("SKELETON_MISMATCH"));
        }
        Skeleton = SourceAnimation->GetSkeleton();
    }
    if (!Skeleton)
    {
        ANIM_ERROR_RESPONSE(TEXT("skeletonPath or animationPath is required"), TEXT("MISSING_SKELETON"));
    }

    // Same reuse rule as the sequence and procedural creators: re-running with
    // the same name used to create a second asset over the first.
    UAnimMontageFactory* Factory = NewObject<UAnimMontageFactory>();
    Factory->TargetSkeleton = Skeleton;
    Factory->SourceAnimation = SourceAnimation;
    bool bExisting = false;
    FString Code;
    FString Error;
    UAnimMontage* NewMontage = Cast<UAnimMontage>(McpAnimationHandlers::CreateOrReuseAnimAsset(
        UAnimMontage::StaticClass(), Factory, Path, Name, bExisting, Code, Error));
    if (!NewMontage)
    {
        ANIM_ERROR_RESPONSE(Error, Code);
    }
    Response->SetStringField(TEXT("assetPath"), Path / Name);
    Response->SetBoolField(TEXT("existingAsset"), bExisting);
    if (bExisting)
    {
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Montage '%s' already exists - reusing existing asset"), *Name));
        McpHandlerUtils::AddVerification(Response, NewMontage);
        return Response;
    }

    // The factory puts a SourceAnimation into DefaultSlot; name that track, or
    // add one, as slotName.
    if (!SlotName.IsEmpty())
    {
        if (NewMontage->SlotAnimTracks.Num() == 0)
        {
            NewMontage->SlotAnimTracks.AddDefaulted();
        }
        NewMontage->SlotAnimTracks[0].SlotName = FName(*SlotName);
    }

    SaveAnimAsset(NewMontage, bSave);

    Response->SetStringField(TEXT("slotName"), SlotName);
    ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Montage '%s' created"), *Name));
    McpHandlerUtils::AddVerification(Response, NewMontage);
    return Response;
    }

    if (SubAction == TEXT("add_montage_section"))
    {
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        FString SectionName = GetJsonStringField(Params, TEXT("sectionName"), TEXT(""));
        float StartTime = static_cast<float>(GetJsonNumberField(Params, TEXT("startTime"), 0.0));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

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
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        FString AnimationPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("animationPath"), TEXT("")));
        FString SlotName = GetJsonStringField(Params, TEXT("slotName"), TEXT("DefaultSlot"));
        float StartTime = static_cast<float>(GetJsonNumberField(Params, TEXT("startTime"), 0.0));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

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
#if ENGINE_MINOR_VERSION >= 1
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
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        FString SectionName = GetJsonStringField(Params, TEXT("sectionName"), TEXT(""));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

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
