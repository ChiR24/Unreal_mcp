#include "McpAutomationBridge_SkeletonHandlersAssetLoading.h"
#include "McpAutomationBridge_SkeletonHandlersPayload.h"

#include "Engine/SkeletalMesh.h"
#include "McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
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

bool UMcpAutomationBridgeSubsystem::HandleBindClothToSkeletalMesh(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletalMeshPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));
    FString ClothAssetName = GetStringFieldSkel(Payload, TEXT("clothAssetName"));
    int32 MeshLodIndex = 0;
    int32 SectionIndex = 0;
    int32 AssetLodIndex = 0;

    Payload->TryGetNumberField(TEXT("meshLodIndex"), MeshLodIndex);
    Payload->TryGetNumberField(TEXT("sectionIndex"), SectionIndex);
    Payload->TryGetNumberField(TEXT("assetLodIndex"), AssetLodIndex);

    if (SkeletalMeshPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("skeletalMeshPath is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    FString Error;
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!Mesh)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return true;
    }

#if WITH_EDITOR
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath);

    // Find the cloth asset by name if provided
    UClothingAssetBase* TargetClothAsset = nullptr;
    // UE 5.7 returns TArray<TObjectPtr<>> - UE 5.0 returns TArray<UClothingAssetBase*>
    const auto& ClothingAssets = Mesh->GetMeshClothingAssets();

    if (!ClothAssetName.IsEmpty())
    {
        for (const auto& ClothAssetPtr : ClothingAssets)
        {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
            // UE 5.3+ uses TObjectPtr in non-const getter
            UClothingAssetBase* ClothAsset = ClothAssetPtr.Get();
#else
            // UE 5.0-5.2 uses raw pointers
            UClothingAssetBase* ClothAsset = ClothAssetPtr;
#endif
            if (ClothAsset && ClothAsset->GetName() == ClothAssetName)
            {
                TargetClothAsset = ClothAsset;
                break;
            }
        }

        if (!TargetClothAsset)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Cloth asset '%s' not found on mesh"), *ClothAssetName),
                TEXT("CLOTH_NOT_FOUND"));
            return true;
        }

        // Bind the cloth asset to the specified section
        bool bSuccess = TargetClothAsset->BindToSkeletalMesh(Mesh, MeshLodIndex, SectionIndex, AssetLodIndex);

        if (bSuccess)
        {
            McpSafeAssetSave(Mesh);
            Result->SetBoolField(TEXT("success"), true);
            Result->SetStringField(TEXT("clothAssetName"), ClothAssetName);
            Result->SetNumberField(TEXT("meshLodIndex"), MeshLodIndex);
            Result->SetNumberField(TEXT("sectionIndex"), SectionIndex);
            Result->SetNumberField(TEXT("assetLodIndex"), AssetLodIndex);

            SendAutomationResponse(RequestingSocket, RequestId, true,
                FString::Printf(TEXT("Cloth asset '%s' bound to section %d"), *ClothAssetName, SectionIndex), Result);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId,
                TEXT("Failed to bind cloth asset to skeletal mesh section"),
                TEXT("BIND_FAILED"));
            return true;
        }
    }
    else
    {
        // No cloth asset specified - return list of available cloth assets
        TArray<TSharedPtr<FJsonValue>> ClothingArray;
        for (const auto& ClothAssetPtr : ClothingAssets)
        {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
            UClothingAssetBase* ClothAsset = ClothAssetPtr.Get();
#else
            UClothingAssetBase* ClothAsset = ClothAssetPtr;
#endif
            if (!ClothAsset) continue;

            TSharedPtr<FJsonObject> ClothObj = McpHandlerUtils::CreateResultObject();
            ClothObj->SetStringField(TEXT("name"), ClothAsset->GetName());
            // Use UClothingAssetCommon::GetNumLods() for UE 5.7+ compatibility
            if (UClothingAssetCommon* ClothAssetCommon = Cast<UClothingAssetCommon>(ClothAsset))
            {
                ClothObj->SetNumberField(TEXT("numLods"), ClothAssetCommon->GetNumLods());
            }
            ClothingArray.Add(MakeShared<FJsonValueObject>(ClothObj));
        }

        Result->SetArrayField(TEXT("availableClothAssets"), ClothingArray);
        Result->SetNumberField(TEXT("clothingAssetCount"), ClothingAssets.Num());

        SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Found %d cloth assets. Provide clothAssetName to bind."), ClothingAssets.Num()), Result);
    }

    return true;
#else
    SendAutomationError(RequestingSocket, RequestId,
        TEXT("Cloth binding requires editor mode."),
        TEXT("NOT_EDITOR"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleAssignClothAssetToMesh(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletalMeshPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));

    if (SkeletalMeshPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("skeletalMeshPath is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    FString Error;
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!Mesh)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return true;
    }

    // List current clothing assets
    TArray<TSharedPtr<FJsonValue>> ClothingArray;
    for (const auto& ClothAssetPtr : Mesh->GetMeshClothingAssets())
    {
        #if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
        UClothingAssetBase* ClothAsset = ClothAssetPtr.Get();
        #else
        UClothingAssetBase* ClothAsset = ClothAssetPtr;
        #endif
        if (!ClothAsset) continue;

        TSharedPtr<FJsonObject> ClothObj = McpHandlerUtils::CreateResultObject();
        ClothObj->SetStringField(TEXT("name"), ClothAsset->GetName());
        ClothingArray.Add(MakeShared<FJsonValueObject>(ClothObj));
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath);
    Result->SetArrayField(TEXT("clothingAssets"), ClothingArray);
    Result->SetNumberField(TEXT("count"), ClothingArray.Num());

    // Return an explicit error rather than claiming cloth assignment occurred.
    SendAutomationError(RequestingSocket, RequestId,
        TEXT("Cloth asset assignment requires using the Cloth Paint tool in Unreal Editor. ")
        TEXT("Use the Skeletal Mesh Editor's Paint Cloth tool to assign cloth assets to mesh sections."),
        TEXT("MANUAL_INTERVENTION_REQUIRED"));
    return true;
}

#endif // WITH_EDITOR
