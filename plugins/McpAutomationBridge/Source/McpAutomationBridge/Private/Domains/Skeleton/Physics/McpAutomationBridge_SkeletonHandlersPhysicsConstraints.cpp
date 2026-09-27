#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersAssetLoading.h"
#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersPayload.h"

#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersProjectPaths.h"
#include "Safety/McpSafeOperations.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"

using namespace McpSkeletonHandlers;

namespace
{
// A missing angle and motion leave that axis alone; an angle without a motion
// means Limited. Every limits call used to reset the axes it did not name to
// 45 degrees Limited, so tightening one swing silently loosened the others.
void ApplyMcpConstraintAxis(const TSharedPtr<FJsonObject>& Limits, const TCHAR* AngleField, const TCHAR* MotionField,
                            float CurrentAngle, TFunctionRef<void(EAngularConstraintMotion, float)> Set)
{
    double Angle = CurrentAngle;
    const bool bAngle = Limits->TryGetNumberField(AngleField, Angle);
    FString MotionText;
    const bool bMotion = Limits->TryGetStringField(MotionField, MotionText);
    if (!bAngle && !bMotion)
    {
        return;
    }
    EAngularConstraintMotion Motion = EAngularConstraintMotion::ACM_Limited;
    if (MotionText.Equals(TEXT("Free"), ESearchCase::IgnoreCase))
    {
        Motion = EAngularConstraintMotion::ACM_Free;
    }
    else if (MotionText.Equals(TEXT("Locked"), ESearchCase::IgnoreCase))
    {
        Motion = EAngularConstraintMotion::ACM_Locked;
    }
    Set(Motion, static_cast<float>(Angle));
}

void ApplyMcpConstraintLimits(FConstraintInstance& Instance, const TSharedPtr<FJsonObject>& Limits)
{
    if (!Limits.IsValid())
    {
        return;
    }
    ApplyMcpConstraintAxis(Limits, TEXT("swing1LimitAngle"), TEXT("swing1Motion"), Instance.GetAngularSwing1Limit(),
        [&Instance](EAngularConstraintMotion Motion, float Angle) { Instance.SetAngularSwing1Limit(Motion, Angle); });
    ApplyMcpConstraintAxis(Limits, TEXT("swing2LimitAngle"), TEXT("swing2Motion"), Instance.GetAngularSwing2Limit(),
        [&Instance](EAngularConstraintMotion Motion, float Angle) { Instance.SetAngularSwing2Limit(Motion, Angle); });
    ApplyMcpConstraintAxis(Limits, TEXT("twistLimitAngle"), TEXT("twistMotion"), Instance.GetAngularTwistLimit(),
        [&Instance](EAngularConstraintMotion Motion, float Angle) { Instance.SetAngularTwistLimit(Motion, Angle); });
}

// The constraint joining the two bodies, in either order; null when none does.
UPhysicsConstraintTemplate* FindMcpConstraint(UPhysicsAsset* PhysicsAsset, const FString& BodyA, const FString& BodyB)
{
    const FName A(*BodyA);
    const FName B(*BodyB);
    for (UPhysicsConstraintTemplate* Candidate : PhysicsAsset->ConstraintSetup)
    {
        if (!Candidate)
        {
            continue;
        }
        const FName Bone1 = Candidate->DefaultInstance.ConstraintBone1;
        const FName Bone2 = Candidate->DefaultInstance.ConstraintBone2;
        if ((Bone1 == A && Bone2 == B) || (Bone1 == B && Bone2 == A))
        {
            return Candidate;
        }
    }
    return nullptr;
}
}

bool UMcpAutomationBridgeSubsystem::HandleAddPhysicsConstraint(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString PhysicsAssetPath = GetJsonStringField(Payload, TEXT("physicsAssetPath"));
    FString BodyA = GetJsonStringField(Payload, TEXT("bodyA"));
    FString BodyB = GetJsonStringField(Payload, TEXT("bodyB"));
    FString ConstraintName = GetJsonStringField(Payload, TEXT("constraintName"));

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

    FString Error;
    UPhysicsAsset* PhysicsAsset = LoadPhysicsAssetFromPath(PhysicsAssetPath, Error);
    if (!PhysicsAsset)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("PHYSICS_ASSET_NOT_FOUND"));
        return true;
    }

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

    // set_physics_constraint edits the constraint the two bodies already share;
    // it used to append a second one on every call. add_physics_constraint
    // refuses a duplicate instead of stacking another joint on the same pair.
    const bool bUpsert = GetJsonStringField(Payload, TEXT("subAction")) == TEXT("set_physics_constraint");
    UPhysicsConstraintTemplate* Constraint = FindMcpConstraint(PhysicsAsset, BodyA, BodyB);
    if (Constraint && !bUpsert)
    {
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("A constraint between '%s' and '%s' already exists; use set_physics_constraint or configure_constraint_limits to change it"), *BodyA, *BodyB),
            TEXT("CONSTRAINT_EXISTS"));
        return true;
    }
    PhysicsAsset->Modify();
    const bool bCreated = Constraint == nullptr;
    if (bCreated)
    {
        Constraint = NewObject<UPhysicsConstraintTemplate>(PhysicsAsset, NAME_None, RF_Transactional);
        if (!Constraint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create physics constraint"), TEXT("CREATION_FAILED"));
            return true;
        }
        Constraint->DefaultInstance.ConstraintBone1 = FName(*BodyA);
        Constraint->DefaultInstance.ConstraintBone2 = FName(*BodyB);
        // A new joint starts at 45 degrees Limited on every axis; limits refines it.
        Constraint->DefaultInstance.SetAngularSwing1Limit(EAngularConstraintMotion::ACM_Limited, 45.0f);
        Constraint->DefaultInstance.SetAngularSwing2Limit(EAngularConstraintMotion::ACM_Limited, 45.0f);
        Constraint->DefaultInstance.SetAngularTwistLimit(EAngularConstraintMotion::ACM_Limited, 45.0f);
        PhysicsAsset->ConstraintSetup.Add(Constraint);
    }

    // Set default constraint profile name via JointName (ProfileName removed in UE 5.7)
    if (!ConstraintName.IsEmpty())
    {
        Constraint->DefaultInstance.JointName = FName(*ConstraintName);
    }

    const TSharedPtr<FJsonObject>* LimitsObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("limits"), LimitsObj) && LimitsObj)
    {
        ApplyMcpConstraintLimits(Constraint->DefaultInstance, *LimitsObj);
    }

    PhysicsAsset->UpdateBodySetupIndexMap();
    SaveIfRequested(PhysicsAsset, Payload);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("bodyA"), BodyA);
    Result->SetStringField(TEXT("bodyB"), BodyB);
    Result->SetNumberField(TEXT("constraintIndex"), PhysicsAsset->ConstraintSetup.IndexOfByKey(Constraint));
    Result->SetBoolField(TEXT("created"), bCreated);

    SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Constraint %s between '%s' and '%s'"), bCreated ? TEXT("created") : TEXT("updated"), *BodyA, *BodyB), Result);
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleConfigureConstraintLimits(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString PhysicsAssetPath = GetJsonStringField(Payload, TEXT("physicsAssetPath"));
    FString BodyA = GetJsonStringField(Payload, TEXT("bodyA"));
    FString BodyB = GetJsonStringField(Payload, TEXT("bodyB"));

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

    FString Error;
    UPhysicsAsset* PhysicsAsset = LoadPhysicsAssetFromPath(PhysicsAssetPath, Error);
    if (!PhysicsAsset)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("PHYSICS_ASSET_NOT_FOUND"));
        return true;
    }

    UPhysicsConstraintTemplate* Constraint = FindMcpConstraint(PhysicsAsset, BodyA, BodyB);
    if (!Constraint)
    {
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("No constraint found between '%s' and '%s'"), *BodyA, *BodyB),
            TEXT("CONSTRAINT_NOT_FOUND"));
        return true;
    }

    // limits carries the fields; without it they are read from the request itself.
    PhysicsAsset->Modify();
    const TSharedPtr<FJsonObject>* LimitsObj = nullptr;
    const bool bHasLimits = Payload->TryGetObjectField(TEXT("limits"), LimitsObj) && LimitsObj;
    ApplyMcpConstraintLimits(Constraint->DefaultInstance, bHasLimits ? *LimitsObj : Payload);

    SaveIfRequested(PhysicsAsset, Payload);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("bodyA"), BodyA);
    Result->SetStringField(TEXT("bodyB"), BodyB);

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Constraint limits configured"), Result);
    return true;
}

