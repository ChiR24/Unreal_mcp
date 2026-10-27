#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Spline/McpSplinePrivate.h"

#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

static bool GetSplinePointTarget(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    AActor*& OutActor,
    USplineComponent*& OutSplineComp,
    int32& OutPointIndex,
    const TCHAR* ValueField)
{
    // Left out, pointIndex edited point 0 and a missing value reset the point (rotation zero, scale one).
    if (!Payload->HasField(TEXT("pointIndex")) || (ValueField && !Payload->HasField(ValueField)))
    {
        const FString Needed = ValueField ? FString::Printf(TEXT("pointIndex and %s are"), ValueField) : FString(TEXT("pointIndex is"));
        Self->SendAutomationResponse(Socket, RequestId, false, Needed + TEXT(" required. Nothing was changed."), nullptr,
                                     TEXT("INVALID_ARGUMENT"));
        return false;
    }
    OutPointIndex = GetJsonIntField(Payload, TEXT("pointIndex"), 0);
    OutSplineComp = ResolveSplineTarget(Self, RequestId, Socket, GetJsonStringField(Payload, TEXT("actorName")), OutActor);
    if (!OutSplineComp)
    {
        return false;
    }

    if (OutPointIndex < 0 || OutPointIndex >= OutSplineComp->GetNumberOfSplinePoints())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Invalid point index: %d"), OutPointIndex), nullptr, TEXT("INVALID_INDEX"));
        return false;
    }

    return true;
}

static void SendPointMutationResponse(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    AActor* Actor,
    int32 PointIndex,
    const FString& Message,
    const FMcpScopedEditorTransaction& Transaction)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("pointIndex"), PointIndex);
    Transaction.DescribeInto(Result);
    McpHandlerUtils::AddVerification(Result, Actor);
    Self->SendAutomationResponse(Socket, RequestId, true, Message, Result);
}

bool HandleSetSplinePointTangents(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    AActor* Actor = nullptr;
    USplineComponent* SplineComp = nullptr;
    int32 PointIndex = 0;
    if (!GetSplinePointTarget(Self, RequestId, Payload, Socket, Actor, SplineComp, PointIndex, nullptr))
    {
        return true;
    }
    if (!Payload->HasField(TEXT("arriveTangent")) && !Payload->HasField(TEXT("leaveTangent")))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("arriveTangent or leaveTangent is required"), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }
    // These point edits ran outside any transaction, so undo could not take them back.
    const FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Edit Spline Point")), EMcpMutationDurability::EditorStateOnly,
                                                  {SplineComp});
    // A distinct leaveTangent used to be logged and dropped on the claim that a point has one
    // tangent; SetTangentsAtSplinePoint takes both.
    ApplySplinePointTangents(SplineComp, PointIndex, Payload);
    SplineComp->UpdateSpline();
    Actor->MarkPackageDirty();

    SendPointMutationResponse(Self, RequestId, Socket, Actor, PointIndex,
        FString::Printf(TEXT("Set tangents for spline point %d"), PointIndex), Transaction);
    return true;
}

bool HandleSetSplinePointRotation(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    AActor* Actor = nullptr;
    USplineComponent* SplineComp = nullptr;
    int32 PointIndex = 0;
    if (!GetSplinePointTarget(Self, RequestId, Payload, Socket, Actor, SplineComp, PointIndex, TEXT("pointRotation")))
    {
        return true;
    }
    const FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Edit Spline Point")), EMcpMutationDurability::EditorStateOnly,
                                                  {SplineComp});

    FRotator Rotation = ExtractRotatorField(Payload, TEXT("pointRotation"), FRotator::ZeroRotator);
    SplineComp->SetRotationAtSplinePoint(PointIndex, Rotation, ESplineCoordinateSpace::Local, true);
    SplineComp->UpdateSpline();
    Actor->MarkPackageDirty();

    SendPointMutationResponse(Self, RequestId, Socket, Actor, PointIndex,
        FString::Printf(TEXT("Set rotation for spline point %d"), PointIndex), Transaction);
    return true;
}

bool HandleSetSplinePointScale(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    AActor* Actor = nullptr;
    USplineComponent* SplineComp = nullptr;
    int32 PointIndex = 0;
    if (!GetSplinePointTarget(Self, RequestId, Payload, Socket, Actor, SplineComp, PointIndex, TEXT("pointScale")))
    {
        return true;
    }
    const FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Edit Spline Point")), EMcpMutationDurability::EditorStateOnly,
                                                  {SplineComp});

    FVector Scale = ExtractVectorField(Payload, TEXT("pointScale"), FVector::OneVector);
    SplineComp->SetScaleAtSplinePoint(PointIndex, Scale, true);
    SplineComp->UpdateSpline();
    Actor->MarkPackageDirty();

    SendPointMutationResponse(Self, RequestId, Socket, Actor, PointIndex,
        FString::Printf(TEXT("Set scale for spline point %d"), PointIndex), Transaction);
    return true;
}

bool HandleSetSplineType(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    FString SplineType = GetJsonStringField(Payload, TEXT("splineType"), TEXT("Curve"));
    int32 PointIndex = GetJsonIntField(Payload, TEXT("pointIndex"), -1);

    ESplinePointType::Type PointType;
    if (!ParseSplinePointType(SplineType, PointType))
    {
        return RefuseSplinePointType(Self, RequestId, Socket, SplineType);
    }
    AActor* Actor = nullptr;
    USplineComponent* SplineComp = ResolveSplineTarget(Self, RequestId, Socket, ActorName, Actor);
    if (!SplineComp)
    {
        return true;
    }

    // It ran outside any transaction, so undo could not take a type change back.
    FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Set Spline Point Type")), EMcpMutationDurability::EditorStateOnly,
                                            {SplineComp});
    if (PointIndex >= 0)
    {
        if (PointIndex >= SplineComp->GetNumberOfSplinePoints())
        {
            Self->SendAutomationResponse(Socket, RequestId, false,
                FString::Printf(TEXT("Invalid point index: %d"), PointIndex), nullptr, TEXT("INVALID_INDEX"));
            return true;
        }
        SplineComp->SetSplinePointType(PointIndex, PointType, true);
    }
    else
    {
        for (int32 i = 0; i < SplineComp->GetNumberOfSplinePoints(); i++)
        {
            SplineComp->SetSplinePointType(i, PointType, false);
        }
    }

    SplineComp->UpdateSpline();
    Actor->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("splineType"), SplinePointTypeToString(PointType));
    Result->SetNumberField(TEXT("pointsAffected"), PointIndex >= 0 ? 1 : SplineComp->GetNumberOfSplinePoints());
    Transaction.DescribeInto(Result);
    McpHandlerUtils::AddVerification(Result, Actor);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Set spline type to %s"), *SplinePointTypeToString(PointType)), Result);
    return true;
}
