#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AnimationAuthoringSupport.h"

#if WITH_EDITOR
namespace McpAnimationAuthoring {

TSharedPtr<FJsonObject> HandleBlueprintAssetActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("create_anim_blueprint"))
    {
    FString Name = GetStringFieldAnimAuth(Params, TEXT("name"), TEXT(""));
    FString Path = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("path"), TEXT("/Game/Blueprints")));
    FString SkeletonPath = GetStringFieldAnimAuth(Params, TEXT("skeletonPath"), TEXT(""));
    FString ParentClass = GetStringFieldAnimAuth(Params, TEXT("parentClass"), TEXT("AnimInstance"));
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

    // Check if an asset already exists at the target path to prevent assertion failure in Kismet2.cpp
        FString ObjectPath = FString::Printf(TEXT("%s/%s"), *Path, *Name);
        if (UEditorAssetLibrary::DoesAssetExist(ObjectPath))
        {
            UObject* ExistingAsset = UEditorAssetLibrary::LoadAsset(ObjectPath);
            if (ExistingAsset)
            {
                if (Cast<UAnimBlueprint>(ExistingAsset))
                {
                    // Same type - return success with existing asset info
                    Response->SetStringField(TEXT("assetPath"), ObjectPath);
                    Response->SetBoolField(TEXT("existingAsset"), true);
                    ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Animation Blueprint '%s' already exists - reusing existing asset"), *Name));
                    return Response;
                }
                else
                {
                    UBlueprint* ExistingBlueprint = Cast<UBlueprint>(ExistingAsset);
                    const bool bLegacyPlainBlueprint =
                        ExistingBlueprint && ExistingAsset->GetClass() == UBlueprint::StaticClass();

                    if (bLegacyPlainBlueprint)
                    {
                        const FString ExistingParentClassName =
                            ExistingBlueprint->ParentClass ? ExistingBlueprint->ParentClass->GetPathName() : TEXT("<none>");
                        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
                            TEXT("create_anim_blueprint: Replacing legacy plain Blueprint at '%s' (parent=%s)"),
                            *ObjectPath,
                            *ExistingParentClassName);

                        const bool bDeletedLegacyAsset = UEditorAssetLibrary::DeleteAsset(ObjectPath);
                        if (!bDeletedLegacyAsset || UEditorAssetLibrary::DoesAssetExist(ObjectPath))
                        {
                            ANIM_ERROR_RESPONSE(
                                FString::Printf(TEXT("Failed to replace legacy plain Blueprint at '%s' before creating AnimBlueprint"), *ObjectPath),
                                TEXT("LEGACY_ASSET_DELETE_FAILED")
                            );
                        }
                    }
                    else
                    {
                        // Different type - return error to prevent assertion crash
                        FString ExistingClassName = ExistingAsset->GetClass()->GetName();
                        FString ExistingParentClassName =
                            (ExistingBlueprint && ExistingBlueprint->ParentClass)
                                ? ExistingBlueprint->ParentClass->GetPathName()
                                : TEXT("<none>");
                        ANIM_ERROR_RESPONSE(
                            FString::Printf(TEXT("Cannot create AnimBlueprint: asset '%s' already exists as type '%s' (parent=%s)"),
                                *ObjectPath, *ExistingClassName, *ExistingParentClassName),
                            TEXT("ASSET_TYPE_MISMATCH")
                        );
                    }
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

        UAnimBlueprintFactory* Factory = NewObject<UAnimBlueprintFactory>();
        Factory->TargetSkeleton = Skeleton;
        Factory->ParentClass = UAnimInstance::StaticClass();
        UAnimBlueprint* NewAnimBP = Cast<UAnimBlueprint>(
            Factory->FactoryCreateNew(UAnimBlueprint::StaticClass(), Package,
                                      FName(*Name), RF_Public | RF_Standalone,
                                      nullptr, GWarn));
        if (!NewAnimBP)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create animation blueprint"), TEXT("CREATE_FAILED"));
        }

        // Compile the blueprint
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(NewAnimBP);

        SaveAnimAsset(NewAnimBP, bSave);

        FString FullPath = Path / Name;
        Response->SetStringField(TEXT("assetPath"), FullPath);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Animation Blueprint '%s' created"), *Name));
        return Response;
    }
    return nullptr;
}

} // namespace McpAnimationAuthoring
#endif // WITH_EDITOR
