#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersAssetLoading.h"
#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersPayload.h"

#include "Safety/McpSafeOperations.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#if __has_include("PhysicsEngine/SkeletalBodySetup.h")
#include "PhysicsEngine/SkeletalBodySetup.h"
#endif

using namespace McpSkeletonHandlers;

bool UMcpAutomationBridgeSubsystem::HandleRemovePhysicsBody(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString PhysicsAssetPath = GetJsonStringField(Payload, TEXT("physicsAssetPath"));
    FString BoneName = GetJsonStringField(Payload, TEXT("boneName"));

    if (PhysicsAssetPath.IsEmpty() || BoneName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("physicsAssetPath and boneName are required"), TEXT("MISSING_PARAM"));
        return true;
    }

    // LoadPhysicsAssetFromPath sanitizes the path (this loaded the raw input before).
    FString Error;
    UPhysicsAsset* PhysAsset = LoadPhysicsAssetFromPath(PhysicsAssetPath, Error);
    if (!PhysAsset)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("PHYSICS_ASSET_NOT_FOUND"));
        return true;
    }

    const int32 BodyIndex = PhysAsset->FindBodyIndex(FName(*BoneName));

    if (BodyIndex == INDEX_NONE)
    {
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("No physics body found for bone: %s"), *BoneName),
            TEXT("BODY_NOT_FOUND"));
        return true;
    }

    PhysAsset->Modify();

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
}

