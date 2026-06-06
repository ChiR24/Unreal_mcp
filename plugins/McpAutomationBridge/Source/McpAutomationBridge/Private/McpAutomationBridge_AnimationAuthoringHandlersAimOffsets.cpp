#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AnimationAuthoringSupport.h"

#if WITH_EDITOR
namespace McpAnimationAuthoring {

TSharedPtr<FJsonObject> HandleAimOffsetActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("create_aim_offset"))
    {
#if MCP_HAS_BLENDSPACE_FACTORY
    FString Name = GetStringFieldAnimAuth(Params, TEXT("name"), TEXT(""));
    FString Path = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("path"), TEXT("/Game/Animations")));
    FString SkeletonPath = GetStringFieldAnimAuth(Params, TEXT("skeletonPath"), TEXT(""));
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

        UBlendSpaceFactoryNew* Factory = NewObject<UBlendSpaceFactoryNew>();
        Factory->TargetSkeleton = Skeleton;
        UAimOffsetBlendSpace* NewAimOffset = Cast<UAimOffsetBlendSpace>(
            Factory->FactoryCreateNew(UAimOffsetBlendSpace::StaticClass(), Package,
                                      FName(*Name), RF_Public | RF_Standalone,
                                      nullptr, GWarn));
        if (!NewAimOffset)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create aim offset"), TEXT("CREATE_FAILED"));
        }

        // Configure default aim offset axes (Yaw and Pitch)
        // Note: In UE 5.7+, GetBlendParameter returns const ref
        // The factory should have set reasonable defaults, just trigger update
        NewAimOffset->PostEditChange();
        NewAimOffset->MarkPackageDirty();

        SaveAnimAsset(NewAimOffset, bSave);

        FString FullPath = Path / Name;
        Response->SetStringField(TEXT("assetPath"), FullPath);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Aim Offset '%s' created"), *Name));
#else
        ANIM_ERROR_RESPONSE(TEXT("Blend space factory not available"), TEXT("NOT_SUPPORTED"));
#endif
        return Response;
    }

    if (SubAction == TEXT("add_aim_offset_sample"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        FString AnimationPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("animationPath"), TEXT("")));
        float Yaw = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("yaw"), 0.0));
        float Pitch = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("pitch"), 0.0));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

        UAimOffsetBlendSpace* AimOffset = Cast<UAimOffsetBlendSpace>(StaticLoadObject(UAimOffsetBlendSpace::StaticClass(), nullptr, *AssetPath));
        if (!AimOffset)
        {
            // Try as regular blend space
            UBlendSpace* BlendSpace = Cast<UBlendSpace>(StaticLoadObject(UBlendSpace::StaticClass(), nullptr, *AssetPath));
            if (BlendSpace)
            {
                UAnimSequence* Animation = LoadAnimSequenceFromPath(AnimationPath);
                if (!Animation)
                {
                    ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation: %s"), *AnimationPath), TEXT("ANIMATION_NOT_FOUND"));
                }

                FVector SampleValue(Yaw, Pitch, 0.0f);
                BlendSpace->AddSample(Animation, SampleValue);

                SaveAnimAsset(BlendSpace, bSave);

                ANIM_SUCCESS_RESPONSE(TEXT("Aim offset sample added"));
                return Response;
            }

            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load aim offset: %s"), *AssetPath), TEXT("AIMOFFSET_NOT_FOUND"));
        }

        UAnimSequence* Animation = LoadAnimSequenceFromPath(AnimationPath);
        if (!Animation)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation: %s"), *AnimationPath), TEXT("ANIMATION_NOT_FOUND"));
        }

        // Add sample with yaw/pitch coordinates
        FVector SampleValue(Yaw, Pitch, 0.0f);
        AimOffset->AddSample(Animation, SampleValue);

        SaveAnimAsset(AimOffset, bSave);

        ANIM_SUCCESS_RESPONSE(TEXT("Aim offset sample added"));
        return Response;
    }

    // ===== 10.4 Animation Blueprints =====
    return nullptr;
}

} // namespace McpAnimationAuthoring
#endif // WITH_EDITOR
