#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Domains/AnimationAuthoring/McpAnimSupport.h"

namespace McpAnimationAuthoring {

namespace
{
// One axis of a blend space being created: the fields the action reads it from and what each defaults to.
struct FAxisSpec
{
    const TCHAR* NameField;
    const TCHAR* MinField;
    const TCHAR* MaxField;
    const TCHAR* DefaultName;
    double DefaultMin;
    double DefaultMax;
};

// create_blend_space_1d (TAsset UBlendSpace1D, one axis) and create_blend_space_2d (UBlendSpace, two): an asset of the
// kind already at the path is kept and reported, another kind there is refused, else the factory makes one for the
// skeleton and the axes are set.
template <typename TAsset, typename TFactory>
TSharedPtr<FJsonObject> CreateBlendSpace(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response,
                                         TArrayView<const FAxisSpec> Axes, bool bAddVerification)
{
    FString Name = GetJsonStringField(Params, TEXT("name"), TEXT(""));
    FString Path = NormalizeAnimPath(GetJsonStringField(Params, TEXT("path"), TEXT("/Game/Animations")));
    FString SkeletonPath = GetJsonStringField(Params, TEXT("skeletonPath"), TEXT(""));
    bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
    const FString Kind = FString::Printf(TEXT("%dD"), Axes.Num());

    if (Name.IsEmpty())
    {
        ANIM_ERROR_RESPONSE(TEXT("Name is required"), TEXT("MISSING_NAME"));
    }

    // The engine's factory makes nothing without a skeleton, so the call failed late with an empty package left behind.
    if (SkeletonPath.IsEmpty())
    {
        ANIM_ERROR_RESPONSE(TEXT("skeletonPath is required: a blend space is made for one skeleton"), TEXT("MISSING_SKELETON"));
    }
    USkeleton* Skeleton = LoadSkeletonFromPathAnim(SkeletonPath);
    if (!Skeleton)
    {
        ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load skeleton: %s"), *SkeletonPath), TEXT("SKELETON_NOT_FOUND"));
    }

    // Check if an asset already exists at the target path to prevent modal dialog
    FString ObjectPath = FString::Printf(TEXT("%s/%s"), *Path, *Name);
    if (McpAssetExists(ObjectPath))
    {
        UObject* ExistingAsset = McpLoadAsset(ObjectPath);
        if (ExistingAsset)
        {
            if (Cast<TAsset>(ExistingAsset))
            {
                // Same type - return success with existing asset info
                Response->SetStringField(TEXT("assetPath"), ObjectPath);
                Response->SetBoolField(TEXT("existingAsset"), true);
                // The axes asked for were dropped without a word: the reply now shows the ones the asset keeps.
                Response->SetArrayField(TEXT("axes"), McpBlendSpaceAxesJson(Cast<UBlendSpace>(ExistingAsset), Axes.Num()));
                ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Blend Space %s '%s' already exists and was kept as it is: its axes were not changed (set_axis_settings changes them)"), *Kind, *Name));
                return Response;
            }
            // Different type - return error to prevent modal dialog
            ANIM_ERROR_RESPONSE(
                FString::Printf(TEXT("Cannot create %s: asset '%s' already exists as type '%s'"),
                    *TAsset::StaticClass()->GetName(), *ObjectPath, *ExistingAsset->GetClass()->GetName()),
                TEXT("ASSET_TYPE_MISMATCH")
            );
        }
    }

    // Create package and asset directly to avoid UI dialogs
    FString PackagePath = Path / Name;
    UPackage* Package = McpCreatePackageOrNull(PackagePath);
    if (!Package)
    {
        ANIM_ERROR_RESPONSE(TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
    }

    TFactory* Factory = NewObject<TFactory>();
    Factory->TargetSkeleton = Skeleton;
    TAsset* NewBlendSpace = Cast<TAsset>(
        Factory->FactoryCreateNew(TAsset::StaticClass(), Package,
                                  FName(*Name), RF_Public | RF_Standalone,
                                  nullptr, GWarn));
    if (!NewBlendSpace)
    {
        ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Failed to create blend space %s"), *Kind), TEXT("CREATE_FAILED"));
    }

    // BlendParameters is protected, so the axes are set through reflection (McpSetBlendSpaceAxis); a factory-new
    // asset's axes neither snap to the grid nor wrap, which is what this used to write.
    for (int32 Index = 0; Index < Axes.Num(); ++Index)
    {
        const FAxisSpec& Axis = Axes[Index];
        McpSetBlendSpaceAxis(NewBlendSpace, Index, GetJsonStringField(Params, Axis.NameField, Axis.DefaultName),
            static_cast<float>(GetJsonNumberField(Params, Axis.MinField, Axis.DefaultMin)),
            static_cast<float>(GetJsonNumberField(Params, Axis.MaxField, Axis.DefaultMax)));
    }

    NewBlendSpace->PostEditChange();

    SaveAnimAssetAndReport(NewBlendSpace, bSave, Response);

    Response->SetStringField(TEXT("assetPath"), PackagePath);
    ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Blend Space %s '%s' created"), *Kind, *Name));
    if (bAddVerification)
    {
        McpHandlerUtils::AddVerification(Response, NewBlendSpace);
    }
    return Response;
}
}

TSharedPtr<FJsonObject> HandleBlendSpaceAssetActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("create_blend_space_1d"))
    {
        static const FAxisSpec Axes[] = {{TEXT("axisName"), TEXT("axisMin"), TEXT("axisMax"), TEXT("Speed"), 0.0, 600.0}};
        return CreateBlendSpace<UBlendSpace1D, UBlendSpaceFactory1D>(Params, Response, MakeArrayView(Axes), /*bAddVerification=*/true);
    }

    // create_blend_space is the 2D blend space (create_blend_space_1d is the single-axis one).
    if (SubAction == TEXT("create_blend_space_2d") || SubAction == TEXT("create_blend_space"))
    {
        static const FAxisSpec Axes[] = {
            {TEXT("horizontalAxisName"), TEXT("horizontalMin"), TEXT("horizontalMax"), TEXT("Direction"), -180.0, 180.0},
            {TEXT("verticalAxisName"), TEXT("verticalMin"), TEXT("verticalMax"), TEXT("Speed"), 0.0, 600.0}};
        return CreateBlendSpace<UBlendSpace, UBlendSpaceFactoryNew>(Params, Response, MakeArrayView(Axes), /*bAddVerification=*/false);
    }
    return nullptr;
}

} // namespace McpAnimationAuthoring
