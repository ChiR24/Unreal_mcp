#include "McpAutomationBridge_SkeletonHandlersAssetLoading.h"
#include "McpAutomationBridge_SkeletonHandlersPayload.h"

#include "McpAutomationBridgeHelpersProjectPaths.h"
#include "McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"

#if WITH_EDITOR
using namespace McpSkeletonHandlers;

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
        return true;
    }

    if (BodyA.IsEmpty() || BodyB.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("bodyA and bodyB are required"), TEXT("MISSING_PARAM"));
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

    // Check that both bodies exist
    if (PhysicsAsset->FindBodyIndex(FName(*BodyA)) == INDEX_NONE)
    {
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Body '%s' not found in physics asset"), *BodyA),
            TEXT("BODY_NOT_FOUND"));
        return true;
    }

    if (PhysicsAsset->FindBodyIndex(FName(*BodyB)) == INDEX_NONE)
    {
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Body '%s' not found in physics asset"), *BodyB),
            TEXT("BODY_NOT_FOUND"));
        return true;
    }

    // Create constraint
    UPhysicsConstraintTemplate* Constraint = NewObject<UPhysicsConstraintTemplate>(PhysicsAsset, NAME_None, RF_Transactional);
    if (!Constraint)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create physics constraint"), TEXT("CREATION_FAILED"));
        return true;
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

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("bodyA"), BodyA);
    Result->SetStringField(TEXT("bodyB"), BodyB);
    Result->SetNumberField(TEXT("constraintIndex"), PhysicsAsset->ConstraintSetup.Num() - 1);

    SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Constraint created between '%s' and '%s'"), *BodyA, *BodyB), Result);
    return true;
}

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
        return true;
    }

    if (BodyA.IsEmpty() || BodyB.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("bodyA and bodyB are required to identify constraint"), TEXT("MISSING_PARAM"));
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
        return true;
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

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("bodyA"), BodyA);
    Result->SetStringField(TEXT("bodyB"), BodyB);

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Constraint limits configured"), Result);
    return true;
}

#endif // WITH_EDITOR
