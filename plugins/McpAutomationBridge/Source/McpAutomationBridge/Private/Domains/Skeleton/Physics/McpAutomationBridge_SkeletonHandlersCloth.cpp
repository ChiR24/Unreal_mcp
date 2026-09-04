#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersAssetLoading.h"
#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersPayload.h"

#include "Engine/SkeletalMesh.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersProjectPaths.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#if __has_include("ClothingAsset/ClothingAssetBase.h")
#include "ClothingAsset/ClothingAssetBase.h"
#elif __has_include("ClothingAssetBase.h")
#include "ClothingAssetBase.h"
#endif
#if __has_include("ClothingAsset.h")
#include "ClothingAsset.h"
#elif __has_include("ClothingAssetCommon.h")
#include "ClothingAssetCommon.h"
#endif

#if WITH_EDITOR
using namespace McpSkeletonHandlers;

namespace
{
struct FClothBindOutcome
{
    bool bSuccess = false;
    FString Message;
    FString ErrorCode;
    TSharedPtr<FJsonObject> Result;
};

TArray<TSharedPtr<FJsonValue>> DescribeClothAssets(USkeletalMesh* Mesh)
{
    TArray<TSharedPtr<FJsonValue>> ClothingArray;
    for (UClothingAssetBase* ClothAsset : Mesh->GetMeshClothingAssets())
    {
        if (!ClothAsset)
        {
            continue;
        }
        TSharedPtr<FJsonObject> ClothObj = McpHandlerUtils::CreateResultObject();
        ClothObj->SetStringField(TEXT("name"), ClothAsset->GetName());
        ClothObj->SetStringField(TEXT("path"), ClothAsset->GetPathName());
        if (const UClothingAssetCommon* CommonAsset = Cast<UClothingAssetCommon>(ClothAsset))
        {
            ClothObj->SetNumberField(TEXT("numLods"), CommonAsset->GetNumLods());
        }
        ClothingArray.Add(MakeShared<FJsonValueObject>(ClothObj));
    }
    return ClothingArray;
}

// Resolves the cloth asset a request names: by name (or path) among the
// mesh's clothing assets, else by object path. A path-loaded asset that is
// not yet registered on the mesh is added when bAllowAdd is set.
UClothingAssetBase* ResolveClothAsset(USkeletalMesh* Mesh, const FString& ClothAssetName, const FString& ClothAssetPath,
    bool bAllowAdd, FString& OutError, FString& OutErrorCode)
{
    if (!ClothAssetName.IsEmpty())
    {
        for (UClothingAssetBase* ClothAsset : Mesh->GetMeshClothingAssets())
        {
            if (ClothAsset && (ClothAsset->GetName() == ClothAssetName || ClothAsset->GetPathName() == ClothAssetName))
            {
                return ClothAsset;
            }
        }
    }

    if (!ClothAssetPath.IsEmpty())
    {
        const FString SanitizedPath = SanitizeProjectRelativePath(ClothAssetPath);
        if (SanitizedPath.IsEmpty())
        {
            OutError = FString::Printf(TEXT("Invalid clothAssetPath '%s': contains traversal sequences"), *ClothAssetPath);
            OutErrorCode = TEXT("INVALID_PATH");
            return nullptr;
        }
        UClothingAssetBase* Loaded = Cast<UClothingAssetBase>(StaticLoadObject(UClothingAssetBase::StaticClass(), nullptr, *SanitizedPath));
        if (!Loaded)
        {
            OutError = FString::Printf(TEXT("Cloth asset not found: %s"), *ClothAssetPath);
            OutErrorCode = TEXT("CLOTH_NOT_FOUND");
            return nullptr;
        }
        if (!Mesh->GetMeshClothingAssets().Contains(Loaded))
        {
            if (!bAllowAdd)
            {
                OutError = FString::Printf(TEXT("Cloth asset '%s' is not registered on %s; use assign_cloth_asset_to_mesh to add it"),
                    *Loaded->GetName(), *Mesh->GetPathName());
                OutErrorCode = TEXT("CLOTH_NOT_FOUND");
                return nullptr;
            }
            Mesh->AddClothingAsset(Loaded);
        }
        return Loaded;
    }

    OutError = FString::Printf(TEXT("Cloth asset '%s' not found on %s"), *ClothAssetName, *Mesh->GetPathName());
    OutErrorCode = TEXT("CLOTH_NOT_FOUND");
    return nullptr;
}

FClothBindOutcome RunClothBinding(const TSharedPtr<FJsonObject>& Payload, bool bAllowAdd)
{
    FClothBindOutcome Outcome;
    const FString SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    if (SkeletalMeshPath.IsEmpty())
    {
        Outcome.Message = TEXT("skeletalMeshPath is required");
        Outcome.ErrorCode = TEXT("MISSING_PARAM");
        return Outcome;
    }

    FString Error;
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!Mesh)
    {
        Outcome.Message = Error;
        Outcome.ErrorCode = TEXT("MESH_NOT_FOUND");
        return Outcome;
    }

    const FString ClothAssetName = GetJsonStringField(Payload, TEXT("clothAssetName"));
    const FString ClothAssetPath = GetJsonStringField(Payload, TEXT("clothAssetPath"));
    const int32 MeshLodIndex = GetIntFieldSkel(Payload, TEXT("meshLodIndex"), GetIntFieldSkel(Payload, TEXT("lodIndex"), 0));
    const int32 SectionIndex = GetIntFieldSkel(Payload, TEXT("sectionIndex"), 0);
    const int32 AssetLodIndex = GetIntFieldSkel(Payload, TEXT("assetLodIndex"), 0);

    Outcome.Result = McpHandlerUtils::CreateResultObject();
    Outcome.Result->SetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath);
    Outcome.Result->SetArrayField(TEXT("availableClothAssets"), DescribeClothAssets(Mesh));

    if (ClothAssetName.IsEmpty() && ClothAssetPath.IsEmpty())
    {
        // Listing the candidates is not a binding; fail so the caller knows
        // nothing changed (dogfood #97).
        Outcome.Message = FString::Printf(
            TEXT("No cloth asset specified; %s has %d clothing asset(s). Pass clothAssetName (see availableClothAssets) or clothAssetPath."),
            *Mesh->GetName(), Mesh->GetMeshClothingAssets().Num());
        Outcome.ErrorCode = TEXT("CLOTH_ASSET_REQUIRED");
        return Outcome;
    }

    UClothingAssetBase* ClothAsset = ResolveClothAsset(Mesh, ClothAssetName, ClothAssetPath, bAllowAdd, Outcome.Message, Outcome.ErrorCode);
    if (!ClothAsset)
    {
        return Outcome;
    }

    Mesh->Modify();
    if (!ClothAsset->BindToSkeletalMesh(Mesh, MeshLodIndex, SectionIndex, AssetLodIndex))
    {
        Outcome.Message = FString::Printf(
            TEXT("Failed to bind cloth asset '%s' to %s LOD %d section %d (asset LOD %d); check that the LOD, section and asset LOD exist"),
            *ClothAsset->GetName(), *Mesh->GetName(), MeshLodIndex, SectionIndex, AssetLodIndex);
        Outcome.ErrorCode = TEXT("BIND_FAILED");
        return Outcome;
    }
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    McpSafeAssetSave(Mesh);

    Outcome.bSuccess = true;
    Outcome.Result->SetStringField(TEXT("clothAssetName"), ClothAsset->GetName());
    Outcome.Result->SetStringField(TEXT("clothAssetPath"), ClothAsset->GetPathName());
    Outcome.Result->SetNumberField(TEXT("meshLodIndex"), MeshLodIndex);
    Outcome.Result->SetNumberField(TEXT("sectionIndex"), SectionIndex);
    Outcome.Result->SetNumberField(TEXT("assetLodIndex"), AssetLodIndex);
    Outcome.Result->SetBoolField(TEXT("bound"), true);
    McpHandlerUtils::AddVerification(Outcome.Result, Mesh);
    Outcome.Message = FString::Printf(TEXT("Cloth asset '%s' bound to %s LOD %d section %d"),
        *ClothAsset->GetName(), *Mesh->GetName(), MeshLodIndex, SectionIndex);
    return Outcome;
}
} // namespace

bool UMcpAutomationBridgeSubsystem::HandleBindClothToSkeletalMesh(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FClothBindOutcome Outcome = RunClothBinding(Payload, /*bAllowAdd*/ false);
    SendAutomationResponse(RequestingSocket, RequestId, Outcome.bSuccess, Outcome.Message, Outcome.Result, Outcome.ErrorCode);
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleAssignClothAssetToMesh(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Same binding as bind_cloth_to_skeletal_mesh, but a clothAssetPath that
    // is not yet on the mesh is registered first (UClothingAssetBase::
    // BindToSkeletalMesh, implemented by UClothingAssetCommon).
    const FClothBindOutcome Outcome = RunClothBinding(Payload, /*bAllowAdd*/ true);
    SendAutomationResponse(RequestingSocket, RequestId, Outcome.bSuccess, Outcome.Message, Outcome.Result, Outcome.ErrorCode);
    return true;
}

#endif // WITH_EDITOR
