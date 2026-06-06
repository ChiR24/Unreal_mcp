#include "McpAutomationBridge_SkeletonHandlersPayload.h"

#include "McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/SkeletalBodySetup.h"

#if WITH_EDITOR
using namespace McpSkeletonHandlers;

bool UMcpAutomationBridgeSubsystem::HandleRemovePhysicsBody(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    FString PhysicsAssetPath = GetStringFieldSkel(Payload, TEXT("physicsAssetPath"));
    FString BoneName = GetStringFieldSkel(Payload, TEXT("boneName"));

    if (PhysicsAssetPath.IsEmpty() || BoneName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("physicsAssetPath and boneName are required"), TEXT("MISSING_PARAM"));
        return true;
    }

    // Load physics asset
    UPhysicsAsset* PhysAsset = Cast<UPhysicsAsset>(
        StaticLoadObject(UPhysicsAsset::StaticClass(), nullptr, *PhysicsAssetPath));
    if (!PhysAsset)
    {
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Physics asset not found: %s"), *PhysicsAssetPath),
            TEXT("PHYSICS_ASSET_NOT_FOUND"));
        return true;
    }

    // Find and remove the body setup for this bone
    int32 BodyIndex = INDEX_NONE;
    for (int32 i = 0; i < PhysAsset->SkeletalBodySetups.Num(); ++i)
    {
        if (PhysAsset->SkeletalBodySetups[i] &&
            PhysAsset->SkeletalBodySetups[i]->BoneName == FName(*BoneName))
        {
            BodyIndex = i;
            break;
        }
    }

    if (BodyIndex == INDEX_NONE)
    {
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("No physics body found for bone: %s"), *BoneName),
            TEXT("BODY_NOT_FOUND"));
        return true;
    }

    // Remove the body setup and any associated constraints
    PhysAsset->Modify();

    // Remove constraints that reference this body
    FName BoneFName(*BoneName);
    for (int32 i = PhysAsset->ConstraintSetup.Num() - 1; i >= 0; --i)
    {
        UPhysicsConstraintTemplate* Constraint = PhysAsset->ConstraintSetup[i];
        if (Constraint)
        {
            FConstraintInstance& CI = Constraint->DefaultInstance;
            if (CI.ConstraintBone1 == BoneFName || CI.ConstraintBone2 == BoneFName)
            {
                PhysAsset->ConstraintSetup.RemoveAt(i);
            }
        }
    }

    // Remove the body setup
    PhysAsset->SkeletalBodySetups.RemoveAt(BodyIndex);
    PhysAsset->UpdateBoundsBodiesArray();
    PhysAsset->UpdateBodySetupIndexMap();
    PhysAsset->MarkPackageDirty();
    McpSafeAssetSave(PhysAsset);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("physicsAssetPath"), PhysicsAssetPath);
    Result->SetStringField(TEXT("boneName"), BoneName);
    Result->SetNumberField(TEXT("remainingBodies"), PhysAsset->SkeletalBodySetups.Num());

    SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Physics body for bone '%s' removed"), *BoneName), Result);
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("remove_physics_body requires editor mode"), TEXT("NOT_EDITOR"));
    return true;
#endif
}

#endif // WITH_EDITOR
