#include "McpAutomationBridge_SkeletonHandlersAssetLoading.h"
#include "McpAutomationBridge_SkeletonHandlersPayload.h"

#include "Engine/SkeletalMesh.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
#include "PhysicsEngine/SkeletalBodySetup.h"
#endif

#if WITH_EDITOR
using namespace McpSkeletonHandlers;

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
        return true;
    }

    FString Error;
    UPhysicsAsset* PhysicsAsset = LoadPhysicsAssetFromPath(PhysicsAssetPath, Error);
    if (!PhysicsAsset)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("PHYSICS_ASSET_NOT_FOUND"));
        return true;
    }

    TArray<TSharedPtr<FJsonValue>> BodyArray;
    for (USkeletalBodySetup* BodySetup : PhysicsAsset->SkeletalBodySetups)
    {
        if (!BodySetup) continue;

        TSharedPtr<FJsonObject> BodyObj = McpHandlerUtils::CreateResultObject();
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

        BodyArray.Add(MakeShared<FJsonValueObject>(BodyObj));
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("physicsBodies"), BodyArray);
    Result->SetNumberField(TEXT("count"), BodyArray.Num());
    Result->SetNumberField(TEXT("constraintCount"), PhysicsAsset->ConstraintSetup.Num());

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Physics bodies listed"), Result);
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleGetPhysicsAssetInfo(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    FString PhysicsAssetPath = GetStringFieldSkel(Payload, TEXT("physicsAssetPath"));
    FString SkeletalMeshPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));

    UPhysicsAsset* PhysAsset = nullptr;

    if (!PhysicsAssetPath.IsEmpty())
    {
        PhysAsset = Cast<UPhysicsAsset>(
            StaticLoadObject(UPhysicsAsset::StaticClass(), nullptr, *PhysicsAssetPath));
    }
    else if (!SkeletalMeshPath.IsEmpty())
    {
        FString Error;
        USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
        if (Mesh)
        {
            PhysAsset = Mesh->GetPhysicsAsset();
        }
    }

    if (!PhysAsset)
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Physics asset not found. Provide physicsAssetPath or skeletalMeshPath"), TEXT("NOT_FOUND"));
        return true;
    }

    // Gather physics bodies info
    TArray<TSharedPtr<FJsonValue>> BodiesArray;
    for (USkeletalBodySetup* BodySetup : PhysAsset->SkeletalBodySetups)
    {
        if (BodySetup)
        {
            TSharedPtr<FJsonObject> BodyObj = McpHandlerUtils::CreateResultObject();
            BodyObj->SetStringField(TEXT("boneName"), BodySetup->BoneName.ToString());
            BodyObj->SetStringField(TEXT("physicsType"),
                BodySetup->PhysicsType == EPhysicsType::PhysType_Kinematic ? TEXT("Kinematic") :
                BodySetup->PhysicsType == EPhysicsType::PhysType_Simulated ? TEXT("Simulated") : TEXT("Default"));
            BodyObj->SetNumberField(TEXT("numSpheres"), BodySetup->AggGeom.SphereElems.Num());
            BodyObj->SetNumberField(TEXT("numBoxes"), BodySetup->AggGeom.BoxElems.Num());
            BodyObj->SetNumberField(TEXT("numCapsules"), BodySetup->AggGeom.SphylElems.Num());
            BodyObj->SetNumberField(TEXT("numConvex"), BodySetup->AggGeom.ConvexElems.Num());
            BodiesArray.Add(MakeShared<FJsonValueObject>(BodyObj));
        }
    }

    // Gather constraints info
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
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("get_physics_asset_info requires editor mode"), TEXT("NOT_EDITOR"));
    return true;
#endif
}

#endif // WITH_EDITOR
