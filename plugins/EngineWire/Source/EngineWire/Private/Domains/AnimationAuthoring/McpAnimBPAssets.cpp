#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Domains/AnimationAuthoring/McpAnimSupport.h"

namespace McpAnimationAuthoring {

TSharedPtr<FJsonObject> HandleBlueprintAssetActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    // The one Animation Blueprint creator: the record's name and the two former
    // names folded into it all reach this branch.
    if (SubAction == TEXT("create_animation_blueprint") ||
        SubAction == TEXT("create_anim_blueprint") ||
        SubAction == TEXT("create_animation_bp"))
    {
    FString Name = GetJsonStringField(Params, TEXT("name"), TEXT(""));
    FString Path = NormalizeAnimPath(GetJsonStringField(Params, TEXT("path"), TEXT("/Game/Blueprints")));
    FString SkeletonPath = GetJsonStringField(Params, TEXT("skeletonPath"), TEXT(""));
    FString ParentClass = GetJsonStringField(Params, TEXT("parentClass"), TEXT("AnimInstance"));
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
    else if (Params->HasField(TEXT("skeletalMeshPath")))
    {
        // The skeleton a mesh is built on, for a caller holding only the mesh.
        const FString MeshPath = GetJsonStringField(Params, TEXT("skeletalMeshPath"), TEXT(""));
        USkeletalMesh* Mesh = LoadSkeletalMeshFromPathAnim(MeshPath);
        Skeleton = Mesh ? Mesh->GetSkeleton() : nullptr;
        if (!Skeleton)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load a skeleton from skeletal mesh: %s"), *MeshPath), TEXT("SKELETON_NOT_FOUND"));
        }
    }

    // parentClass was read and then ignored: every Blueprint derived from
    // AnimInstance whatever was asked for.
    UClass* ParentAnimClass = ResolveClassByName(ParentClass);
    if (!ParentAnimClass || !ParentAnimClass->IsChildOf(UAnimInstance::StaticClass()))
    {
        ANIM_ERROR_RESPONSE(FString::Printf(TEXT("parentClass '%s' is not an AnimInstance class; pass AnimInstance, a native AnimInstance subclass or an Animation Blueprint path"), *ParentClass), TEXT("INVALID_PARENT_CLASS"));
    }

    // Check if an asset already exists at the target path to prevent assertion failure in Kismet2.cpp
        FString ObjectPath = FString::Printf(TEXT("%s/%s"), *Path, *Name);
        if (McpAssetExists(ObjectPath))
        {
            UObject* ExistingAsset = McpLoadAsset(ObjectPath);
            if (ExistingAsset)
            {
                if (Cast<UAnimBlueprint>(ExistingAsset))
                {
                    // Same type - return success with existing asset info
                    Response->SetStringField(TEXT("assetPath"), ObjectPath);
                    Response->SetBoolField(TEXT("existingAsset"), true);
                    McpHandlerUtils::MarkNoAssetsChanged(Response);
                    ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Animation Blueprint '%s' already exists - reusing existing asset"), *Name));
                    return Response;
                }
                else
                {
                    // Any other asset is refused. A plain Blueprint here used to be force-deleted and replaced, which
                    // destroyed whatever Blueprint shared the name, without consent.
                    UBlueprint* ExistingBlueprint = Cast<UBlueprint>(ExistingAsset);
                    FString ExistingClassName = ExistingAsset->GetClass()->GetName();
                    FString ExistingParentClassName =
                        (ExistingBlueprint && ExistingBlueprint->ParentClass)
                            ? ExistingBlueprint->ParentClass->GetPathName()
                            : TEXT("<none>");
                    ANIM_ERROR_RESPONSE(
                        FString::Printf(TEXT("Cannot create AnimBlueprint: asset '%s' already exists as type '%s' (parent=%s); choose another name or delete it first"),
                            *ObjectPath, *ExistingClassName, *ExistingParentClassName),
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

        UAnimBlueprintFactory* Factory = NewObject<UAnimBlueprintFactory>();
        Factory->TargetSkeleton = Skeleton;
        Factory->ParentClass = ParentAnimClass;
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

        const bool bSaved = SaveAnimAsset(NewAnimBP, bSave);

        FString FullPath = Path / Name;
        Response->SetStringField(TEXT("assetPath"), FullPath);
        Response->SetBoolField(TEXT("saved"), bSaved);
        Response->SetStringField(TEXT("parentClass"), ParentAnimClass->GetName());
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Animation Blueprint '%s' created"), *Name));
        return Response;
    }
    return nullptr;
}

} // namespace McpAnimationAuthoring
