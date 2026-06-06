#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AnimationAuthoringSupport.h"

#if WITH_EDITOR
namespace McpAnimationAuthoring {

TSharedPtr<FJsonObject> HandleBlendSpaceAssetActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("create_blend_space_1d"))
    {
#if MCP_HAS_BLENDSPACE_FACTORY
    FString Name = GetStringFieldAnimAuth(Params, TEXT("name"), TEXT(""));
    FString Path = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("path"), TEXT("/Game/Animations")));
    FString SkeletonPath = GetStringFieldAnimAuth(Params, TEXT("skeletonPath"), TEXT(""));
    FString AxisName = GetStringFieldAnimAuth(Params, TEXT("axisName"), TEXT("Speed"));
    float AxisMin = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("axisMin"), 0.0));
    float AxisMax = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("axisMax"), 600.0));
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

    // Check if an asset already exists at the target path to prevent modal dialog
        FString ObjectPath = FString::Printf(TEXT("%s/%s"), *Path, *Name);
        if (UEditorAssetLibrary::DoesAssetExist(ObjectPath))
        {
            UObject* ExistingAsset = UEditorAssetLibrary::LoadAsset(ObjectPath);
            if (ExistingAsset)
            {
                if (Cast<UBlendSpace1D>(ExistingAsset))
                {
                    // Same type - return success with existing asset info
                    Response->SetStringField(TEXT("assetPath"), ObjectPath);
                    Response->SetBoolField(TEXT("existingAsset"), true);
                    ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Blend Space 1D '%s' already exists - reusing existing asset"), *Name));
                    return Response;
                }
                else
                {
                    // Different type - return error to prevent modal dialog
                    FString ExistingClassName = ExistingAsset->GetClass()->GetName();
                    ANIM_ERROR_RESPONSE(
                        FString::Printf(TEXT("Cannot create BlendSpace1D: asset '%s' already exists as type '%s'"),
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

        UBlendSpaceFactory1D* Factory = NewObject<UBlendSpaceFactory1D>();
        Factory->TargetSkeleton = Skeleton;
        UBlendSpace1D* NewBlendSpace = Cast<UBlendSpace1D>(
            Factory->FactoryCreateNew(UBlendSpace1D::StaticClass(), Package,
                                      FName(*Name), RF_Public | RF_Standalone,
                                      nullptr, GWarn));
        if (!NewBlendSpace)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create blend space 1D"), TEXT("CREATE_FAILED"));
        }

        // Configure axis - use reflection since BlendParameters is protected
        FBlendParameter NewParam;
        NewParam.DisplayName = AxisName;
        NewParam.Min = AxisMin;
        NewParam.Max = AxisMax;
        NewParam.GridNum = 4;
        NewParam.bSnapToGrid = false;
        NewParam.bWrapInput = false;

        // Use FProperty reflection to set the protected BlendParameters array
        if (FProperty* BlendParamsProp = UBlendSpace::StaticClass()->FindPropertyByName(TEXT("BlendParameters")))
        {
            NewBlendSpace->Modify();
            FBlendParameter* BlendParamsPtr = BlendParamsProp->ContainerPtrToValuePtr<FBlendParameter>(NewBlendSpace);
            if (BlendParamsPtr)
            {
                BlendParamsPtr[0] = NewParam;
            }
        }

        NewBlendSpace->PostEditChange();

        SaveAnimAsset(NewBlendSpace, bSave);

        FString FullPath = Path / Name;
        Response->SetStringField(TEXT("assetPath"), FullPath);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Blend Space 1D '%s' created"), *Name));
        McpHandlerUtils::AddVerification(Response, NewBlendSpace);
#else
        ANIM_ERROR_RESPONSE(TEXT("Blend space factory not available"), TEXT("NOT_SUPPORTED"));
#endif
        return Response;
    }

    if (SubAction == TEXT("create_blend_space_2d"))
    {
#if MCP_HAS_BLENDSPACE_FACTORY
    FString Name = GetStringFieldAnimAuth(Params, TEXT("name"), TEXT(""));
    FString Path = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("path"), TEXT("/Game/Animations")));
    FString SkeletonPath = GetStringFieldAnimAuth(Params, TEXT("skeletonPath"), TEXT(""));
    FString HorizontalAxisName = GetStringFieldAnimAuth(Params, TEXT("horizontalAxisName"), TEXT("Direction"));
    float HorizontalMin = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("horizontalMin"), -180.0));
    float HorizontalMax = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("horizontalMax"), 180.0));
    FString VerticalAxisName = GetStringFieldAnimAuth(Params, TEXT("verticalAxisName"), TEXT("Speed"));
    float VerticalMin = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("verticalMin"), 0.0));
    float VerticalMax = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("verticalMax"), 600.0));
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

    // Check if an asset already exists at the target path to prevent modal dialog
        FString ObjectPath = FString::Printf(TEXT("%s/%s"), *Path, *Name);
        if (UEditorAssetLibrary::DoesAssetExist(ObjectPath))
        {
            UObject* ExistingAsset = UEditorAssetLibrary::LoadAsset(ObjectPath);
            if (ExistingAsset)
            {
                if (Cast<UBlendSpace>(ExistingAsset))
                {
                    // Same type - return success with existing asset info
                    Response->SetStringField(TEXT("assetPath"), ObjectPath);
                    Response->SetBoolField(TEXT("existingAsset"), true);
                    ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Blend Space 2D '%s' already exists - reusing existing asset"), *Name));
                    return Response;
                }
                else
                {
                    // Different type - return error to prevent modal dialog
                    FString ExistingClassName = ExistingAsset->GetClass()->GetName();
                    ANIM_ERROR_RESPONSE(
                        FString::Printf(TEXT("Cannot create BlendSpace: asset '%s' already exists as type '%s'"),
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

        UBlendSpaceFactoryNew* Factory = NewObject<UBlendSpaceFactoryNew>();
        Factory->TargetSkeleton = Skeleton;
        UBlendSpace* NewBlendSpace = Cast<UBlendSpace>(
            Factory->FactoryCreateNew(UBlendSpace::StaticClass(), Package,
                                      FName(*Name), RF_Public | RF_Standalone,
                                      nullptr, GWarn));
        if (!NewBlendSpace)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create blend space 2D"), TEXT("CREATE_FAILED"));
        }

        // Configure axes using reflection since BlendParameters is protected in UE 5.7+
        FBlendParameter HParam;
        HParam.DisplayName = HorizontalAxisName;
        HParam.Min = HorizontalMin;
        HParam.Max = HorizontalMax;
        HParam.GridNum = 4;
        HParam.bSnapToGrid = false;
        HParam.bWrapInput = false;

        FBlendParameter VParam;
        VParam.DisplayName = VerticalAxisName;
        VParam.Min = VerticalMin;
        VParam.Max = VerticalMax;
        VParam.GridNum = 4;
        VParam.bSnapToGrid = false;
        VParam.bWrapInput = false;

        // Use FProperty reflection to set the protected BlendParameters array
        if (FProperty* BlendParamsProp = UBlendSpace::StaticClass()->FindPropertyByName(TEXT("BlendParameters")))
        {
            NewBlendSpace->Modify();
            FBlendParameter* BlendParamsPtr = BlendParamsProp->ContainerPtrToValuePtr<FBlendParameter>(NewBlendSpace);
            if (BlendParamsPtr)
            {
                BlendParamsPtr[0] = HParam;
                BlendParamsPtr[1] = VParam;
            }
        }

        NewBlendSpace->PostEditChange();

        SaveAnimAsset(NewBlendSpace, bSave);

        FString FullPath = Path / Name;
        Response->SetStringField(TEXT("assetPath"), FullPath);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Blend Space 2D '%s' created"), *Name));
#else
        ANIM_ERROR_RESPONSE(TEXT("Blend space factory not available"), TEXT("NOT_SUPPORTED"));
#endif
        return Response;
    }
    return nullptr;
}

} // namespace McpAnimationAuthoring
#endif // WITH_EDITOR
