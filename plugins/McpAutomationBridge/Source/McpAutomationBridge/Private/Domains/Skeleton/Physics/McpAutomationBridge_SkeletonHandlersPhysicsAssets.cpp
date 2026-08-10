#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersAssetLoading.h"
#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersPayload.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SkeletalMesh.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "UObject/Package.h"

#if WITH_EDITOR
using namespace McpSkeletonHandlers;

bool UMcpAutomationBridgeSubsystem::HandleCreatePhysicsAsset(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    // Also accept skeletonPath for backward compatibility
    if (SkeletalMeshPath.IsEmpty())
    {
        SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletonPath"));
    }
    FString OutputPath = GetJsonStringField(Payload, TEXT("outputPath"));

    if (SkeletalMeshPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("skeletalMeshPath (or skeletonPath) is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    FString Error;
    USkeletalMesh* SkeletalMesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!SkeletalMesh)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return true;
    }

    if (OutputPath.IsEmpty())
    {
        OutputPath = FPaths::GetPath(SkeletalMeshPath);
        FString MeshName = FPaths::GetBaseFilename(SkeletalMeshPath);
        OutputPath = FString::Printf(TEXT("%s/%s_PhysicsAsset"), *OutputPath, *MeshName);
    }

    // Create package and asset directly to avoid UI dialogs
    FString PackagePath = FPaths::GetPath(OutputPath);
    FString AssetName = FPaths::GetBaseFilename(OutputPath);
    FString FullPackagePath = PackagePath / AssetName;

    UPackage* Package = CreatePackage(*FullPackagePath);
    if (!Package)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
        return true;
    }

    UPhysicsAsset* PhysicsAsset = NewObject<UPhysicsAsset>(Package, FName(*AssetName), RF_Public | RF_Standalone | RF_Transactional);
    if (!PhysicsAsset)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create physics asset"), TEXT("CREATE_FAILED"));
        return true;
    }

    PhysicsAsset->SetPreviewMesh(SkeletalMesh);
    PhysicsAsset->UpdateBodySetupIndexMap();
    PhysicsAsset->UpdateBoundsBodiesArray();
    FAssetRegistryModule::AssetCreated(PhysicsAsset);
    Package->MarkPackageDirty();
    McpSafeAssetSave(PhysicsAsset);

    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("physicsAssetPath"), PhysicsAsset->GetPathName());
    Result->SetStringField(TEXT("skeletalMeshPath"), SkeletalMesh->GetPathName());
    Result->SetNumberField(TEXT("bodyCount"), PhysicsAsset->SkeletalBodySetups.Num());
    Result->SetNumberField(TEXT("constraintCount"), PhysicsAsset->ConstraintSetup.Num());

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Physics asset created"), Result);
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleSetPhysicsAsset(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    FString SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    if (SkeletalMeshPath.IsEmpty())
    {
        SkeletalMeshPath = GetJsonStringField(Payload, TEXT("meshPath"));
    }
    FString PhysicsAssetPath = GetJsonStringField(Payload, TEXT("physicsAssetPath"));

    if (SkeletalMeshPath.IsEmpty() || PhysicsAssetPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("skeletalMeshPath and physicsAssetPath are required"), TEXT("MISSING_PARAM"));
        return true;
    }

    FString Error;
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!Mesh)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UPhysicsAsset* PhysAsset = Cast<UPhysicsAsset>(
        StaticLoadObject(UPhysicsAsset::StaticClass(), nullptr, *PhysicsAssetPath));
    if (!PhysAsset)
    {
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Physics asset not found: %s"), *PhysicsAssetPath),
            TEXT("PHYSICS_ASSET_NOT_FOUND"));
        return true;
    }

    Mesh->SetPhysicsAsset(PhysAsset);
    Mesh->MarkPackageDirty();
    McpSafeAssetSave(Mesh);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath);
    Result->SetStringField(TEXT("physicsAssetPath"), PhysicsAssetPath);
    Result->SetStringField(TEXT("physicsAssetName"), PhysAsset->GetName());

    SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Physics asset '%s' assigned to skeletal mesh"), *PhysAsset->GetName()), Result);
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("set_physics_asset requires editor mode"), TEXT("NOT_EDITOR"));
    return true;
#endif
}

#endif // WITH_EDITOR
