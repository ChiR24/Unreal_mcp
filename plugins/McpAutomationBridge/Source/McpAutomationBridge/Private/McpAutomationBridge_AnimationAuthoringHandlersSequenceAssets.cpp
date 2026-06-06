#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AnimationAuthoringSupport.h"

#if WITH_EDITOR
namespace McpAnimationAuthoring {

TSharedPtr<FJsonObject> HandleSequenceAssetActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("create_animation_sequence"))
    {
    FString Name = GetStringFieldAnimAuth(Params, TEXT("name"), TEXT(""));
    FString Path = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("path"), TEXT("/Game/Animations")));
    FString SkeletonPath = GetStringFieldAnimAuth(Params, TEXT("skeletonPath"), TEXT(""));
    int32 NumFrames = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("numFrames"), 30));
    int32 FrameRate = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("frameRate"), 30));
    bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

    if (FrameRate <= 0)
    {
        ANIM_ERROR_RESPONSE(TEXT("frameRate must be greater than 0"), TEXT("INVALID_FRAME_RATE"));
    }

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

    // Check if an asset already exists at the target path to prevent assertion failure
        FString ObjectPath = FString::Printf(TEXT("%s/%s"), *Path, *Name);
        if (UEditorAssetLibrary::DoesAssetExist(ObjectPath))
        {
            UObject* ExistingAsset = UEditorAssetLibrary::LoadAsset(ObjectPath);
            if (ExistingAsset)
            {
                if (Cast<UAnimSequence>(ExistingAsset))
                {
                    // Same type - return success with existing asset info
                    Response->SetStringField(TEXT("assetPath"), ObjectPath);
                    Response->SetBoolField(TEXT("existingAsset"), true);
                    ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Animation sequence '%s' already exists - reusing existing asset"), *Name));
                    McpHandlerUtils::AddVerification(Response, ExistingAsset);
                    return Response;
                }
                else
                {
                    // Different type - return error to prevent crash
                    FString ExistingClassName = ExistingAsset->GetClass()->GetName();
                    ANIM_ERROR_RESPONSE(
                        FString::Printf(TEXT("Cannot create animation sequence: asset '%s' already exists as type '%s'"),
                            *ObjectPath, *ExistingClassName),
                        TEXT("ASSET_TYPE_MISMATCH")
                    );
                }
            }
        }

        // Create package and asset directly to avoid UI dialogs
        FString PackagePath = Path / Name;
        UPackage* Package = CreatePackage(*PackagePath);
        if (!Package)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
        }

        UAnimSequenceFactory* Factory = NewObject<UAnimSequenceFactory>();
        Factory->TargetSkeleton = Skeleton;
        UAnimSequence* NewSequence = Cast<UAnimSequence>(
            Factory->FactoryCreateNew(UAnimSequence::StaticClass(), Package,
                                      FName(*Name), RF_Public | RF_Standalone,
                                      nullptr, GWarn));
        if (!NewSequence)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create animation sequence"), TEXT("CREATE_FAILED"));
        }

        // Set sequence length
        float Duration = static_cast<float>(NumFrames) / static_cast<float>(FrameRate);

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        // UE 5.1+: Use SetNumberOfFrames with FFrameNumber
        NewSequence->GetController().SetFrameRate(FFrameRate(FrameRate, 1));
        NewSequence->GetController().SetNumberOfFrames(FFrameNumber(NumFrames));
#else
        // SequenceLength is deprecated in UE 5.1+ but needed for UE 5.0 compatibility
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        NewSequence->SequenceLength = Duration;
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif

        SaveAnimAsset(NewSequence, bSave);

        FString FullPath = Path / Name;
        Response->SetStringField(TEXT("assetPath"), FullPath);
        Response->SetBoolField(TEXT("existingAsset"), false);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Animation sequence '%s' created"), *Name));
        McpHandlerUtils::AddVerification(Response, NewSequence);
        return Response;
    }

    if (SubAction == TEXT("set_sequence_length"))
    {
        FString AssetPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("assetPath"), TEXT("")));
        int32 NumFrames = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("numFrames"), 30));
        int32 FrameRate = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("frameRate"), 30));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

        UAnimSequence* Sequence = LoadAnimSequenceFromPath(AssetPath);
        if (!Sequence)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation sequence: %s"), *AssetPath), TEXT("SEQUENCE_NOT_FOUND"));
        }

        float Duration = static_cast<float>(NumFrames) / static_cast<float>(FrameRate);

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        // UE 5.1+: Use SetNumberOfFrames with FFrameNumber
        Sequence->GetController().SetFrameRate(FFrameRate(FrameRate, 1));
        Sequence->GetController().SetNumberOfFrames(FFrameNumber(NumFrames));
        if (Params->HasField(TEXT("frameRate")))
        {
            // Frame rate already set above
        }
#else
        // SequenceLength is deprecated in UE 5.1+ but needed for UE 5.0 compatibility
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        Sequence->SequenceLength = Duration;
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif

        SaveAnimAsset(Sequence, bSave);

        ANIM_SUCCESS_RESPONSE(TEXT("Sequence length updated"));
        McpHandlerUtils::AddVerification(Response, Sequence);
        return Response;
    }
    return nullptr;
}

} // namespace McpAnimationAuthoring
#endif // WITH_EDITOR
