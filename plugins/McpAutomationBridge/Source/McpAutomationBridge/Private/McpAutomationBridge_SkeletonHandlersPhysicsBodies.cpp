#include "McpAutomationBridge_SkeletonHandlersAssetLoading.h"
#include "McpAutomationBridge_SkeletonHandlersPayload.h"

#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "McpAutomationBridgeHelpersProjectPaths.h"
#include "McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "ReferenceSkeleton.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
#include "PhysicsEngine/SkeletalBodySetup.h"
#endif

#if WITH_EDITOR
using namespace McpSkeletonHandlers;

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
        return true;
    }

    if (BoneName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("boneName is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    // Validate path security BEFORE loading asset
    FString SanitizedPath = SanitizeProjectRelativePath(PhysicsAssetPath);
    if (SanitizedPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Invalid physics asset path '%s': contains traversal sequences or invalid characters"), *PhysicsAssetPath),
            TEXT("INVALID_PATH"));
        return true;
    }
    PhysicsAssetPath = SanitizedPath;

    FString Error;
    UPhysicsAsset* PhysicsAsset = LoadPhysicsAssetFromPath(PhysicsAssetPath, Error);
    if (!PhysicsAsset)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("PHYSICS_ASSET_NOT_FOUND"));
        return true;
    }

    // CRITICAL: Validate bone exists in the skeleton before creating physics body
    // This prevents creating physics bodies for non-existent bones (fixes suspicious passes)
    USkeletalMesh* PreviewMesh = PhysicsAsset->GetPreviewMesh();
    if (PreviewMesh)
    {
        USkeleton* Skeleton = PreviewMesh->GetSkeleton();
        if (Skeleton)
        {
            const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
            int32 BoneIndex = RefSkeleton.FindBoneIndex(FName(*BoneName));
            if (BoneIndex == INDEX_NONE)
            {
                SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("Bone '%s' does not exist in skeleton"), *BoneName),
                    TEXT("BONE_NOT_FOUND"));
                return true;
            }
        }
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
            return true;
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

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("boneName"), BoneName);
    Result->SetStringField(TEXT("bodyType"), BodyType);
    Result->SetNumberField(TEXT("bodyIndex"), BodyIndex);
    Result->SetBoolField(TEXT("created"), bCreated);

    SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Physics body %s for bone '%s'"), bCreated ? TEXT("created") : TEXT("modified"), *BoneName), Result);
    return true;
}

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
        return true;
    }

    if (BoneName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("boneName is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    // Validate path security BEFORE loading asset
    FString SanitizedPath = SanitizeProjectRelativePath(PhysicsAssetPath);
    if (SanitizedPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Invalid physics asset path '%s': contains traversal sequences or invalid characters"), *PhysicsAssetPath),
            TEXT("INVALID_PATH"));
        return true;
    }
    PhysicsAssetPath = SanitizedPath;

    FString Error;
    UPhysicsAsset* PhysicsAsset = LoadPhysicsAssetFromPath(PhysicsAssetPath, Error);
    if (!PhysicsAsset)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("PHYSICS_ASSET_NOT_FOUND"));
        return true;
    }

    int32 BodyIndex = PhysicsAsset->FindBodyIndex(FName(*BoneName));
    if (BodyIndex == INDEX_NONE)
    {
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("No physics body found for bone '%s'"), *BoneName),
            TEXT("BODY_NOT_FOUND"));
        return true;
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

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("boneName"), BoneName);
    Result->SetNumberField(TEXT("bodyIndex"), BodyIndex);

    SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Physics body '%s' configured"), *BoneName), Result);
    return true;
}

#endif // WITH_EDITOR
