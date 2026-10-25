#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AnimationAuthoring/McpAnimSupport.h"

namespace McpAnimationAuthoring {

TSharedPtr<FJsonObject> HandleAimOffsetActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("create_aim_offset"))
    {
    FString Name = GetJsonStringField(Params, TEXT("name"), TEXT(""));
    FString Path = NormalizeAnimPath(GetJsonStringField(Params, TEXT("path"), TEXT("/Game/Animations")));
    FString SkeletonPath = GetJsonStringField(Params, TEXT("skeletonPath"), TEXT(""));
    bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

    if (Name.IsEmpty())
    {
        ANIM_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
    }

    USkeleton* Skeleton = nullptr;
    // The engine's factory makes nothing without a skeleton, so the call failed late with an empty package left behind.
    if (SkeletonPath.IsEmpty())
    {
        ANIM_ERROR_RESPONSE(TEXT("skeletonPath is required: an aim offset is made for one skeleton"), TEXT("MISSING_SKELETON"));
    }
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
        FString CreateError, CreateCode;
        UPackage* Package = McpCreatePackageForNewAsset(PackagePath, CreateError, CreateCode);
        if (!Package)
        {
            ANIM_ERROR_RESPONSE(CreateError, CreateCode);
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

        // The engine's factory leaves both axes at None, 0 to 100 (an aim offset made in the editor too), so samples
        // added at a yaw and pitch fell outside them. Yaw -180 to 180 and Pitch -90 to 90, as aim offsets use.
        if (!McpSetBlendSpaceAxis(NewAimOffset, 0, TEXT("Yaw"), -180.f, 180.f) ||
            !McpSetBlendSpaceAxis(NewAimOffset, 1, TEXT("Pitch"), -90.f, 90.f))
        {
            ANIM_ERROR_RESPONSE(TEXT("The aim offset was created but its axes could not be set"), TEXT("AXIS_SETUP_FAILED"));
        }
        NewAimOffset->PostEditChange();
        NewAimOffset->MarkPackageDirty();
        // Read back after PostEditChange, so the reply shows the ranges the asset kept.
        Response->SetArrayField(TEXT("axes"), McpBlendSpaceAxesJson(NewAimOffset, 2));

        SaveAnimAssetAndReport(NewAimOffset, bSave, Response);

        FString FullPath = Path / Name;
        Response->SetStringField(TEXT("assetPath"), FullPath);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Aim Offset '%s' created"), *Name));
        return Response;
    }

    if (SubAction == TEXT("add_aim_offset_sample"))
    {
        FString AssetPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")));
        FString AnimationPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("animationPath"), TEXT("")));
        float Yaw = static_cast<float>(GetJsonNumberField(Params, TEXT("yaw"), 0.0));
        float Pitch = static_cast<float>(GetJsonNumberField(Params, TEXT("pitch"), 0.0));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

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

    return nullptr;
}

} // namespace McpAnimationAuthoring
