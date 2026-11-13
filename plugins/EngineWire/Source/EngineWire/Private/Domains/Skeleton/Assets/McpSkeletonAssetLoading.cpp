#include "Domains/Skeleton/Assets/McpSkeletonAssetLoading.h"
#include "Domains/Skeleton/Assets/McpSkeletonPayload.h"

#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/BridgeHelpers/Security/McpProjectPaths.h"
#include "EngineWireSubsystem.h"
#include "PhysicsEngine/PhysicsAsset.h"

namespace McpSkeletonHandlers
{
namespace
{
// The asset of class T at Path ("<Cap> path is required" when there is none, the project's refusal when the path is
// outside it, "Failed to load <noun>" when nothing of that class is there: StaticLoadObject with a class finds only one).
template <typename T>
T* LoadSkeletonDomainAsset(const FString& Path, const TCHAR* Cap, const TCHAR* Noun, FString& OutError)
{
    OutError.Reset();
    if (Path.IsEmpty())
    {
        OutError = FString::Printf(TEXT("%s path is required"), Cap);
        return nullptr;
    }
    const FString SanitizedPath = SanitizeProjectRelativePath(Path);
    if (SanitizedPath.IsEmpty())
    {
        OutError = McpPathRefusalMessage(*FString::Printf(TEXT("%s path"), Noun), Path);
        return nullptr;
    }
    T* Asset = Cast<T>(StaticLoadObject(T::StaticClass(), nullptr, *SanitizedPath));
    if (!Asset)
    {
        OutError = FString::Printf(TEXT("Failed to load %s: %s"), Noun, *Path);
    }
    return Asset;
}

// The skeletal mesh a skeleton-addressed request can read or write: the skeleton's preview mesh, else the first
// compatible mesh the asset registry knows. Null when no mesh uses the skeleton.
USkeletalMesh* FindSkeletalMeshForSkeleton(USkeleton* Skeleton)
{
    if (!Skeleton)
    {
        return nullptr;
    }
    if (USkeletalMesh* PreviewMesh = Skeleton->GetPreviewMesh(false))
    {
        return PreviewMesh;
    }
    return Skeleton->FindCompatibleMesh();
}
}

USkeleton* LoadSkeletonFromPathSkel(const FString& SkeletonPath, FString& OutError)
{
    return LoadSkeletonDomainAsset<USkeleton>(SkeletonPath, TEXT("Skeleton"), TEXT("skeleton"), OutError);
}

USkeletalMesh* LoadSkeletalMeshFromPathSkel(const FString& MeshPath, FString& OutError)
{
    return LoadSkeletonDomainAsset<USkeletalMesh>(MeshPath, TEXT("Skeletal mesh"), TEXT("skeletal mesh"), OutError);
}

// The asset at Path loaded once as whatever it is, without a line in the log. Probing a mesh path as a Skeleton first
// logged "Failed to find object", which the reply then carried as an engine warning although the mesh was found.
static UObject* LoadAssetQuietly(const FString& Path)
{
    const FString SanitizedPath = SanitizeProjectRelativePath(Path);
    return SanitizedPath.IsEmpty() ? nullptr : StaticLoadObject(UObject::StaticClass(), nullptr, *SanitizedPath, nullptr, LOAD_NoWarn);
}

USkeleton* LoadSkeletonOrMeshSkeleton(const FString& Path, FString& OutError)
{
    if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(LoadAssetQuietly(Path)))
    {
        OutError.Reset();
        return Mesh->GetSkeleton();
    }
    return LoadSkeletonFromPathSkel(Path, OutError);
}

USkeleton* LoadPayloadSkeletonOrReply(UEngineWireSubsystem& Bridge, const FString& RequestId,
                                      TSharedPtr<FMcpBridgeWebSocket> Socket, const TSharedPtr<FJsonObject>& Payload)
{
    const FString SkeletonPath = GetJsonStringField(Payload, TEXT("skeletonPath"), GetJsonStringField(Payload, TEXT("skeletalMeshPath")));
    FString Error;
    USkeleton* Skeleton = LoadSkeletonOrMeshSkeleton(SkeletonPath, Error);
    if (!Skeleton)
    {
        Bridge.SendAutomationError(Socket, RequestId, Error, TEXT("SKELETON_NOT_FOUND"));
    }
    return Skeleton;
}

UPhysicsAsset* LoadPhysicsAssetFromPath(const FString& PhysicsPath, FString& OutError)
{
    return LoadSkeletonDomainAsset<UPhysicsAsset>(PhysicsPath, TEXT("Physics asset"), TEXT("physics asset"), OutError);
}

FSkeletonMeshTarget ResolveSkeletonMeshTarget(const TSharedPtr<FJsonObject>& Payload)
{
    FSkeletonMeshTarget Target;
    FString MeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    if (MeshPath.IsEmpty())
    {
        MeshPath = GetJsonStringField(Payload, TEXT("meshPath"));
    }
    const FString SkeletonPath = GetJsonStringField(Payload, TEXT("skeletonPath"));

    if (!MeshPath.IsEmpty())
    {
        Target.SourcePath = MeshPath;
        Target.Mesh = LoadSkeletalMeshFromPathSkel(MeshPath, Target.Error);
        if (!Target.Mesh)
        {
            Target.ErrorCode = TEXT("MESH_NOT_FOUND");
            return Target;
        }
        Target.Skeleton = Target.Mesh->GetSkeleton();
        return Target;
    }

    if (SkeletonPath.IsEmpty())
    {
        Target.Error = TEXT("skeletalMeshPath (or skeletonPath) is required");
        Target.ErrorCode = TEXT("MISSING_PARAM");
        return Target;
    }

    Target.SourcePath = SkeletonPath;
    // Older callers pass a skeletal mesh through skeletonPath: one quiet load tells which it is.
    if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(LoadAssetQuietly(SkeletonPath)))
    {
        Target.Mesh = Mesh;
        Target.Skeleton = Mesh->GetSkeleton();
        return Target;
    }
    Target.Skeleton = LoadSkeletonFromPathSkel(SkeletonPath, Target.Error);
    if (!Target.Skeleton)
    {
        Target.ErrorCode = TEXT("SKELETON_NOT_FOUND");
        return Target;
    }
    Target.Mesh = FindSkeletalMeshForSkeleton(Target.Skeleton);
    return Target;
}
}
