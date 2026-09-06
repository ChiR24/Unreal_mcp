#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersAssetLoading.h"
#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersPayload.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SkeletalMesh.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersProjectPaths.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "UObject/Package.h"
// UE 5.7 ships PhysicsAssetUtils.h in Developer/PhysicsUtilities (a public
// dependency of UnrealEd); earlier versions ship it in UnrealEd itself.
#if __has_include("PhysicsAssetUtils.h")
#include "PhysicsAssetUtils.h"
#define MCP_HAS_PHYSICS_ASSET_UTILS 1
#else
#define MCP_HAS_PHYSICS_ASSET_UTILS 0
#endif

#if WITH_EDITOR
using namespace McpSkeletonHandlers;

#if MCP_HAS_PHYSICS_ASSET_UTILS
namespace
{
// Optional overrides on top of the editor's "Create Physics Asset" defaults.
void ApplyCreateParams(const TSharedPtr<FJsonObject>& Payload, FPhysAssetCreateParams& Params, FString& OutGeomType)
{
    Params.MinBoneSize = static_cast<float>(GetJsonNumberField(Payload, TEXT("minBoneSize"), Params.MinBoneSize));
    Params.bCreateConstraints = GetJsonBoolField(Payload, TEXT("createConstraints"), Params.bCreateConstraints);
    Params.bBodyForAll = GetJsonBoolField(Payload, TEXT("bodyForAll"), Params.bBodyForAll);

    const FString GeomType = GetJsonStringField(Payload, TEXT("geomType")).ToLower();
    if (GeomType == TEXT("box"))
    {
        Params.GeomType = EFG_Box;
    }
    else if (GeomType == TEXT("sphere"))
    {
        Params.GeomType = EFG_Sphere;
    }
    else if (GeomType == TEXT("capsule") || GeomType == TEXT("sphyl"))
    {
        Params.GeomType = EFG_Sphyl;
    }
    else if (GeomType == TEXT("singleconvexhull") || GeomType == TEXT("single_convex_hull") || GeomType == TEXT("convex"))
    {
        Params.GeomType = EFG_SingleConvexHull;
    }
    else if (GeomType == TEXT("multiconvexhull") || GeomType == TEXT("multi_convex_hull"))
    {
        Params.GeomType = EFG_MultiConvexHull;
    }

    switch (Params.GeomType.GetValue())
    {
        case EFG_Box: OutGeomType = TEXT("Box"); break;
        case EFG_Sphere: OutGeomType = TEXT("Sphere"); break;
        case EFG_SingleConvexHull: OutGeomType = TEXT("SingleConvexHull"); break;
        case EFG_MultiConvexHull: OutGeomType = TEXT("MultiConvexHull"); break;
        default: OutGeomType = TEXT("Capsule"); break;
    }
}
} // namespace
#endif

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
    if (SkeletalMeshPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("skeletalMeshPath (or skeletonPath) is required"), TEXT("MISSING_PARAM"));
        return true;
    }

#if !MCP_HAS_PHYSICS_ASSET_UTILS
    SendAutomationError(RequestingSocket, RequestId,
        TEXT("Physics asset body generation is unavailable in this engine build (PhysicsAssetUtils.h not found)"),
        TEXT("NOT_SUPPORTED"));
    return true;
#else
    FString Error;
    USkeletalMesh* SkeletalMesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!SkeletalMesh)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return true;
    }

    FString OutputPath = GetJsonStringField(Payload, TEXT("outputPath"));
    if (OutputPath.IsEmpty())
    {
        // name + path/savePath form, else <mesh folder>/<mesh>_PhysicsAsset.
        const FString AssetName = GetJsonStringField(Payload, TEXT("name"));
        FString Directory = GetJsonStringField(Payload, TEXT("path"));
        if (Directory.IsEmpty())
        {
            Directory = GetJsonStringField(Payload, TEXT("savePath"));
        }
        if (Directory.IsEmpty())
        {
            Directory = FPaths::GetPath(SkeletalMeshPath);
        }
        OutputPath = Directory / (AssetName.IsEmpty() ? FPaths::GetBaseFilename(SkeletalMeshPath) + TEXT("_PhysicsAsset") : AssetName);
    }
    const FString SanitizedOutputPath = SanitizeProjectRelativePath(OutputPath);
    if (SanitizedOutputPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Invalid outputPath '%s': contains traversal sequences"), *OutputPath), TEXT("INVALID_PATH"));
        return true;
    }

    // Create package and asset directly to avoid UI dialogs
    const FString PackagePath = FPaths::GetPath(SanitizedOutputPath);
    const FString AssetName = FPaths::GetBaseFilename(SanitizedOutputPath);
    const FString FullPackagePath = PackagePath / AssetName;

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

    // Generate bodies and constraints the way the editor's Create Physics
    // Asset dialog does; an empty asset satisfied nothing (dogfood #98).
    const bool bAssignToMesh = GetJsonBoolField(Payload, TEXT("assignToMesh"), false);
    FPhysAssetCreateParams Params;
    FString GeomTypeName;
    ApplyCreateParams(Payload, Params, GeomTypeName);
    FText CreateError;
    const bool bGenerated = FPhysicsAssetUtils::CreateFromSkeletalMesh(PhysicsAsset, SkeletalMesh, Params, CreateError, bAssignToMesh);
    if (!bGenerated || PhysicsAsset->SkeletalBodySetups.Num() == 0)
    {
        // Never leave an empty asset behind as if it were the deliverable.
        PhysicsAsset->ClearFlags(RF_Public | RF_Standalone);
        PhysicsAsset->MarkAsGarbage();
        const FString Reason = CreateError.IsEmpty()
            ? FString::Printf(TEXT("no bodies were generated for %s (bones may be smaller than minBoneSize %.2f)"), *SkeletalMesh->GetName(), Params.MinBoneSize)
            : CreateError.ToString();
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Physics asset body generation failed: %s"), *Reason), TEXT("BODY_GENERATION_FAILED"));
        return true;
    }

    PhysicsAsset->SetPreviewMesh(SkeletalMesh);
    PhysicsAsset->UpdateBodySetupIndexMap();
    PhysicsAsset->UpdateBoundsBodiesArray();
    FAssetRegistryModule::AssetCreated(PhysicsAsset);
    Package->MarkPackageDirty();
    McpSafeAssetSave(PhysicsAsset);
    if (bAssignToMesh)
    {
        McpSafeAssetSave(SkeletalMesh);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("physicsAssetPath"), PhysicsAsset->GetPathName());
    Result->SetStringField(TEXT("assetPath"), PhysicsAsset->GetPathName());
    Result->SetStringField(TEXT("skeletalMeshPath"), SkeletalMesh->GetPathName());
    Result->SetNumberField(TEXT("bodyCount"), PhysicsAsset->SkeletalBodySetups.Num());
    Result->SetNumberField(TEXT("constraintCount"), PhysicsAsset->ConstraintSetup.Num());
    Result->SetStringField(TEXT("geomType"), GeomTypeName);
    Result->SetNumberField(TEXT("minBoneSize"), Params.MinBoneSize);
    Result->SetBoolField(TEXT("assignedToMesh"), bAssignToMesh);
    McpHandlerUtils::AddVerification(Result, PhysicsAsset);

    SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Physics asset created with %d bodies and %d constraints"),
            PhysicsAsset->SkeletalBodySetups.Num(), PhysicsAsset->ConstraintSetup.Num()), Result);
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSetPhysicsAsset(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
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
}

#endif // WITH_EDITOR
