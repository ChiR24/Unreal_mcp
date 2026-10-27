#include "Domains/Skeleton/Assets/McpSkeletonAssetLoading.h"
#include "Domains/Skeleton/Assets/McpSkeletonPayload.h"

#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Foundation/BridgeHelpers/Security/McpProjectPaths.h"
#include "Safety/McpSafeOperations.h"
#include "EngineWireSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "ReferenceSkeleton.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
#if __has_include("PhysicsEngine/SkeletalBodySetup.h")
#include "PhysicsEngine/SkeletalBodySetup.h"
#endif
#endif

using namespace McpSkeletonHandlers;

bool UEngineWireSubsystem::HandleAddPhysicsBody(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString PhysicsAssetPath = GetJsonStringField(Payload, TEXT("physicsAssetPath"));
    FString BoneName = GetJsonStringField(Payload, TEXT("boneName"));
    FString BodyType = GetJsonStringField(Payload, TEXT("bodyType"));

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
    // Any other bodyType built a capsule without a word.
    if (!BodyType.IsEmpty() && !BodyType.Equals(TEXT("Capsule"), ESearchCase::IgnoreCase) && !BodyType.Equals(TEXT("Sphyl"), ESearchCase::IgnoreCase)
        && !BodyType.Equals(TEXT("Sphere"), ESearchCase::IgnoreCase) && !BodyType.Equals(TEXT("Box"), ESearchCase::IgnoreCase))
    {
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("bodyType '%s' is not a body shape; use Capsule (the default), "
            "Sphere or Box. Nothing was changed."), *BodyType), TEXT("INVALID_ARGUMENT"));
        return true;
    }

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

    int32 BodyIndex = PhysicsAsset->FindBodyIndex(FName(*BoneName));
    USkeletalBodySetup* BodySetup = nullptr;
    bool bCreated = false;

    PhysicsAsset->Modify();
    if (BodyIndex == INDEX_NONE)
    {
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
        BodySetup->Modify();
    }

    if (BodyType.IsEmpty()) BodyType = TEXT("Capsule");

    double Radius = 10.0;
    double Length = 20.0;
    double Width = 10.0, Height = 10.0, Depth = 10.0;

    Payload->TryGetNumberField(TEXT("radius"), Radius);
    Payload->TryGetNumberField(TEXT("length"), Length);
    Payload->TryGetNumberField(TEXT("width"), Width);
    Payload->TryGetNumberField(TEXT("height"), Height);
    Payload->TryGetNumberField(TEXT("depth"), Depth);

    FVector Center = ExtractVectorField(Payload, TEXT("center"), FVector::ZeroVector);
    FRotator Rotation = ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);

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
    else
    {
        // Capsule/Sphyl, and the default for any other bodyType.
        FKSphylElem CapsuleElem;
        CapsuleElem.Radius = static_cast<float>(Radius);
        CapsuleElem.Length = static_cast<float>(Length);
        CapsuleElem.Center = Center;
        CapsuleElem.Rotation = Rotation;
        BodySetup->AggGeom.SphylElems.Add(CapsuleElem);
    }

    PhysicsAsset->UpdateBodySetupIndexMap();
    PhysicsAsset->UpdateBoundsBodiesArray();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("boneName"), BoneName);
    Result->SetStringField(TEXT("bodyType"), BodyType);
    Result->SetNumberField(TEXT("bodyIndex"), BodyIndex);
    Result->SetBoolField(TEXT("created"), bCreated);
    SaveAndReport(PhysicsAsset, Payload, Result);

    SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Physics body %s for bone '%s'"), bCreated ? TEXT("created") : TEXT("modified"), *BoneName), Result);
    return true;
}

bool UEngineWireSubsystem::HandleConfigurePhysicsBody(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString PhysicsAssetPath = GetJsonStringField(Payload, TEXT("physicsAssetPath"));
    FString BoneName = GetJsonStringField(Payload, TEXT("boneName"));

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
    static const TCHAR* Fields[] = {TEXT("mass"), TEXT("linearDamping"), TEXT("angularDamping"), TEXT("collisionEnabled"), TEXT("simulatePhysics")};
    bool bAnySent = false;
    for (const TCHAR* Field : Fields)
    {
        bAnySent |= Payload->HasField(Field);
    }
    if (!bAnySent)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Nothing to set: send mass, linearDamping, angularDamping, collisionEnabled or simulatePhysics."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    double Mass = 0.0;
    if (Payload->TryGetNumberField(TEXT("mass"), Mass) && Mass <= 0.0)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("mass must be above 0 kg. Nothing was changed."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    PhysicsAsset->Modify();
    BodySetup->Modify();
    if (Payload->HasField(TEXT("mass")))
    {
        // It turned the override on and never wrote the mass, so the body took the default override instead.
        BodySetup->DefaultInstance.SetMassOverride(static_cast<float>(Mass), true);
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
        // A physics asset decides per body through PhysicsType: Default simulates when the mesh does (a ragdoll), Kinematic
        // follows the animation. DefaultInstance.bSimulatePhysics, which this wrote, is not read for its bodies.
        BodySetup->PhysicsType = bSimulatePhysics ? EPhysicsType::PhysType_Default : EPhysicsType::PhysType_Kinematic;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    SaveAndReport(PhysicsAsset, Payload, Result);
    Result->SetStringField(TEXT("boneName"), BoneName);
    Result->SetNumberField(TEXT("bodyIndex"), BodyIndex);
    if (BodySetup->DefaultInstance.bOverrideMass)
    {
        Result->SetNumberField(TEXT("mass"), BodySetup->DefaultInstance.GetMassOverride());
    }
    Result->SetNumberField(TEXT("linearDamping"), BodySetup->DefaultInstance.LinearDamping);
    Result->SetNumberField(TEXT("angularDamping"), BodySetup->DefaultInstance.AngularDamping);
    Result->SetStringField(TEXT("physicsType"), BodySetup->PhysicsType == EPhysicsType::PhysType_Kinematic ? TEXT("Kinematic")
        : BodySetup->PhysicsType == EPhysicsType::PhysType_Simulated ? TEXT("Simulated") : TEXT("Default"));

    SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Physics body '%s' configured"), *BoneName), Result);
    return true;
}

