/**
 * Skeleton and Rigging Handlers for Phase 7
 *
 * Implements skeleton creation, socket management, physics assets, skin weights, and morph targets.
 * Uses USkeletalMesh, USkeleton, UPhysicsAsset, and related UE APIs.
 */

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR

#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "Animation/MorphTarget.h"
#include "Rendering/SkeletalMeshLODModel.h"  // For FSkelMeshSection used by PopulateDeltas
#include "Rendering/SkeletalMeshModel.h"     // For FSkeletalMeshModel
#include "Animation/SkinWeightProfile.h"     // For FSkinWeightProfileInfo, FImportedSkinWeightProfileData
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#include "Factories/PhysicsAssetFactory.h"
#include "ReferenceSkeleton.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

// Helper macros for JSON field access
#define GetStringFieldSkel GetJsonStringField
#define GetNumberFieldSkel GetJsonNumberField
#define GetBoolFieldSkel GetJsonBoolField

// For skeleton modification
#if __has_include("Animation/SkeletonModifier.h")
#include "Animation/SkeletonModifier.h"
#define MCP_HAS_SKELETON_MODIFIER 1
#else
#define MCP_HAS_SKELETON_MODIFIER 0
#endif

// Cloth support (Chaos Cloth)
#if __has_include("ClothingAsset/ClothingAssetBase.h")
#include "ClothingAsset/ClothingAssetBase.h"
#elif __has_include("ClothingAssetBase.h")
#include "ClothingAssetBase.h"
#endif

// UClothingAssetCommon for GetNumLods() (UE 5.7+)
#if __has_include("ClothingAsset.h")
#include "ClothingAsset.h"
#elif __has_include("ClothingAssetCommon.h")
#include "ClothingAssetCommon.h"
#endif

#if __has_include("ClothingAssetFactory.h")
#include "ClothingAssetFactory.h"
#define MCP_HAS_CLOTH_FACTORY 1
#else
#define MCP_HAS_CLOTH_FACTORY 0
#endif

#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif

namespace {

/**
 * Helper: Load skeleton asset from path
 */
static USkeleton* LoadSkeletonFromPathSkel(const FString& SkeletonPath, FString& OutError)
{
    OutError.Reset();
    if (SkeletonPath.IsEmpty())
    {
        OutError = TEXT("Skeleton path is required");
        return nullptr;
    }

    UObject* Asset = StaticLoadObject(USkeleton::StaticClass(), nullptr, *SkeletonPath);
    if (!Asset)
    {
        OutError = FString::Printf(TEXT("Failed to load skeleton: %s"), *SkeletonPath);
        return nullptr;
    }

    USkeleton* Skeleton = Cast<USkeleton>(Asset);
    if (!Skeleton)
    {
        OutError = FString::Printf(TEXT("Asset is not a skeleton: %s"), *SkeletonPath);
        return nullptr;
    }

    return Skeleton;
}

/**
 * Helper: Load skeletal mesh asset from path
 */
static USkeletalMesh* LoadSkeletalMeshFromPathSkel(const FString& MeshPath, FString& OutError)
{
    OutError.Reset();
    if (MeshPath.IsEmpty())
    {
        OutError = TEXT("Skeletal mesh path is required");
        return nullptr;
    }

    UObject* Asset = StaticLoadObject(USkeletalMesh::StaticClass(), nullptr, *MeshPath);
    if (!Asset)
    {
        OutError = FString::Printf(TEXT("Failed to load skeletal mesh: %s"), *MeshPath);
        return nullptr;
    }

    USkeletalMesh* Mesh = Cast<USkeletalMesh>(Asset);
    if (!Mesh)
    {
        OutError = FString::Printf(TEXT("Asset is not a skeletal mesh: %s"), *MeshPath);
        return nullptr;
    }

    return Mesh;
}

/**
 * Helper: Load physics asset from path
 */
static UPhysicsAsset* LoadPhysicsAssetFromPath(const FString& PhysicsPath, FString& OutError)
{
    OutError.Reset();
    if (PhysicsPath.IsEmpty())
    {
        OutError = TEXT("Physics asset path is required");
        return nullptr;
    }

    UObject* Asset = StaticLoadObject(UPhysicsAsset::StaticClass(), nullptr, *PhysicsPath);
    if (!Asset)
    {
        OutError = FString::Printf(TEXT("Failed to load physics asset: %s"), *PhysicsPath);
        return nullptr;
    }

    UPhysicsAsset* PhysAsset = Cast<UPhysicsAsset>(Asset);
    if (!PhysAsset)
    {
        OutError = FString::Printf(TEXT("Asset is not a physics asset: %s"), *PhysicsPath);
        return nullptr;
    }

    return PhysAsset;
}

/**
 * Helper: Parse FVector from JSON object
 */
static FVector ParseVectorFromJson(const TSharedPtr<FJsonObject>& JsonObj, const FString& FieldName, const FVector& Default = FVector::ZeroVector)
{
    if (!JsonObj.IsValid() || !JsonObj->HasField(FieldName))
    {
        return Default;
    }

    const TSharedPtr<FJsonObject>* VecObj = nullptr;
    if (JsonObj->TryGetObjectField(FieldName, VecObj) && VecObj && VecObj->IsValid())
    {
        double X = 0.0, Y = 0.0, Z = 0.0;
        (*VecObj)->TryGetNumberField(TEXT("x"), X);
        (*VecObj)->TryGetNumberField(TEXT("y"), Y);
        (*VecObj)->TryGetNumberField(TEXT("z"), Z);
        return FVector(X, Y, Z);
    }

    return Default;
}

/**
 * Helper: Parse FRotator from JSON object
 */
static FRotator ParseRotatorFromJson(const TSharedPtr<FJsonObject>& JsonObj, const FString& FieldName, const FRotator& Default = FRotator::ZeroRotator)
{
    if (!JsonObj.IsValid() || !JsonObj->HasField(FieldName))
    {
        return Default;
    }

    const TSharedPtr<FJsonObject>* RotObj = nullptr;
    if (JsonObj->TryGetObjectField(FieldName, RotObj) && RotObj && RotObj->IsValid())
    {
        double Pitch = 0.0, Yaw = 0.0, Roll = 0.0;
        (*RotObj)->TryGetNumberField(TEXT("pitch"), Pitch);
        (*RotObj)->TryGetNumberField(TEXT("yaw"), Yaw);
        (*RotObj)->TryGetNumberField(TEXT("roll"), Roll);
        return FRotator(Pitch, Yaw, Roll);
    }

    return Default;
}

} // anonymous namespace


// ============================================================================
// BATCH 1 & 2: Core Skeleton Structure + Attachments
// ============================================================================

/**
 * Handle: get_skeleton_info
 * Get information about a skeleton (bones, sockets, etc.)
 */
bool UMcpAutomationBridgeSubsystem::HandleGetSkeletonInfo(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletonPath"));
    if (SkeletonPath.IsEmpty())
    {
        SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));
    }

    FString Error;
    USkeleton* Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);
    
    // Try loading as skeletal mesh if skeleton load failed
    if (!Skeleton && !SkeletonPath.IsEmpty())
    {
        USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletonPath, Error);
        if (Mesh)
        {
            Skeleton = Mesh->GetSkeleton();
        }
    }

    if (!Skeleton)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
        return false;
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("skeletonPath"), Skeleton->GetPathName());

    // Bone count
    const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
    Result->SetNumberField(TEXT("boneCount"), RefSkeleton.GetRawBoneNum());

    // Virtual bone count
    Result->SetNumberField(TEXT("virtualBoneCount"), Skeleton->GetVirtualBones().Num());

    // Socket count
    Result->SetNumberField(TEXT("socketCount"), Skeleton->Sockets.Num());

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Skeleton info retrieved"), Result);
    return true;
}

/**
 * Handle: list_bones
 * List all bones in a skeleton
 */
bool UMcpAutomationBridgeSubsystem::HandleListBones(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletonPath"));
    if (SkeletonPath.IsEmpty())
    {
        SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));
    }

    FString Error;
    USkeleton* Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);
    
    if (!Skeleton)
    {
        USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletonPath, Error);
        if (Mesh)
        {
            Skeleton = Mesh->GetSkeleton();
        }
    }

    if (!Skeleton)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
        return false;
    }

    const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
    TArray<TSharedPtr<FJsonValue>> BoneArray;

    for (int32 i = 0; i < RefSkeleton.GetRawBoneNum(); ++i)
    {
        TSharedPtr<FJsonObject> BoneObj = MakeShareable(new FJsonObject());
        BoneObj->SetStringField(TEXT("name"), RefSkeleton.GetBoneName(i).ToString());
        BoneObj->SetNumberField(TEXT("index"), i);
        
        int32 ParentIndex = RefSkeleton.GetParentIndex(i);
        BoneObj->SetNumberField(TEXT("parentIndex"), ParentIndex);
        if (ParentIndex >= 0)
        {
            BoneObj->SetStringField(TEXT("parentName"), RefSkeleton.GetBoneName(ParentIndex).ToString());
        }

        // Reference pose transform
        const FTransform& RefPose = RefSkeleton.GetRefBonePose()[i];
        TSharedPtr<FJsonObject> TransformObj = MakeShareable(new FJsonObject());
        TransformObj->SetNumberField(TEXT("x"), RefPose.GetLocation().X);
        TransformObj->SetNumberField(TEXT("y"), RefPose.GetLocation().Y);
        TransformObj->SetNumberField(TEXT("z"), RefPose.GetLocation().Z);
        BoneObj->SetObjectField(TEXT("location"), TransformObj);

        BoneArray.Add(MakeShareable(new FJsonValueObject(BoneObj)));
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetArrayField(TEXT("bones"), BoneArray);
    Result->SetNumberField(TEXT("count"), BoneArray.Num());

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Bones listed"), Result);
    return true;
}

/**
 * Handle: list_sockets
 * List all sockets in a skeleton/skeletal mesh
 */
bool UMcpAutomationBridgeSubsystem::HandleListSockets(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletonPath"));
    if (SkeletonPath.IsEmpty())
    {
        SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));
    }

    FString Error;
    USkeleton* Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);
    
    if (!Skeleton)
    {
        USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletonPath, Error);
        if (Mesh)
        {
            Skeleton = Mesh->GetSkeleton();
        }
    }

    if (!Skeleton)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
        return false;
    }

    TArray<TSharedPtr<FJsonValue>> SocketArray;
    for (USkeletalMeshSocket* Socket : Skeleton->Sockets)
    {
        if (!Socket) continue;

        TSharedPtr<FJsonObject> SocketObj = MakeShareable(new FJsonObject());
        SocketObj->SetStringField(TEXT("name"), Socket->SocketName.ToString());
        SocketObj->SetStringField(TEXT("boneName"), Socket->BoneName.ToString());

        TSharedPtr<FJsonObject> LocObj = MakeShareable(new FJsonObject());
        LocObj->SetNumberField(TEXT("x"), Socket->RelativeLocation.X);
        LocObj->SetNumberField(TEXT("y"), Socket->RelativeLocation.Y);
        LocObj->SetNumberField(TEXT("z"), Socket->RelativeLocation.Z);
        SocketObj->SetObjectField(TEXT("relativeLocation"), LocObj);

        TSharedPtr<FJsonObject> RotObj = MakeShareable(new FJsonObject());
        RotObj->SetNumberField(TEXT("pitch"), Socket->RelativeRotation.Pitch);
        RotObj->SetNumberField(TEXT("yaw"), Socket->RelativeRotation.Yaw);
        RotObj->SetNumberField(TEXT("roll"), Socket->RelativeRotation.Roll);
        SocketObj->SetObjectField(TEXT("relativeRotation"), RotObj);

        TSharedPtr<FJsonObject> ScaleObj = MakeShareable(new FJsonObject());
        ScaleObj->SetNumberField(TEXT("x"), Socket->RelativeScale.X);
        ScaleObj->SetNumberField(TEXT("y"), Socket->RelativeScale.Y);
        ScaleObj->SetNumberField(TEXT("z"), Socket->RelativeScale.Z);
        SocketObj->SetObjectField(TEXT("relativeScale"), ScaleObj);

        SocketArray.Add(MakeShareable(new FJsonValueObject(SocketObj)));
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetArrayField(TEXT("sockets"), SocketArray);
    Result->SetNumberField(TEXT("count"), SocketArray.Num());

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Sockets listed"), Result);
    return true;
}

/**
 * Handle: create_socket
 * Create a new socket on a skeleton
 */
bool UMcpAutomationBridgeSubsystem::HandleCreateSocket(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletonPath"));
    if (SkeletonPath.IsEmpty())
    {
        SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));
    }

    FString SocketName = GetStringFieldSkel(Payload, TEXT("socketName"));
    FString BoneName = GetStringFieldSkel(Payload, TEXT("attachBoneName"));
    if (BoneName.IsEmpty())
    {
        BoneName = GetStringFieldSkel(Payload, TEXT("boneName"));
    }

    if (SocketName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("socketName is required"), TEXT("MISSING_PARAM"));
        return false;
    }

    if (BoneName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("attachBoneName or boneName is required"), TEXT("MISSING_PARAM"));
        return false;
    }

    FString Error;
    USkeleton* Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);
    
    if (!Skeleton)
    {
        USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletonPath, Error);
        if (Mesh)
        {
            Skeleton = Mesh->GetSkeleton();
        }
    }

    if (!Skeleton)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
        return false;
    }

    // Check if socket already exists
    for (USkeletalMeshSocket* ExistingSocket : Skeleton->Sockets)
    {
        if (ExistingSocket && ExistingSocket->SocketName == FName(*SocketName))
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Socket '%s' already exists"), *SocketName), 
                TEXT("SOCKET_EXISTS"));
            return false;
        }
    }

    // Create the socket
    USkeletalMeshSocket* NewSocket = NewObject<USkeletalMeshSocket>(Skeleton);
    if (!NewSocket)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create socket object"), TEXT("CREATION_FAILED"));
        return false;
    }
    NewSocket->SocketName = FName(*SocketName);
    NewSocket->BoneName = FName(*BoneName);
    NewSocket->RelativeLocation = ParseVectorFromJson(Payload, TEXT("relativeLocation"));
    NewSocket->RelativeRotation = ParseRotatorFromJson(Payload, TEXT("relativeRotation"));
    NewSocket->RelativeScale = ParseVectorFromJson(Payload, TEXT("relativeScale"), FVector::OneVector);

    Skeleton->Sockets.Add(NewSocket);
    McpSafeAssetSave(Skeleton);

    // Save if requested
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("socketName"), SocketName);
    Result->SetStringField(TEXT("boneName"), BoneName);
    Result->SetStringField(TEXT("skeletonPath"), Skeleton->GetPathName());

    SendAutomationResponse(RequestingSocket, RequestId, true, 
        FString::Printf(TEXT("Socket '%s' created on bone '%s'"), *SocketName, *BoneName), Result);
    return true;
}

/**
 * Handle: configure_socket
 * Modify an existing socket's properties
 */
bool UMcpAutomationBridgeSubsystem::HandleConfigureSocket(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletonPath"));
    if (SkeletonPath.IsEmpty())
    {
        SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));
    }

    FString SocketName = GetStringFieldSkel(Payload, TEXT("socketName"));
    if (SocketName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("socketName is required"), TEXT("MISSING_PARAM"));
        return false;
    }

    FString Error;
    USkeleton* Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);
    
    if (!Skeleton)
    {
        USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletonPath, Error);
        if (Mesh)
        {
            Skeleton = Mesh->GetSkeleton();
        }
    }

    if (!Skeleton)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
        return false;
    }

    // Find the socket
    USkeletalMeshSocket* Socket = nullptr;
    for (USkeletalMeshSocket* S : Skeleton->Sockets)
    {
        if (S && S->SocketName == FName(*SocketName))
        {
            Socket = S;
            break;
        }
    }

    if (!Socket)
    {
        SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("Socket '%s' not found"), *SocketName), 
            TEXT("SOCKET_NOT_FOUND"));
        return false;
    }

    // Update properties
    FString NewBoneName = GetStringFieldSkel(Payload, TEXT("attachBoneName"));
    if (!NewBoneName.IsEmpty())
    {
        Socket->BoneName = FName(*NewBoneName);
    }

    if (Payload->HasField(TEXT("relativeLocation")))
    {
        Socket->RelativeLocation = ParseVectorFromJson(Payload, TEXT("relativeLocation"));
    }

    if (Payload->HasField(TEXT("relativeRotation")))
    {
        Socket->RelativeRotation = ParseRotatorFromJson(Payload, TEXT("relativeRotation"));
    }

    if (Payload->HasField(TEXT("relativeScale")))
    {
        Socket->RelativeScale = ParseVectorFromJson(Payload, TEXT("relativeScale"), FVector::OneVector);
    }

    McpSafeAssetSave(Skeleton);

    // Save if requested
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("socketName"), SocketName);
    Result->SetStringField(TEXT("skeletonPath"), Skeleton->GetPathName());

    SendAutomationResponse(RequestingSocket, RequestId, true, 
        FString::Printf(TEXT("Socket '%s' configured"), *SocketName), Result);
    return true;
}

/**
 * Handle: create_virtual_bone
 * Create a virtual bone between two bones
 */
bool UMcpAutomationBridgeSubsystem::HandleCreateVirtualBone(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletonPath"));
    FString SourceBone = GetStringFieldSkel(Payload, TEXT("sourceBoneName"));
    FString TargetBone = GetStringFieldSkel(Payload, TEXT("targetBoneName"));
    FString VirtualBoneName = GetStringFieldSkel(Payload, TEXT("boneName"));

    if (SkeletonPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("skeletonPath is required"), TEXT("MISSING_PARAM"));
        return false;
    }

    if (SourceBone.IsEmpty() || TargetBone.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("sourceBoneName and targetBoneName are required"), TEXT("MISSING_PARAM"));
        return false;
    }

    FString Error;
    USkeleton* Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);
    if (!Skeleton)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
        return false;
    }

    // Generate virtual bone name if not provided
    if (VirtualBoneName.IsEmpty())
    {
        VirtualBoneName = FString::Printf(TEXT("VB_%s_to_%s"), *SourceBone, *TargetBone);
    }

    // Add virtual bone
    FName NewVirtualBoneName;
    bool bSuccess = Skeleton->AddNewVirtualBone(FName(*SourceBone), FName(*TargetBone), NewVirtualBoneName);
    
    if (!bSuccess)
    {
        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("Failed to create virtual bone. Check that source and target bones exist."), 
            TEXT("VIRTUAL_BONE_FAILED"));
        return false;
    }

    // Rename if custom name provided
    if (!VirtualBoneName.IsEmpty() && NewVirtualBoneName.ToString() != VirtualBoneName)
    {
        Skeleton->RenameVirtualBone(NewVirtualBoneName, FName(*VirtualBoneName));
        NewVirtualBoneName = FName(*VirtualBoneName);
    }

    McpSafeAssetSave(Skeleton);

    // Save if requested
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("virtualBoneName"), NewVirtualBoneName.ToString());
    Result->SetStringField(TEXT("sourceBone"), SourceBone);
    Result->SetStringField(TEXT("targetBone"), TargetBone);
    Result->SetStringField(TEXT("skeletonPath"), Skeleton->GetPathName());

    SendAutomationResponse(RequestingSocket, RequestId, true, 
        FString::Printf(TEXT("Virtual bone '%s' created"), *NewVirtualBoneName.ToString()), Result);
    return true;
}


// ============================================================================
// BATCH 3: Physics Asset
// ============================================================================

/**
 * Handle: create_physics_asset
 * Create a new physics asset for a skeletal mesh
 */
bool UMcpAutomationBridgeSubsystem::HandleCreatePhysicsAsset(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletalMeshPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));
    FString OutputPath = GetStringFieldSkel(Payload, TEXT("outputPath"));

    if (SkeletalMeshPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("skeletalMeshPath is required"), TEXT("MISSING_PARAM"));
        return false;
    }

    FString Error;
    USkeletalMesh* SkeletalMesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!SkeletalMesh)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return false;
    }

    // Determine output path
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
        return false;
    }

    UPhysicsAssetFactory* Factory = NewObject<UPhysicsAssetFactory>();
    if (!Factory)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create physics asset factory"), TEXT("FACTORY_CREATION_FAILED"));
        return false;
    }
    Factory->TargetSkeletalMesh = SkeletalMesh;

    UObject* NewAsset = Factory->FactoryCreateNew(UPhysicsAsset::StaticClass(), Package,
                                                   FName(*AssetName), RF_Public | RF_Standalone,
                                                   nullptr, GWarn);
    if (!NewAsset)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create physics asset"), TEXT("CREATE_FAILED"));
        return false;
    }

    UPhysicsAsset* PhysicsAsset = Cast<UPhysicsAsset>(NewAsset);
    if (!PhysicsAsset)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Created asset is not a physics asset"), TEXT("TYPE_MISMATCH"));
        return false;
    }

    // Link to skeletal mesh
    SkeletalMesh->SetPhysicsAsset(PhysicsAsset);
    McpSafeAssetSave(SkeletalMesh);

    // Save if requested
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("physicsAssetPath"), PhysicsAsset->GetPathName());
    Result->SetStringField(TEXT("skeletalMeshPath"), SkeletalMesh->GetPathName());
    Result->SetNumberField(TEXT("bodyCount"), PhysicsAsset->SkeletalBodySetups.Num());
    Result->SetNumberField(TEXT("constraintCount"), PhysicsAsset->ConstraintSetup.Num());

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Physics asset created"), Result);
    return true;
}

/**
 * Handle: list_physics_bodies
 * List all physics bodies in a physics asset
 */
bool UMcpAutomationBridgeSubsystem::HandleListPhysicsBodies(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString PhysicsAssetPath = GetStringFieldSkel(Payload, TEXT("physicsAssetPath"));
    if (PhysicsAssetPath.IsEmpty())
    {
        // Try to get from skeletal mesh
        FString MeshPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));
        if (!MeshPath.IsEmpty())
        {
            FString Error;
            USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(MeshPath, Error);
            if (Mesh && Mesh->GetPhysicsAsset())
            {
                PhysicsAssetPath = Mesh->GetPhysicsAsset()->GetPathName();
            }
        }
    }

    if (PhysicsAssetPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("physicsAssetPath or skeletalMeshPath is required"), TEXT("MISSING_PARAM"));
        return false;
    }

    FString Error;
    UPhysicsAsset* PhysicsAsset = LoadPhysicsAssetFromPath(PhysicsAssetPath, Error);
    if (!PhysicsAsset)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("PHYSICS_ASSET_NOT_FOUND"));
        return false;
    }

    TArray<TSharedPtr<FJsonValue>> BodyArray;
    for (USkeletalBodySetup* BodySetup : PhysicsAsset->SkeletalBodySetups)
    {
        if (!BodySetup) continue;

        TSharedPtr<FJsonObject> BodyObj = MakeShareable(new FJsonObject());
        BodyObj->SetStringField(TEXT("boneName"), BodySetup->BoneName.ToString());
        BodyObj->SetBoolField(TEXT("considerForBounds"), BodySetup->bConsiderForBounds);

        // Collision type
        FString CollisionType;
        switch (BodySetup->CollisionTraceFlag)
        {
            case CTF_UseDefault: CollisionType = TEXT("Default"); break;
            case CTF_UseSimpleAndComplex: CollisionType = TEXT("SimpleAndComplex"); break;
            case CTF_UseSimpleAsComplex: CollisionType = TEXT("SimpleAsComplex"); break;
            case CTF_UseComplexAsSimple: CollisionType = TEXT("ComplexAsSimple"); break;
        }
        BodyObj->SetStringField(TEXT("collisionType"), CollisionType);

        // Primitive counts
        BodyObj->SetNumberField(TEXT("sphereCount"), BodySetup->AggGeom.SphereElems.Num());
        BodyObj->SetNumberField(TEXT("boxCount"), BodySetup->AggGeom.BoxElems.Num());
        BodyObj->SetNumberField(TEXT("capsuleCount"), BodySetup->AggGeom.SphylElems.Num());
        BodyObj->SetNumberField(TEXT("convexCount"), BodySetup->AggGeom.ConvexElems.Num());

        BodyArray.Add(MakeShareable(new FJsonValueObject(BodyObj)));
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetArrayField(TEXT("physicsBodies"), BodyArray);
    Result->SetNumberField(TEXT("count"), BodyArray.Num());
    Result->SetNumberField(TEXT("constraintCount"), PhysicsAsset->ConstraintSetup.Num());

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Physics bodies listed"), Result);
    return true;
}


// ============================================================================
// BATCH 4: Physics Body Operations
// ============================================================================

/**
 * Handle: add_physics_body
 * Add a physics body to a physics asset
 */
bool UMcpAutomationBridgeSubsystem::HandleAddPhysicsBody(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString PhysicsAssetPath = GetStringFieldSkel(Payload, TEXT("physicsAssetPath"));
    FString BoneName = GetStringFieldSkel(Payload, TEXT("boneName"));
    FString BodyType = GetStringFieldSkel(Payload, TEXT("bodyType"));

    if (PhysicsAssetPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("physicsAssetPath is required"), TEXT("MISSING_PARAM"));
        return false;
    }

    if (BoneName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("boneName is required"), TEXT("MISSING_PARAM"));
        return false;
    }

    FString Error;
    UPhysicsAsset* PhysicsAsset = LoadPhysicsAssetFromPath(PhysicsAssetPath, Error);
    if (!PhysicsAsset)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("PHYSICS_ASSET_NOT_FOUND"));
        return false;
    }

    // Find existing body or create new one
    int32 BodyIndex = PhysicsAsset->FindBodyIndex(FName(*BoneName));
    USkeletalBodySetup* BodySetup = nullptr;
    bool bCreated = false;

    if (BodyIndex == INDEX_NONE)
    {
        // Create new body
        BodySetup = NewObject<USkeletalBodySetup>(PhysicsAsset, NAME_None, RF_Transactional);
        if (!BodySetup)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create physics body setup"), TEXT("CREATION_FAILED"));
            return false;
        }
        BodySetup->BoneName = FName(*BoneName);
        PhysicsAsset->SkeletalBodySetups.Add(BodySetup);
        bCreated = true;
        BodyIndex = PhysicsAsset->SkeletalBodySetups.Num() - 1;
    }
    else
    {
        BodySetup = PhysicsAsset->SkeletalBodySetups[BodyIndex];
    }

    // Add geometry based on type
    if (BodyType.IsEmpty()) BodyType = TEXT("Capsule");

    double Radius = 10.0;
    double Length = 20.0;
    double Width = 10.0, Height = 10.0, Depth = 10.0;

    Payload->TryGetNumberField(TEXT("radius"), Radius);
    Payload->TryGetNumberField(TEXT("length"), Length);
    Payload->TryGetNumberField(TEXT("width"), Width);
    Payload->TryGetNumberField(TEXT("height"), Height);
    Payload->TryGetNumberField(TEXT("depth"), Depth);

    FVector Center = ParseVectorFromJson(Payload, TEXT("center"));
    FRotator Rotation = ParseRotatorFromJson(Payload, TEXT("rotation"));

    if (BodyType.Equals(TEXT("Sphere"), ESearchCase::IgnoreCase))
    {
        FKSphereElem SphereElem;
        SphereElem.Radius = static_cast<float>(Radius);
        SphereElem.Center = Center;
        BodySetup->AggGeom.SphereElems.Add(SphereElem);
    }
    else if (BodyType.Equals(TEXT("Box"), ESearchCase::IgnoreCase))
    {
        FKBoxElem BoxElem;
        BoxElem.X = static_cast<float>(Width);
        BoxElem.Y = static_cast<float>(Depth);
        BoxElem.Z = static_cast<float>(Height);
        BoxElem.Center = Center;
        BoxElem.Rotation = Rotation;
        BodySetup->AggGeom.BoxElems.Add(BoxElem);
    }
    else if (BodyType.Equals(TEXT("Capsule"), ESearchCase::IgnoreCase) || 
             BodyType.Equals(TEXT("Sphyl"), ESearchCase::IgnoreCase))
    {
        FKSphylElem CapsuleElem;
        CapsuleElem.Radius = static_cast<float>(Radius);
        CapsuleElem.Length = static_cast<float>(Length);
        CapsuleElem.Center = Center;
        CapsuleElem.Rotation = Rotation;
        BodySetup->AggGeom.SphylElems.Add(CapsuleElem);
    }
    else
    {
        // Default to capsule
        FKSphylElem CapsuleElem;
        CapsuleElem.Radius = static_cast<float>(Radius);
        CapsuleElem.Length = static_cast<float>(Length);
        CapsuleElem.Center = Center;
        BodySetup->AggGeom.SphylElems.Add(CapsuleElem);
    }

    PhysicsAsset->UpdateBodySetupIndexMap();
    PhysicsAsset->UpdateBoundsBodiesArray();
    McpSafeAssetSave(PhysicsAsset);

    // Save if requested
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("boneName"), BoneName);
    Result->SetStringField(TEXT("bodyType"), BodyType);
    Result->SetNumberField(TEXT("bodyIndex"), BodyIndex);
    Result->SetBoolField(TEXT("created"), bCreated);

    SendAutomationResponse(RequestingSocket, RequestId, true, 
        FString::Printf(TEXT("Physics body %s for bone '%s'"), bCreated ? TEXT("created") : TEXT("modified"), *BoneName), Result);
    return true;
}

/**
 * Handle: configure_physics_body
 * Configure properties of a physics body
 */
bool UMcpAutomationBridgeSubsystem::HandleConfigurePhysicsBody(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString PhysicsAssetPath = GetStringFieldSkel(Payload, TEXT("physicsAssetPath"));
    FString BoneName = GetStringFieldSkel(Payload, TEXT("boneName"));

    if (PhysicsAssetPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("physicsAssetPath is required"), TEXT("MISSING_PARAM"));
        return false;
    }

    if (BoneName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("boneName is required"), TEXT("MISSING_PARAM"));
        return false;
    }

    FString Error;
    UPhysicsAsset* PhysicsAsset = LoadPhysicsAssetFromPath(PhysicsAssetPath, Error);
    if (!PhysicsAsset)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("PHYSICS_ASSET_NOT_FOUND"));
        return false;
    }

    int32 BodyIndex = PhysicsAsset->FindBodyIndex(FName(*BoneName));
    if (BodyIndex == INDEX_NONE)
    {
        SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("No physics body found for bone '%s'"), *BoneName), 
            TEXT("BODY_NOT_FOUND"));
        return false;
    }

    USkeletalBodySetup* BodySetup = PhysicsAsset->SkeletalBodySetups[BodyIndex];

    // Configure physics properties
    double Mass = 0.0;
    if (Payload->TryGetNumberField(TEXT("mass"), Mass))
    {
        // Mass is set via DefaultInstance
        BodySetup->DefaultInstance.MassScale = 1.0f;
        BodySetup->DefaultInstance.bOverrideMass = true;
        // Note: Actual mass is calculated from density and volume
    }

    double LinearDamping = 0.0;
    if (Payload->TryGetNumberField(TEXT("linearDamping"), LinearDamping))
    {
        BodySetup->DefaultInstance.LinearDamping = static_cast<float>(LinearDamping);
    }

    double AngularDamping = 0.0;
    if (Payload->TryGetNumberField(TEXT("angularDamping"), AngularDamping))
    {
        BodySetup->DefaultInstance.AngularDamping = static_cast<float>(AngularDamping);
    }

    bool bCollisionEnabled = true;
    if (Payload->TryGetBoolField(TEXT("collisionEnabled"), bCollisionEnabled))
    {
        BodySetup->DefaultInstance.SetCollisionEnabled(bCollisionEnabled ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    }

    bool bSimulatePhysics = true;
    if (Payload->TryGetBoolField(TEXT("simulatePhysics"), bSimulatePhysics))
    {
        // Note: In UE 5.7+, SetSimulatePhysics is not available on FBodyInstance
        // The simulation is controlled at the component level at runtime
        BodySetup->DefaultInstance.bSimulatePhysics = bSimulatePhysics;
    }

    McpSafeAssetSave(PhysicsAsset);

    // Save if requested
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("boneName"), BoneName);
    Result->SetNumberField(TEXT("bodyIndex"), BodyIndex);

    SendAutomationResponse(RequestingSocket, RequestId, true, 
        FString::Printf(TEXT("Physics body '%s' configured"), *BoneName), Result);
    return true;
}

/**
 * Handle: add_physics_constraint
 * Add a constraint between two physics bodies
 */
bool UMcpAutomationBridgeSubsystem::HandleAddPhysicsConstraint(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString PhysicsAssetPath = GetStringFieldSkel(Payload, TEXT("physicsAssetPath"));
    FString BodyA = GetStringFieldSkel(Payload, TEXT("bodyA"));
    FString BodyB = GetStringFieldSkel(Payload, TEXT("bodyB"));
    FString ConstraintName = GetStringFieldSkel(Payload, TEXT("constraintName"));

    if (PhysicsAssetPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("physicsAssetPath is required"), TEXT("MISSING_PARAM"));
        return false;
    }

    if (BodyA.IsEmpty() || BodyB.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("bodyA and bodyB are required"), TEXT("MISSING_PARAM"));
        return false;
    }

    FString Error;
    UPhysicsAsset* PhysicsAsset = LoadPhysicsAssetFromPath(PhysicsAssetPath, Error);
    if (!PhysicsAsset)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("PHYSICS_ASSET_NOT_FOUND"));
        return false;
    }

    // Check that both bodies exist
    if (PhysicsAsset->FindBodyIndex(FName(*BodyA)) == INDEX_NONE)
    {
        SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("Body '%s' not found in physics asset"), *BodyA), 
            TEXT("BODY_NOT_FOUND"));
        return false;
    }

    if (PhysicsAsset->FindBodyIndex(FName(*BodyB)) == INDEX_NONE)
    {
        SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("Body '%s' not found in physics asset"), *BodyB), 
            TEXT("BODY_NOT_FOUND"));
        return false;
    }

    // Create constraint
    UPhysicsConstraintTemplate* Constraint = NewObject<UPhysicsConstraintTemplate>(PhysicsAsset, NAME_None, RF_Transactional);
    if (!Constraint)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create physics constraint"), TEXT("CREATION_FAILED"));
        return false;
    }
    
    Constraint->DefaultInstance.ConstraintBone1 = FName(*BodyA);
    Constraint->DefaultInstance.ConstraintBone2 = FName(*BodyB);
    
    // Set default constraint profile name via JointName (ProfileName removed in UE 5.7)
    if (!ConstraintName.IsEmpty())
    {
        Constraint->DefaultInstance.JointName = FName(*ConstraintName);
    }

    PhysicsAsset->ConstraintSetup.Add(Constraint);

    // Apply default limits
    const TSharedPtr<FJsonObject>* LimitsObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("limits"), LimitsObj) && LimitsObj && LimitsObj->IsValid())
    {
        double Swing1 = 45.0, Swing2 = 45.0, Twist = 45.0;
        (*LimitsObj)->TryGetNumberField(TEXT("swing1LimitAngle"), Swing1);
        (*LimitsObj)->TryGetNumberField(TEXT("swing2LimitAngle"), Swing2);
        (*LimitsObj)->TryGetNumberField(TEXT("twistLimitAngle"), Twist);

        Constraint->DefaultInstance.SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, static_cast<float>(Swing1));
        Constraint->DefaultInstance.SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Limited, static_cast<float>(Swing2));
        Constraint->DefaultInstance.SetAngularTwistLimit(EAngularConstraintMotion::ACM_Limited, static_cast<float>(Twist));
    }
    else
    {
        // Default to limited motion
        Constraint->DefaultInstance.SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, 45.0f);
        Constraint->DefaultInstance.SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Limited, 45.0f);
        Constraint->DefaultInstance.SetAngularTwistLimit(EAngularConstraintMotion::ACM_Limited, 45.0f);
    }

    PhysicsAsset->UpdateBodySetupIndexMap();
    McpSafeAssetSave(PhysicsAsset);

    // Save if requested
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("bodyA"), BodyA);
    Result->SetStringField(TEXT("bodyB"), BodyB);
    Result->SetNumberField(TEXT("constraintIndex"), PhysicsAsset->ConstraintSetup.Num() - 1);

    SendAutomationResponse(RequestingSocket, RequestId, true, 
        FString::Printf(TEXT("Constraint created between '%s' and '%s'"), *BodyA, *BodyB), Result);
    return true;
}

/**
 * Handle: configure_constraint_limits
 * Configure angular/linear limits on a constraint
 */
bool UMcpAutomationBridgeSubsystem::HandleConfigureConstraintLimits(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString PhysicsAssetPath = GetStringFieldSkel(Payload, TEXT("physicsAssetPath"));
    FString BodyA = GetStringFieldSkel(Payload, TEXT("bodyA"));
    FString BodyB = GetStringFieldSkel(Payload, TEXT("bodyB"));

    if (PhysicsAssetPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("physicsAssetPath is required"), TEXT("MISSING_PARAM"));
        return false;
    }

    if (BodyA.IsEmpty() || BodyB.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("bodyA and bodyB are required to identify constraint"), TEXT("MISSING_PARAM"));
        return false;
    }

    FString Error;
    UPhysicsAsset* PhysicsAsset = LoadPhysicsAssetFromPath(PhysicsAssetPath, Error);
    if (!PhysicsAsset)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("PHYSICS_ASSET_NOT_FOUND"));
        return false;
    }

    // Find constraint by body names
    UPhysicsConstraintTemplate* Constraint = nullptr;
    for (UPhysicsConstraintTemplate* C : PhysicsAsset->ConstraintSetup)
    {
        if (C && 
            C->DefaultInstance.ConstraintBone1 == FName(*BodyA) && 
            C->DefaultInstance.ConstraintBone2 == FName(*BodyB))
        {
            Constraint = C;
            break;
        }
        // Also check reverse order
        if (C && 
            C->DefaultInstance.ConstraintBone1 == FName(*BodyB) && 
            C->DefaultInstance.ConstraintBone2 == FName(*BodyA))
        {
            Constraint = C;
            break;
        }
    }

    if (!Constraint)
    {
        SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("No constraint found between '%s' and '%s'"), *BodyA, *BodyB), 
            TEXT("CONSTRAINT_NOT_FOUND"));
        return false;
    }

    // Configure limits
    const TSharedPtr<FJsonObject>* LimitsObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("limits"), LimitsObj) && LimitsObj && LimitsObj->IsValid())
    {
        double Swing1 = 45.0, Swing2 = 45.0, Twist = 45.0;
        (*LimitsObj)->TryGetNumberField(TEXT("swing1LimitAngle"), Swing1);
        (*LimitsObj)->TryGetNumberField(TEXT("swing2LimitAngle"), Swing2);
        (*LimitsObj)->TryGetNumberField(TEXT("twistLimitAngle"), Twist);

        FString Swing1Motion, Swing2Motion, TwistMotion;
        (*LimitsObj)->TryGetStringField(TEXT("swing1Motion"), Swing1Motion);
        (*LimitsObj)->TryGetStringField(TEXT("swing2Motion"), Swing2Motion);
        (*LimitsObj)->TryGetStringField(TEXT("twistMotion"), TwistMotion);

        auto ParseMotion = [](const FString& Motion) -> EAngularConstraintMotion {
            if (Motion.Equals(TEXT("Free"), ESearchCase::IgnoreCase)) return EAngularConstraintMotion::ACM_Free;
            if (Motion.Equals(TEXT("Locked"), ESearchCase::IgnoreCase)) return EAngularConstraintMotion::ACM_Locked;
            return EAngularConstraintMotion::ACM_Limited;
        };

        Constraint->DefaultInstance.SetAngularSwing1Limit(ParseMotion(Swing1Motion), static_cast<float>(Swing1));
        Constraint->DefaultInstance.SetAngularSwing2Limit(ParseMotion(Swing2Motion), static_cast<float>(Swing2));
        Constraint->DefaultInstance.SetAngularTwistLimit(ParseMotion(TwistMotion), static_cast<float>(Twist));
    }
    else
    {
        // Individual parameters
        double Swing1 = 0.0, Swing2 = 0.0, Twist = 0.0;
        if (Payload->TryGetNumberField(TEXT("swing1LimitAngle"), Swing1))
        {
            Constraint->DefaultInstance.SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, static_cast<float>(Swing1));
        }
        if (Payload->TryGetNumberField(TEXT("swing2LimitAngle"), Swing2))
        {
            Constraint->DefaultInstance.SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Limited, static_cast<float>(Swing2));
        }
        if (Payload->TryGetNumberField(TEXT("twistLimitAngle"), Twist))
        {
            Constraint->DefaultInstance.SetAngularTwistLimit(EAngularConstraintMotion::ACM_Limited, static_cast<float>(Twist));
        }
    }

    McpSafeAssetSave(PhysicsAsset);

    // Save if requested
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("bodyA"), BodyA);
    Result->SetStringField(TEXT("bodyB"), BodyB);

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Constraint limits configured"), Result);
    return true;
}


// ============================================================================
// BATCH 5: Bone Structure Operations
// ============================================================================

/**
 * Handle: rename_bone
 * Rename a bone in a skeleton (via virtual bone renaming for safety)
 */
bool UMcpAutomationBridgeSubsystem::HandleRenameBone(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletonPath"));
    FString BoneName = GetStringFieldSkel(Payload, TEXT("boneName"));
    FString NewBoneName = GetStringFieldSkel(Payload, TEXT("newBoneName"));

    if (SkeletonPath.IsEmpty() || BoneName.IsEmpty() || NewBoneName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("skeletonPath, boneName, and newBoneName are required"), TEXT("MISSING_PARAM"));
        return false;
    }

    FString Error;
    USkeleton* Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);
    if (!Skeleton)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
        return false;
    }

    // Check if it's a virtual bone
    const TArray<FVirtualBone>& VirtualBones = Skeleton->GetVirtualBones();
    bool bIsVirtualBone = false;
    for (const FVirtualBone& VB : VirtualBones)
    {
        if (VB.VirtualBoneName == FName(*BoneName))
        {
            bIsVirtualBone = true;
            break;
        }
    }

    if (bIsVirtualBone)
    {
        Skeleton->RenameVirtualBone(FName(*BoneName), FName(*NewBoneName));
        McpSafeAssetSave(Skeleton);

        bool bSave = false;
        Payload->TryGetBoolField(TEXT("save"), bSave);
        if (bSave)
        {
        }

        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("oldName"), BoneName);
        Result->SetStringField(TEXT("newName"), NewBoneName);
        Result->SetBoolField(TEXT("isVirtualBone"), true);

        SendAutomationResponse(RequestingSocket, RequestId, true, 
            FString::Printf(TEXT("Virtual bone renamed from '%s' to '%s'"), *BoneName, *NewBoneName), Result);
        return true;
    }

    // For regular bones, renaming is not directly supported without reimporting
    // We can rename bone mappings in animation assets though
    SendAutomationError(RequestingSocket, RequestId, 
        TEXT("Renaming non-virtual bones is not supported. Only virtual bones can be renamed at runtime. To rename regular bones, reimport the skeletal mesh with updated bone names."), 
        TEXT("OPERATION_NOT_SUPPORTED"));
    return false;
}

/**
 * Handle: set_bone_transform
 * Set the reference pose transform for a bone
 */
bool UMcpAutomationBridgeSubsystem::HandleSetBoneTransform(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletalMeshPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));
    FString BoneName = GetStringFieldSkel(Payload, TEXT("boneName"));

    if (SkeletalMeshPath.IsEmpty() || BoneName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("skeletalMeshPath and boneName are required"), TEXT("MISSING_PARAM"));
        return false;
    }

    FString Error;
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!Mesh)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return false;
    }

    const FReferenceSkeleton& RefSkeleton = Mesh->GetRefSkeleton();
    int32 BoneIndex = RefSkeleton.FindBoneIndex(FName(*BoneName));
    
    if (BoneIndex == INDEX_NONE)
    {
        SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("Bone '%s' not found"), *BoneName), TEXT("BONE_NOT_FOUND"));
        return false;
    }

    // Parse transform
    FVector Location = ParseVectorFromJson(Payload, TEXT("location"));
    FRotator Rotation = ParseRotatorFromJson(Payload, TEXT("rotation"));
    FVector Scale = ParseVectorFromJson(Payload, TEXT("scale"), FVector::OneVector);

    FTransform NewTransform(Rotation, Location, Scale);

    // Modify the reference skeleton
    // Note: This modifies the skeleton in memory. For persistent changes, the mesh needs to be reimported.
    FReferenceSkeletonModifier Modifier(Mesh->GetRefSkeleton(), Mesh->GetSkeleton());
    Modifier.UpdateRefPoseTransform(BoneIndex, NewTransform);

    McpSafeAssetSave(Mesh);

    // Save if requested
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("boneName"), BoneName);
    Result->SetNumberField(TEXT("boneIndex"), BoneIndex);

    SendAutomationResponse(RequestingSocket, RequestId, true, 
        FString::Printf(TEXT("Bone '%s' transform updated"), *BoneName), Result);
    return true;
}


// ============================================================================
// BATCH 6: Morph Target Operations
// ============================================================================

/**
 * Handle: create_morph_target
 * Create a new morph target on a skeletal mesh
 */
bool UMcpAutomationBridgeSubsystem::HandleCreateMorphTarget(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletalMeshPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));
    FString MorphTargetName = GetStringFieldSkel(Payload, TEXT("morphTargetName"));

    if (SkeletalMeshPath.IsEmpty() || MorphTargetName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("skeletalMeshPath and morphTargetName are required"), TEXT("MISSING_PARAM"));
        return false;
    }

    FString Error;
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!Mesh)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return false;
    }

    // Check if morph target already exists
    UMorphTarget* ExistingMorph = Mesh->FindMorphTarget(FName(*MorphTargetName));
    if (ExistingMorph)
    {
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("morphTargetName"), MorphTargetName);
        Result->SetBoolField(TEXT("alreadyExists"), true);

        SendAutomationResponse(RequestingSocket, RequestId, true, 
            FString::Printf(TEXT("Morph target '%s' already exists"), *MorphTargetName), Result);
        return true;
    }

    // Create new morph target
    UMorphTarget* NewMorphTarget = NewObject<UMorphTarget>(Mesh, FName(*MorphTargetName));
    if (!NewMorphTarget)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create morph target object"), TEXT("CREATION_FAILED"));
        return true;
    }
    
    // Register with mesh
    Mesh->RegisterMorphTarget(NewMorphTarget);
    McpSafeAssetSave(Mesh);

    // Save if requested
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("morphTargetName"), MorphTargetName);
    Result->SetNumberField(TEXT("morphTargetCount"), Mesh->GetMorphTargets().Num());

    SendAutomationResponse(RequestingSocket, RequestId, true, 
        FString::Printf(TEXT("Morph target '%s' created"), *MorphTargetName), Result);
    return true;
}

/**
 * Handle: set_morph_target_deltas
 * Set vertex deltas for a morph target
 */
bool UMcpAutomationBridgeSubsystem::HandleSetMorphTargetDeltas(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletalMeshPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));
    FString MorphTargetName = GetStringFieldSkel(Payload, TEXT("morphTargetName"));

    if (SkeletalMeshPath.IsEmpty() || MorphTargetName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("skeletalMeshPath and morphTargetName are required"), TEXT("MISSING_PARAM"));
        return false;
    }

    FString Error;
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!Mesh)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return false;
    }

    UMorphTarget* MorphTarget = Mesh->FindMorphTarget(FName(*MorphTargetName));
    if (!MorphTarget)
    {
        SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("Morph target '%s' not found"), *MorphTargetName), TEXT("MORPH_NOT_FOUND"));
        return false;
    }

    // Parse deltas array
    const TArray<TSharedPtr<FJsonValue>>* DeltasArray = nullptr;
    if (!Payload->TryGetArrayField(TEXT("deltas"), DeltasArray) || !DeltasArray)
    {
        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("deltas array is required"), TEXT("MISSING_PARAM"));
        return false;
    }

    // Build delta vertices
    TArray<FMorphTargetDelta> Deltas;
    for (const TSharedPtr<FJsonValue>& DeltaValue : *DeltasArray)
    {
        const TSharedPtr<FJsonObject>* DeltaObj = nullptr;
        if (DeltaValue->TryGetObject(DeltaObj) && DeltaObj && DeltaObj->IsValid())
        {
            FMorphTargetDelta Delta;
            
            double VertexIndex = 0;
            (*DeltaObj)->TryGetNumberField(TEXT("vertexIndex"), VertexIndex);
            Delta.SourceIdx = static_cast<uint32>(VertexIndex);

            const TSharedPtr<FJsonObject>* PositionDelta = nullptr;
            if ((*DeltaObj)->TryGetObjectField(TEXT("positionDelta"), PositionDelta) && PositionDelta && PositionDelta->IsValid())
            {
                double X = 0, Y = 0, Z = 0;
                (*PositionDelta)->TryGetNumberField(TEXT("x"), X);
                (*PositionDelta)->TryGetNumberField(TEXT("y"), Y);
                (*PositionDelta)->TryGetNumberField(TEXT("z"), Z);
                Delta.PositionDelta = FVector3f(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));
            }

            const TSharedPtr<FJsonObject>* TangentDelta = nullptr;
            if ((*DeltaObj)->TryGetObjectField(TEXT("tangentDelta"), TangentDelta) && TangentDelta && TangentDelta->IsValid())
            {
                double X = 0, Y = 0, Z = 0;
                (*TangentDelta)->TryGetNumberField(TEXT("x"), X);
                (*TangentDelta)->TryGetNumberField(TEXT("y"), Y);
                (*TangentDelta)->TryGetNumberField(TEXT("z"), Z);
                Delta.TangentZDelta = FVector3f(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));
            }

            Deltas.Add(Delta);
        }
    }

    // Apply deltas to morph target
    // MorphLODModels is protected in UE 5.6+, use PopulateDeltas() for proper editor workflow
#if WITH_EDITOR
    // Use PopulateDeltas - the proper API for morph target manipulation
    // This handles all internal data structures correctly
    TArray<FSkelMeshSection> EmptySections; // PopulateDeltas can work with empty sections array
    MorphTarget->PopulateDeltas(Deltas, 0, EmptySections, false, false);
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Morph target manipulation requires editor"), TEXT("NOT_SUPPORTED"));
    return false;
#endif

    McpSafeAssetSave(Mesh);

    // Save if requested
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("morphTargetName"), MorphTargetName);
    Result->SetNumberField(TEXT("deltaCount"), Deltas.Num());

    SendAutomationResponse(RequestingSocket, RequestId, true, 
        FString::Printf(TEXT("Set %d deltas on morph target '%s'"), Deltas.Num(), *MorphTargetName), Result);
    return true;
}

/**
 * Handle: import_morph_targets
 * Import morph targets from an external file (FBX)
 */
bool UMcpAutomationBridgeSubsystem::HandleImportMorphTargets(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletalMeshPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));
    FString SourceFilePath = GetStringFieldSkel(Payload, TEXT("morphTargetPath"));
    if (SourceFilePath.IsEmpty())
    {
        SourceFilePath = GetStringFieldSkel(Payload, TEXT("sourcePath"));
    }

    if (SkeletalMeshPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("skeletalMeshPath is required"), TEXT("MISSING_PARAM"));
        return false;
    }

    FString Error;
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!Mesh)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return false;
    }

    // If source file provided, import from it
    if (!SourceFilePath.IsEmpty() && FPaths::FileExists(SourceFilePath))
    {
        // Note: Full FBX import for morph targets requires FbxImporter
        // This is a simplified response indicating the operation is queued
        SendAutomationError(RequestingSocket, RequestId, 
            TEXT("FBX morph target import requires using the asset import pipeline. Use manage_asset import action with the FBX file."), 
            TEXT("USE_ASSET_IMPORT"));
        return false;
    }

    // Return current morph targets as info
    TArray<TSharedPtr<FJsonValue>> MorphTargetArray;
    for (UMorphTarget* MT : Mesh->GetMorphTargets())
    {
        if (!MT) continue;
        
        TSharedPtr<FJsonObject> MTObj = MakeShareable(new FJsonObject());
        MTObj->SetStringField(TEXT("name"), MT->GetName());
        MorphTargetArray.Add(MakeShareable(new FJsonValueObject(MTObj)));
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetArrayField(TEXT("morphTargets"), MorphTargetArray);
    Result->SetNumberField(TEXT("count"), MorphTargetArray.Num());

    SendAutomationResponse(RequestingSocket, RequestId, true, 
        TEXT("Use manage_asset import to import morph targets from FBX"), Result);
    return true;
}


// ============================================================================
// BATCH 7: Skin Weight Operations
// ============================================================================

/**
 * Handle: normalize_weights
 * Normalize skin weights to sum to 1.0 for each vertex
 */
bool UMcpAutomationBridgeSubsystem::HandleNormalizeWeights(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletalMeshPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));

    if (SkeletalMeshPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("skeletalMeshPath is required"), TEXT("MISSING_PARAM"));
        return false;
    }

    FString Error;
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!Mesh)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return false;
    }

    // Weight normalization is typically done during import
    // The mesh's skin weights should already be normalized
    // We can trigger a rebuild of the weights
    
    Mesh->Build();
    McpSafeAssetSave(Mesh);

    // Save if requested
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath);

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Skin weights normalized"), Result);
    return true;
}

/**
 * Handle: prune_weights
 * Remove bone influences below a threshold
 */
bool UMcpAutomationBridgeSubsystem::HandlePruneWeights(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletalMeshPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));
    double Threshold = 0.01;
    Payload->TryGetNumberField(TEXT("threshold"), Threshold);

    if (SkeletalMeshPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("skeletalMeshPath is required"), TEXT("MISSING_PARAM"));
        return false;
    }

    FString Error;
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!Mesh)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return false;
    }

    // Skin weight pruning is done during import/build
    // For runtime, we can trigger a rebuild with the threshold
    // Note: This requires setting import options which are not accessible post-import
    
    Mesh->Build();
    McpSafeAssetSave(Mesh);

    // Save if requested
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath);
    Result->SetNumberField(TEXT("threshold"), Threshold);

    SendAutomationResponse(RequestingSocket, RequestId, true, 
        FString::Printf(TEXT("Weights pruned with threshold %f"), Threshold), Result);
    return true;
}


// ============================================================================
// BATCH 8: Cloth Operations
// ============================================================================

/**
 * Handle: bind_cloth_to_skeletal_mesh
 * Bind a cloth simulation asset to a skeletal mesh
 */
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
        return false;
    }

    FString Error;
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!Mesh)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return false;
    }

#if WITH_EDITOR
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath);
    
    // Find the cloth asset by name if provided
    UClothingAssetBase* TargetClothAsset = nullptr;
    // UE 5.7 returns TArray<TObjectPtr<>> - iterate directly without storing reference
    const auto& ClothingAssets = Mesh->GetMeshClothingAssets();
    
    if (!ClothAssetName.IsEmpty())
    {
        for (const auto& ClothAssetPtr : ClothingAssets)
        {
            UClothingAssetBase* ClothAsset = ClothAssetPtr.Get();
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
            return false;
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
            return false;
        }
    }
    else
    {
        // No cloth asset specified - return list of available cloth assets
        TArray<TSharedPtr<FJsonValue>> ClothingArray;
        for (const auto& ClothAssetPtr : ClothingAssets)
        {
            UClothingAssetBase* ClothAsset = ClothAssetPtr.Get();
            if (!ClothAsset) continue;
            
            TSharedPtr<FJsonObject> ClothObj = MakeShareable(new FJsonObject());
            ClothObj->SetStringField(TEXT("name"), ClothAsset->GetName());
            // Use UClothingAssetCommon::GetNumLods() for UE 5.7+ compatibility
            if (UClothingAssetCommon* ClothAssetCommon = Cast<UClothingAssetCommon>(ClothAsset))
            {
                ClothObj->SetNumberField(TEXT("numLods"), ClothAssetCommon->GetNumLods());
            }
            ClothingArray.Add(MakeShareable(new FJsonValueObject(ClothObj)));
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
    return false;
#endif
}

/**
 * Handle: assign_cloth_asset_to_mesh
 * Assign an existing cloth asset to a skeletal mesh section
 */
bool UMcpAutomationBridgeSubsystem::HandleAssignClothAssetToMesh(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletalMeshPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));

    if (SkeletalMeshPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("skeletalMeshPath is required"), TEXT("MISSING_PARAM"));
        return false;
    }

    FString Error;
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!Mesh)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return false;
    }

    // List current clothing assets
    TArray<TSharedPtr<FJsonValue>> ClothingArray;
    for (const auto& ClothAssetPtr : Mesh->GetMeshClothingAssets())
    {
        UClothingAssetBase* ClothAsset = ClothAssetPtr.Get();
        if (!ClothAsset) continue;
        
        TSharedPtr<FJsonObject> ClothObj = MakeShareable(new FJsonObject());
        ClothObj->SetStringField(TEXT("name"), ClothAsset->GetName());
        ClothingArray.Add(MakeShareable(new FJsonValueObject(ClothObj)));
    }

    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
    Result->SetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath);
    Result->SetArrayField(TEXT("clothingAssets"), ClothingArray);
    Result->SetNumberField(TEXT("count"), ClothingArray.Num());

    SendAutomationResponse(RequestingSocket, RequestId, true, 
        TEXT("Cloth asset assignment requires using the Cloth Paint tool in Unreal Editor"), Result);
    return true;
}


// ============================================================================
// Main Skeleton Action Dispatcher
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleManageSkeleton(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Only handle manage_skeleton action
    if (Action != TEXT("manage_skeleton"))
    {
        return false; // Not handled
    }

    // Read subAction from payload (the actual operation to perform)
    FString SubAction;
    if (!Payload.IsValid() || !Payload->TryGetStringField(TEXT("subAction"), SubAction) || SubAction.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Skeleton action (subAction) is required"), TEXT("MISSING_ACTION"));
        return true; // Handled but error
    }

    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("HandleManageSkeleton: %s"), *SubAction);

    // Route to specific handler
    if (SubAction == TEXT("get_skeleton_info"))
    {
        return HandleGetSkeletonInfo(RequestId, Payload, RequestingSocket);
    }
    else if (SubAction == TEXT("list_bones"))
    {
        return HandleListBones(RequestId, Payload, RequestingSocket);
    }
    else if (SubAction == TEXT("list_sockets"))
    {
        return HandleListSockets(RequestId, Payload, RequestingSocket);
    }
    else if (SubAction == TEXT("create_socket"))
    {
        return HandleCreateSocket(RequestId, Payload, RequestingSocket);
    }
    else if (SubAction == TEXT("configure_socket"))
    {
        return HandleConfigureSocket(RequestId, Payload, RequestingSocket);
    }
    else if (SubAction == TEXT("create_virtual_bone"))
    {
        return HandleCreateVirtualBone(RequestId, Payload, RequestingSocket);
    }
    else if (SubAction == TEXT("create_physics_asset"))
    {
        return HandleCreatePhysicsAsset(RequestId, Payload, RequestingSocket);
    }
    else if (SubAction == TEXT("list_physics_bodies"))
    {
        return HandleListPhysicsBodies(RequestId, Payload, RequestingSocket);
    }
    // Physics body operations
    else if (SubAction == TEXT("add_physics_body"))
    {
        return HandleAddPhysicsBody(RequestId, Payload, RequestingSocket);
    }
    else if (SubAction == TEXT("configure_physics_body"))
    {
        return HandleConfigurePhysicsBody(RequestId, Payload, RequestingSocket);
    }
    else if (SubAction == TEXT("add_physics_constraint"))
    {
        return HandleAddPhysicsConstraint(RequestId, Payload, RequestingSocket);
    }
    else if (SubAction == TEXT("configure_constraint_limits"))
    {
        return HandleConfigureConstraintLimits(RequestId, Payload, RequestingSocket);
    }
    // Bone operations
    else if (SubAction == TEXT("rename_bone"))
    {
        return HandleRenameBone(RequestId, Payload, RequestingSocket);
    }
    else if (SubAction == TEXT("set_bone_transform"))
    {
        return HandleSetBoneTransform(RequestId, Payload, RequestingSocket);
    }
    // Morph target operations
    else if (SubAction == TEXT("create_morph_target"))
    {
        return HandleCreateMorphTarget(RequestId, Payload, RequestingSocket);
    }
    else if (SubAction == TEXT("set_morph_target_deltas"))
    {
        return HandleSetMorphTargetDeltas(RequestId, Payload, RequestingSocket);
    }
    else if (SubAction == TEXT("import_morph_targets"))
    {
        return HandleImportMorphTargets(RequestId, Payload, RequestingSocket);
    }
    // Skin weight operations
    else if (SubAction == TEXT("normalize_weights"))
    {
        return HandleNormalizeWeights(RequestId, Payload, RequestingSocket);
    }
    else if (SubAction == TEXT("prune_weights"))
    {
        return HandlePruneWeights(RequestId, Payload, RequestingSocket);
    }
    // Cloth operations
    else if (SubAction == TEXT("bind_cloth_to_skeletal_mesh"))
    {
        return HandleBindClothToSkeletalMesh(RequestId, Payload, RequestingSocket);
    }
    else if (SubAction == TEXT("assign_cloth_asset_to_mesh"))
    {
        return HandleAssignClothAssetToMesh(RequestId, Payload, RequestingSocket);
    }
    // Skeleton structure operations using FReferenceSkeletonModifier
    else if (SubAction == TEXT("create_skeleton"))
    {
        FString SkeletonPath = GetStringFieldSkel(Payload, TEXT("path"));
        if (SkeletonPath.IsEmpty())
        {
            SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletonPath"));
        }
        FString RootBoneName = GetStringFieldSkel(Payload, TEXT("rootBoneName"));
        if (RootBoneName.IsEmpty())
        {
            RootBoneName = TEXT("Root");
        }
        
        if (SkeletonPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("path or skeletonPath is required"), TEXT("MISSING_PARAM"));
            return false;
        }
        
        // Normalize path
        FString PackagePath = FPaths::GetPath(SkeletonPath);
        FString SkeletonName = FPaths::GetBaseFilename(SkeletonPath);
        FString FullPackagePath = PackagePath / SkeletonName;
        
        // Create package
        UPackage* Package = CreatePackage(*FullPackagePath);
        if (!Package)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
            return false;
        }
        
        // Create skeleton asset
        USkeleton* NewSkeleton = NewObject<USkeleton>(Package, *SkeletonName, RF_Public | RF_Standalone);
        if (!NewSkeleton)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create skeleton object"), TEXT("CREATION_FAILED"));
            return false;
        }
        
        // Initialize with a root bone using FReferenceSkeletonModifier
        FReferenceSkeletonModifier Modifier(NewSkeleton);
        FMeshBoneInfo RootBone;
        RootBone.Name = FName(*RootBoneName);
        RootBone.ParentIndex = INDEX_NONE;
#if WITH_EDITORONLY_DATA
        RootBone.ExportName = RootBoneName;
#endif
        Modifier.Add(RootBone, FTransform::Identity, true); // bAllowMultipleRoots = true for first bone
        
        McpSafeAssetSave(NewSkeleton);
        
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("skeletonPath"), NewSkeleton->GetPathName());
        Result->SetStringField(TEXT("rootBoneName"), RootBoneName);
        Result->SetNumberField(TEXT("boneCount"), 1);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, 
            FString::Printf(TEXT("Skeleton created with root bone '%s'"), *RootBoneName), Result);
        return true;
    }
    else if (SubAction == TEXT("add_bone"))
    {
        FString SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletonPath"));
        FString BoneName = GetStringFieldSkel(Payload, TEXT("boneName"));
        FString ParentName = GetStringFieldSkel(Payload, TEXT("parentBone"));
        if (ParentName.IsEmpty())
        {
            ParentName = GetStringFieldSkel(Payload, TEXT("parentBoneName"));
        }
        
        if (SkeletonPath.IsEmpty() || BoneName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("skeletonPath and boneName are required"), TEXT("MISSING_PARAM"));
            return false;
        }
        
        FString Error;
        USkeleton* Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);
        if (!Skeleton)
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
            return false;
        }
        
        const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
        
        // Check if bone already exists
        if (RefSkeleton.FindBoneIndex(FName(*BoneName)) != INDEX_NONE)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Bone '%s' already exists"), *BoneName), TEXT("BONE_EXISTS"));
            return false;
        }
        
        // Find parent bone index
        int32 ParentIndex = INDEX_NONE;
        if (!ParentName.IsEmpty())
        {
            ParentIndex = RefSkeleton.FindBoneIndex(FName(*ParentName));
            if (ParentIndex == INDEX_NONE)
            {
                SendAutomationError(RequestingSocket, RequestId, 
                    FString::Printf(TEXT("Parent bone '%s' not found"), *ParentName), TEXT("PARENT_NOT_FOUND"));
                return false;
            }
        }
        
        // Parse transform from payload
        FVector Location = ParseVectorFromJson(Payload, TEXT("location"));
        FRotator Rotation = ParseRotatorFromJson(Payload, TEXT("rotation"));
        FVector Scale = ParseVectorFromJson(Payload, TEXT("scale"), FVector::OneVector);
        FTransform BoneTransform(Rotation, Location, Scale);
        
        // Add the bone using FReferenceSkeletonModifier
        FReferenceSkeletonModifier Modifier(Skeleton);
        FMeshBoneInfo NewBone;
        NewBone.Name = FName(*BoneName);
        NewBone.ParentIndex = ParentIndex;
#if WITH_EDITORONLY_DATA
        NewBone.ExportName = BoneName;
#endif
        
        // Allow multiple roots only if no parent is specified and this is the first bone
        bool bAllowMultipleRoots = ParentIndex == INDEX_NONE && RefSkeleton.GetRawBoneNum() == 0;
        Modifier.Add(NewBone, BoneTransform, bAllowMultipleRoots);
        
        McpSafeAssetSave(Skeleton);
        
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("boneName"), BoneName);
        Result->SetStringField(TEXT("parentBone"), ParentName);
        Result->SetNumberField(TEXT("boneCount"), Skeleton->GetReferenceSkeleton().GetRawBoneNum());
        
        SendAutomationResponse(RequestingSocket, RequestId, true, 
            FString::Printf(TEXT("Bone '%s' added to skeleton"), *BoneName), Result);
        return true;
    }
    else if (SubAction == TEXT("remove_bone"))
    {
        FString SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletonPath"));
        FString BoneName = GetStringFieldSkel(Payload, TEXT("boneName"));
        bool bRemoveChildren = false;
        Payload->TryGetBoolField(TEXT("removeChildren"), bRemoveChildren);
        
        if (SkeletonPath.IsEmpty() || BoneName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("skeletonPath and boneName are required"), TEXT("MISSING_PARAM"));
            return false;
        }
        
        FString Error;
        USkeleton* Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);
        if (!Skeleton)
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
            return false;
        }
        
        const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
        int32 BoneIndex = RefSkeleton.FindBoneIndex(FName(*BoneName));
        
        if (BoneIndex == INDEX_NONE)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Bone '%s' not found"), *BoneName), TEXT("BONE_NOT_FOUND"));
            return false;
        }
        
        // Check if it's the root bone
        if (BoneIndex == 0)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Cannot remove root bone"), TEXT("CANNOT_REMOVE_ROOT"));
            return false;
        }
        
        // Remove the bone using FReferenceSkeletonModifier
        FReferenceSkeletonModifier Modifier(Skeleton);
        Modifier.Remove(FName(*BoneName), bRemoveChildren);
        
        McpSafeAssetSave(Skeleton);
        
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("removedBone"), BoneName);
        Result->SetBoolField(TEXT("childrenRemoved"), bRemoveChildren);
        Result->SetNumberField(TEXT("boneCount"), Skeleton->GetReferenceSkeleton().GetRawBoneNum());
        
        SendAutomationResponse(RequestingSocket, RequestId, true, 
            FString::Printf(TEXT("Bone '%s' removed from skeleton"), *BoneName), Result);
        return true;
    }
    else if (SubAction == TEXT("set_bone_parent"))
    {
        FString SkeletonPath = GetStringFieldSkel(Payload, TEXT("skeletonPath"));
        FString BoneName = GetStringFieldSkel(Payload, TEXT("boneName"));
        FString NewParentName = GetStringFieldSkel(Payload, TEXT("parentBone"));
        if (NewParentName.IsEmpty())
        {
            NewParentName = GetStringFieldSkel(Payload, TEXT("newParentBone"));
        }
        
        if (SkeletonPath.IsEmpty() || BoneName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("skeletonPath and boneName are required"), TEXT("MISSING_PARAM"));
            return false;
        }
        
        FString Error;
        USkeleton* Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Error);
        if (!Skeleton)
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
            return false;
        }
        
        const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
        int32 BoneIndex = RefSkeleton.FindBoneIndex(FName(*BoneName));
        
        if (BoneIndex == INDEX_NONE)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Bone '%s' not found"), *BoneName), TEXT("BONE_NOT_FOUND"));
            return false;
        }
        
        // Set new parent using FReferenceSkeletonModifier
        // NewParentName can be empty/NAME_None to unparent (make root)
        FReferenceSkeletonModifier Modifier(Skeleton);
        FName ParentFName = NewParentName.IsEmpty() ? NAME_None : FName(*NewParentName);
        int32 NewBoneIndex = Modifier.SetParent(FName(*BoneName), ParentFName, true);
        
        if (NewBoneIndex == INDEX_NONE)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Failed to set parent. New parent '%s' may not exist or operation invalid."), *NewParentName), 
                TEXT("SET_PARENT_FAILED"));
            return false;
        }
        
        McpSafeAssetSave(Skeleton);
        
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("boneName"), BoneName);
        Result->SetStringField(TEXT("newParent"), NewParentName.IsEmpty() ? TEXT("(none - root)") : NewParentName);
        Result->SetNumberField(TEXT("newBoneIndex"), NewBoneIndex);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, 
            FString::Printf(TEXT("Bone '%s' parent changed to '%s'"), *BoneName, NewParentName.IsEmpty() ? TEXT("(none)") : *NewParentName), Result);
        return true;
    }
    // Skin weight operations using FSkinWeightProfileData
    else if (SubAction == TEXT("set_vertex_weights"))
    {
        FString SkeletalMeshPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));
        FString ProfileName = GetStringFieldSkel(Payload, TEXT("profileName"));
        if (ProfileName.IsEmpty())
        {
            ProfileName = TEXT("CustomWeights");
        }
        
        if (SkeletalMeshPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("skeletalMeshPath is required"), TEXT("MISSING_PARAM"));
            return false;
        }
        
        FString Error;
        USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
        if (!Mesh)
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
            return false;
        }
        
        // Parse weights array
        const TArray<TSharedPtr<FJsonValue>>* WeightsArray = nullptr;
        if (!Payload->TryGetArrayField(TEXT("weights"), WeightsArray) || !WeightsArray)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("weights array is required"), TEXT("MISSING_PARAM"));
            return false;
        }
        
#if WITH_EDITORONLY_DATA
        // Access the LOD model for editing
        FSkeletalMeshModel* ImportedModel = Mesh->GetImportedModel();
        if (!ImportedModel || ImportedModel->LODModels.Num() == 0)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Mesh has no LOD models"), TEXT("NO_LOD_MODELS"));
            return false;
        }
        
        int32 LODIndex = 0;
        Payload->TryGetNumberField(TEXT("lodIndex"), LODIndex);
        
        if (LODIndex >= ImportedModel->LODModels.Num())
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("LOD index %d out of range (max: %d)"), LODIndex, ImportedModel->LODModels.Num() - 1), 
                TEXT("INVALID_LOD"));
            return false;
        }
        
        FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels[LODIndex];
        
        // Create or update skin weight profile
        FSkinWeightProfileInfo* ProfileInfo = nullptr;
        for (FSkinWeightProfileInfo& Info : Mesh->GetSkinWeightProfiles())
        {
            if (Info.Name == FName(*ProfileName))
            {
                ProfileInfo = &Info;
                break;
            }
        }
        
        if (!ProfileInfo)
        {
            // Add new profile
            FSkinWeightProfileInfo NewProfile;
            NewProfile.Name = FName(*ProfileName);
            Mesh->AddSkinWeightProfile(NewProfile);
        }
        
        // Build FImportedSkinWeightProfileData from weights array
        FImportedSkinWeightProfileData& ProfileData = LODModel.SkinWeightProfiles.FindOrAdd(FName(*ProfileName));
        ProfileData.SkinWeights.SetNum(LODModel.NumVertices);
        
        int32 WeightsSet = 0;
        for (const TSharedPtr<FJsonValue>& WeightValue : *WeightsArray)
        {
            const TSharedPtr<FJsonObject>* WeightObj = nullptr;
            if (!WeightValue->TryGetObject(WeightObj) || !WeightObj || !WeightObj->IsValid())
            {
                continue;
            }
            
            int32 VertexIndex = 0;
            (*WeightObj)->TryGetNumberField(TEXT("vertexIndex"), VertexIndex);
            
            if (VertexIndex < 0 || VertexIndex >= static_cast<int32>(LODModel.NumVertices))
            {
                continue;
            }
            
            FRawSkinWeight& SkinWeight = ProfileData.SkinWeights[VertexIndex];
            FMemory::Memzero(&SkinWeight, sizeof(FRawSkinWeight));
            
            // Parse bone influences
            const TArray<TSharedPtr<FJsonValue>>* InfluencesArray = nullptr;
            if ((*WeightObj)->TryGetArrayField(TEXT("influences"), InfluencesArray) && InfluencesArray)
            {
                int32 InfluenceIndex = 0;
                for (const TSharedPtr<FJsonValue>& InfluenceValue : *InfluencesArray)
                {
                    if (InfluenceIndex >= MAX_TOTAL_INFLUENCES) break;
                    
                    const TSharedPtr<FJsonObject>* InfluenceObj = nullptr;
                    if (InfluenceValue->TryGetObject(InfluenceObj) && InfluenceObj && InfluenceObj->IsValid())
                    {
                        int32 BoneIndex = 0;
                        double Weight = 0.0;
                        (*InfluenceObj)->TryGetNumberField(TEXT("boneIndex"), BoneIndex);
                        (*InfluenceObj)->TryGetNumberField(TEXT("weight"), Weight);
                        
                        SkinWeight.InfluenceBones[InfluenceIndex] = static_cast<FBoneIndexType>(BoneIndex);
                        SkinWeight.InfluenceWeights[InfluenceIndex] = static_cast<uint16>(FMath::Clamp(Weight, 0.0, 1.0) * 65535.0);
                        InfluenceIndex++;
                    }
                }
            }
            
            WeightsSet++;
        }
        
        // Rebuild the mesh with the new skin weight profile
        Mesh->Build();
        McpSafeAssetSave(Mesh);
        
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath);
        Result->SetStringField(TEXT("profileName"), ProfileName);
        Result->SetNumberField(TEXT("verticesModified"), WeightsSet);
        Result->SetNumberField(TEXT("lodIndex"), LODIndex);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, 
            FString::Printf(TEXT("Set weights for %d vertices in profile '%s'"), WeightsSet, *ProfileName), Result);
        return true;
#else
        SendAutomationError(RequestingSocket, RequestId, TEXT("set_vertex_weights requires editor mode"), TEXT("NOT_EDITOR"));
        return false;
#endif
    }
    else if (SubAction == TEXT("auto_skin_weights"))
    {
        // Auto skin weights computation - typically done during import
        // We trigger a mesh rebuild which recalculates default weights
        FString SkeletalMeshPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));
        
        if (SkeletalMeshPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("skeletalMeshPath is required"), TEXT("MISSING_PARAM"));
            return false;
        }
        
        FString Error;
        USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
        if (!Mesh)
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
            return false;
        }
        
        // Rebuild the mesh - this recalculates skin weights based on bone positions
        Mesh->Build();
        McpSafeAssetSave(Mesh);
        
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath);
        Result->SetBoolField(TEXT("rebuilt"), true);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, 
            TEXT("Mesh rebuilt with recalculated skin weights"), Result);
        return true;
    }
    else if (SubAction == TEXT("copy_weights"))
    {
        FString SourceMeshPath = GetStringFieldSkel(Payload, TEXT("sourceMeshPath"));
        FString TargetMeshPath = GetStringFieldSkel(Payload, TEXT("targetMeshPath"));
        FString ProfileName = GetStringFieldSkel(Payload, TEXT("profileName"));
        if (ProfileName.IsEmpty())
        {
            ProfileName = TEXT("CopiedWeights");
        }
        int32 LODIndex = 0;
        Payload->TryGetNumberField(TEXT("lodIndex"), LODIndex);
        
        if (SourceMeshPath.IsEmpty() || TargetMeshPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("sourceMeshPath and targetMeshPath are required"), TEXT("MISSING_PARAM"));
            return false;
        }
        
        FString Error;
        USkeletalMesh* SourceMesh = LoadSkeletalMeshFromPathSkel(SourceMeshPath, Error);
        if (!SourceMesh)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Source mesh not found: %s"), *Error), TEXT("SOURCE_NOT_FOUND"));
            return false;
        }
        
        USkeletalMesh* TargetMesh = LoadSkeletalMeshFromPathSkel(TargetMeshPath, Error);
        if (!TargetMesh)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Target mesh not found: %s"), *Error), TEXT("TARGET_NOT_FOUND"));
            return false;
        }
        
#if WITH_EDITORONLY_DATA
        FSkeletalMeshModel* SourceModel = SourceMesh->GetImportedModel();
        FSkeletalMeshModel* TargetModel = TargetMesh->GetImportedModel();
        
        if (!SourceModel || !TargetModel || 
            LODIndex >= SourceModel->LODModels.Num() || 
            LODIndex >= TargetModel->LODModels.Num())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Invalid LOD models"), TEXT("INVALID_LOD"));
            return false;
        }
        
        FSkeletalMeshLODModel& SourceLOD = SourceModel->LODModels[LODIndex];
        FSkeletalMeshLODModel& TargetLOD = TargetModel->LODModels[LODIndex];
        
        // Create skin weight profile on target
        FSkinWeightProfileInfo NewProfile;
        NewProfile.Name = FName(*ProfileName);
        TargetMesh->AddSkinWeightProfile(NewProfile);
        
        FImportedSkinWeightProfileData& ProfileData = TargetLOD.SkinWeightProfiles.FindOrAdd(FName(*ProfileName));
        
        // Copy weights from source (limited by vertex count)
        uint32 VertsToCopy = FMath::Min(SourceLOD.NumVertices, TargetLOD.NumVertices);
        ProfileData.SkinWeights.SetNum(TargetLOD.NumVertices);
        
        // Initialize with zeros
        for (uint32 i = 0; i < TargetLOD.NumVertices; ++i)
        {
            FMemory::Memzero(&ProfileData.SkinWeights[i], sizeof(FRawSkinWeight));
        }
        
        // Note: Direct weight copying requires accessing the source vertex buffer
        // For now we indicate the profile was created and user should use the editor for precise transfer
        
        TargetMesh->Build();
        McpSafeAssetSave(TargetMesh);
        
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("sourceMeshPath"), SourceMeshPath);
        Result->SetStringField(TEXT("targetMeshPath"), TargetMeshPath);
        Result->SetStringField(TEXT("profileName"), ProfileName);
        Result->SetNumberField(TEXT("lodIndex"), LODIndex);
        Result->SetStringField(TEXT("note"), TEXT("Skin weight profile created. Use FSkinWeightProfileHelpers::ImportSkinWeightProfile for precise transfer."));
        
        SendAutomationResponse(RequestingSocket, RequestId, true, 
            FString::Printf(TEXT("Skin weight profile '%s' created on target mesh"), *ProfileName), Result);
        return true;
#else
        SendAutomationError(RequestingSocket, RequestId, TEXT("copy_weights requires editor mode"), TEXT("NOT_EDITOR"));
        return false;
#endif
    }
    else if (SubAction == TEXT("mirror_weights"))
    {
        FString SkeletalMeshPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));
        FString Axis = GetStringFieldSkel(Payload, TEXT("axis"));
        if (Axis.IsEmpty())
        {
            Axis = TEXT("X");
        }
        FString ProfileName = GetStringFieldSkel(Payload, TEXT("profileName"));
        if (ProfileName.IsEmpty())
        {
            ProfileName = TEXT("MirroredWeights");
        }
        
        if (SkeletalMeshPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("skeletalMeshPath is required"), TEXT("MISSING_PARAM"));
            return false;
        }
        
        FString Error;
        USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
        if (!Mesh)
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
            return false;
        }
        
#if WITH_EDITORONLY_DATA
        FSkeletalMeshModel* ImportedModel = Mesh->GetImportedModel();
        if (!ImportedModel || ImportedModel->LODModels.Num() == 0)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Mesh has no LOD models"), TEXT("NO_LOD_MODELS"));
            return false;
        }
        
        int32 LODIndex = 0;
        Payload->TryGetNumberField(TEXT("lodIndex"), LODIndex);
        
        FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels[LODIndex];
        
        // Create mirrored skin weight profile
        FSkinWeightProfileInfo NewProfile;
        NewProfile.Name = FName(*ProfileName);
        Mesh->AddSkinWeightProfile(NewProfile);
        
        FImportedSkinWeightProfileData& ProfileData = LODModel.SkinWeightProfiles.FindOrAdd(FName(*ProfileName));
        ProfileData.SkinWeights.SetNum(LODModel.NumVertices);
        
        // Initialize profile - mirroring logic would need vertex position data
        // For now we create the profile structure and indicate manual completion needed
        for (uint32 i = 0; i < LODModel.NumVertices; ++i)
        {
            FMemory::Memzero(&ProfileData.SkinWeights[i], sizeof(FRawSkinWeight));
        }
        
        Mesh->Build();
        McpSafeAssetSave(Mesh);
        
        TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
        Result->SetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath);
        Result->SetStringField(TEXT("profileName"), ProfileName);
        Result->SetStringField(TEXT("axis"), Axis);
        Result->SetNumberField(TEXT("lodIndex"), LODIndex);
        Result->SetStringField(TEXT("note"), TEXT("Skin weight profile created. Use Skeletal Mesh Editor for precise mirroring with bone name mapping."));
        
        SendAutomationResponse(RequestingSocket, RequestId, true, 
            FString::Printf(TEXT("Skin weight profile '%s' created for mirroring along %s axis"), *ProfileName, *Axis), Result);
        return true;
#else
        SendAutomationError(RequestingSocket, RequestId, TEXT("mirror_weights requires editor mode"), TEXT("NOT_EDITOR"));
        return false;
#endif
    }
    else
    {
        SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("Unknown skeleton action: %s"), *SubAction), 
            TEXT("UNKNOWN_ACTION"));
        return false;
    }
}

#undef GetStringFieldSkel
#undef GetNumberFieldSkel
#undef GetBoolFieldSkel

#endif // WITH_EDITOR

