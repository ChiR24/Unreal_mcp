#include "Domains/Skeleton/Assets/McpSkeletonAssetLoading.h"
#include "Domains/Skeleton/Assets/McpSkeletonPayload.h"

#include "Engine/SkeletalMesh.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#if __has_include("PhysicsEngine/SkeletalBodySetup.h")
#include "PhysicsEngine/SkeletalBodySetup.h"
#endif

using namespace McpSkeletonHandlers;

namespace
{
const TCHAR* PhysicsTypeToString(const USkeletalBodySetup* BodySetup)
{
    switch (BodySetup->PhysicsType.GetValue())
    {
        case EPhysicsType::PhysType_Kinematic: return TEXT("Kinematic");
        case EPhysicsType::PhysType_Simulated: return TEXT("Simulated");
        default: return TEXT("Default");
    }
}

const TCHAR* CollisionTraceFlagToString(ECollisionTraceFlag Flag)
{
    switch (Flag)
    {
        case CTF_UseSimpleAndComplex: return TEXT("SimpleAndComplex");
        case CTF_UseSimpleAsComplex: return TEXT("SimpleAsComplex");
        case CTF_UseComplexAsSimple: return TEXT("ComplexAsSimple");
        default: return TEXT("Default");
    }
}

void AddGeometryCounts(const TSharedPtr<FJsonObject>& BodyObj, const USkeletalBodySetup* BodySetup,
    const TCHAR* SphereKey, const TCHAR* BoxKey, const TCHAR* CapsuleKey, const TCHAR* ConvexKey)
{
    BodyObj->SetNumberField(SphereKey, BodySetup->AggGeom.SphereElems.Num());
    BodyObj->SetNumberField(BoxKey, BodySetup->AggGeom.BoxElems.Num());
    BodyObj->SetNumberField(CapsuleKey, BodySetup->AggGeom.SphylElems.Num());
    BodyObj->SetNumberField(ConvexKey, BodySetup->AggGeom.ConvexElems.Num());
}

// Body item of get_physics_asset_info. Its contract closes the item shape
// (additionalProperties:false), so nothing beyond these keys may be added.
TSharedPtr<FJsonObject> DescribeBodyForInfo(const USkeletalBodySetup* BodySetup)
{
    TSharedPtr<FJsonObject> BodyObj = McpHandlerUtils::CreateResultObject();
    BodyObj->SetStringField(TEXT("boneName"), BodySetup->BoneName.ToString());
    BodyObj->SetStringField(TEXT("physicsType"), PhysicsTypeToString(BodySetup));
    AddGeometryCounts(BodyObj, BodySetup, TEXT("numSpheres"), TEXT("numBoxes"), TEXT("numCapsules"), TEXT("numConvex"));
    return BodyObj;
}

// Detailed entry of list_physics_bodies (surfaces under details.bodyDetails).
TSharedPtr<FJsonObject> DescribeBodyForListing(const USkeletalBodySetup* BodySetup, int32 Index)
{
    TSharedPtr<FJsonObject> BodyObj = McpHandlerUtils::CreateResultObject();
    BodyObj->SetNumberField(TEXT("index"), Index);
    BodyObj->SetStringField(TEXT("boneName"), BodySetup->BoneName.ToString());
    AddGeometryCounts(BodyObj, BodySetup, TEXT("sphereCount"), TEXT("boxCount"), TEXT("capsuleCount"), TEXT("convexCount"));
    BodyObj->SetStringField(TEXT("physicsType"), PhysicsTypeToString(BodySetup));
    BodyObj->SetNumberField(TEXT("mass"), BodySetup->CalculateMass());
    BodyObj->SetStringField(TEXT("collisionType"), CollisionTraceFlagToString(BodySetup->CollisionTraceFlag.GetValue()));
    BodyObj->SetBoolField(TEXT("considerForBounds"), BodySetup->bConsiderForBounds);
    return BodyObj;
}

// The physics asset a read names: physicsAssetPath, else the one assigned to skeletalMeshPath. Null after the refusal
// that says which: neither sent, the asset's or the mesh's load error, or a mesh with no physics asset.
UPhysicsAsset* ResolvePhysicsAssetOrReply(UEngineWireSubsystem& Subsystem, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, const TSharedPtr<FMcpBridgeWebSocket>& Socket)
{
    FString Error;
    const FString PhysicsAssetPath = GetJsonStringField(Payload, TEXT("physicsAssetPath"));
    const FString MeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    if (!PhysicsAssetPath.IsEmpty())
    {
        UPhysicsAsset* PhysicsAsset = LoadPhysicsAssetFromPath(PhysicsAssetPath, Error);
        if (!PhysicsAsset)
        {
            Subsystem.SendAutomationError(Socket, RequestId, Error, TEXT("PHYSICS_ASSET_NOT_FOUND"));
        }
        return PhysicsAsset;
    }
    if (MeshPath.IsEmpty())
    {
        Subsystem.SendAutomationError(Socket, RequestId, TEXT("physicsAssetPath or skeletalMeshPath is required"), TEXT("MISSING_PARAM"));
        return nullptr;
    }
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(MeshPath, Error);
    if (!Mesh)
    {
        Subsystem.SendAutomationError(Socket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return nullptr;
    }
    if (!Mesh->GetPhysicsAsset())
    {
        Subsystem.SendAutomationError(Socket, RequestId, FString::Printf(TEXT("%s has no physics asset."), *MeshPath),
            TEXT("PHYSICS_ASSET_NOT_FOUND"));
    }
    return Mesh->GetPhysicsAsset();
}
} // namespace

bool UEngineWireSubsystem::HandleListPhysicsBodies(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    UPhysicsAsset* PhysicsAsset = ResolvePhysicsAssetOrReply(*this, RequestId, Payload, RequestingSocket);
    if (!PhysicsAsset)
    {
        return true;
    }

    TArray<TSharedPtr<FJsonValue>> BoneNames;
    TArray<TSharedPtr<FJsonValue>> BodyDetails;
    for (int32 Index = 0; Index < PhysicsAsset->SkeletalBodySetups.Num(); ++Index)
    {
        const USkeletalBodySetup* BodySetup = PhysicsAsset->SkeletalBodySetups[Index];
        if (!BodySetup)
        {
            continue;
        }
        BoneNames.Add(MakeShared<FJsonValueString>(BodySetup->BoneName.ToString()));
        BodyDetails.Add(MakeShared<FJsonValueObject>(DescribeBodyForListing(BodySetup, Index)));
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("physicsAssetPath"), PhysicsAsset->GetPathName());
    // The contract types `bodies` as a list of strings (bone names); the
    // per-body detail rides alongside as bodyDetails.
    Result->SetArrayField(TEXT("bodies"), BoneNames);
    Result->SetNumberField(TEXT("count"), BoneNames.Num());
    Result->SetArrayField(TEXT("bodyDetails"), BodyDetails);
    Result->SetNumberField(TEXT("constraintCount"), PhysicsAsset->ConstraintSetup.Num());

    SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Listed %d physics bodies"), BoneNames.Num()), Result);
    return true;
}

bool UEngineWireSubsystem::HandleGetPhysicsAssetInfo(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    UPhysicsAsset* PhysAsset = ResolvePhysicsAssetOrReply(*this, RequestId, Payload, RequestingSocket);
    if (!PhysAsset)
    {
        return true;
    }

    TArray<TSharedPtr<FJsonValue>> BodiesArray;
    for (USkeletalBodySetup* BodySetup : PhysAsset->SkeletalBodySetups)
    {
        if (BodySetup)
        {
            BodiesArray.Add(MakeShared<FJsonValueObject>(DescribeBodyForInfo(BodySetup)));
        }
    }

    TArray<TSharedPtr<FJsonValue>> ConstraintsArray;
    for (UPhysicsConstraintTemplate* Constraint : PhysAsset->ConstraintSetup)
    {
        if (Constraint)
        {
            TSharedPtr<FJsonObject> ConObj = McpHandlerUtils::CreateResultObject();
            const FConstraintInstance& CI = Constraint->DefaultInstance;
            ConObj->SetStringField(TEXT("name"), Constraint->GetName());
            ConObj->SetStringField(TEXT("bone1"), CI.ConstraintBone1.ToString());
            ConObj->SetStringField(TEXT("bone2"), CI.ConstraintBone2.ToString());
            ConstraintsArray.Add(MakeShared<FJsonValueObject>(ConObj));
        }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("physicsAssetPath"), PhysAsset->GetPathName());
    Result->SetStringField(TEXT("name"), PhysAsset->GetName());
    Result->SetNumberField(TEXT("numBodies"), BodiesArray.Num());
    Result->SetNumberField(TEXT("numConstraints"), ConstraintsArray.Num());
    Result->SetArrayField(TEXT("bodies"), BodiesArray);
    Result->SetArrayField(TEXT("constraints"), ConstraintsArray);

    SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Physics asset info: %d bodies, %d constraints"),
            BodiesArray.Num(), ConstraintsArray.Num()), Result);
    return true;
}

