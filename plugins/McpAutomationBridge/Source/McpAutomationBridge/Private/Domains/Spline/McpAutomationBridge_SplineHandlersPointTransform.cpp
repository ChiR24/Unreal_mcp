#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Spline/McpAutomationBridge_SplineHandlersPrivate.h"

#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

static bool GetSplinePointTarget(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    AActor*& OutActor,
    USplineComponent*& OutSplineComp,
    int32& OutPointIndex)
{
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
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    AActor* Actor,
    int32 PointIndex,
    const FString& Message)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("pointIndex"), PointIndex);
    McpHandlerUtils::AddVerification(Result, Actor);
    Self->SendAutomationResponse(Socket, RequestId, true, Message, Result);
}

bool HandleSetSplinePointTangents(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    AActor* Actor = nullptr;
    USplineComponent* SplineComp = nullptr;
    int32 PointIndex = 0;
    if (!GetSplinePointTarget(Self, RequestId, Payload, Socket, Actor, SplineComp, PointIndex))
    {
        return true;
    }

    // A distinct leaveTangent used to be logged and dropped on the claim that a point has one
    // tangent; SetTangentsAtSplinePoint takes both.
    if (!ApplySplinePointTangents(SplineComp, PointIndex, Payload))
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("arriveTangent or leaveTangent is required"), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }
    SplineComp->UpdateSpline();
    Actor->MarkPackageDirty();

    SendPointMutationResponse(Self, RequestId, Socket, Actor, PointIndex,
        FString::Printf(TEXT("Set tangents for spline point %d"), PointIndex));
    return true;
}

bool HandleSetSplinePointRotation(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    AActor* Actor = nullptr;
    USplineComponent* SplineComp = nullptr;
    int32 PointIndex = 0;
    if (!GetSplinePointTarget(Self, RequestId, Payload, Socket, Actor, SplineComp, PointIndex))
    {
        return true;
    }

    FRotator Rotation = ExtractRotatorField(Payload, TEXT("pointRotation"), FRotator::ZeroRotator);
    SplineComp->SetRotationAtSplinePoint(PointIndex, Rotation, ESplineCoordinateSpace::Local, true);
    SplineComp->UpdateSpline();
    Actor->MarkPackageDirty();

    SendPointMutationResponse(Self, RequestId, Socket, Actor, PointIndex,
        FString::Printf(TEXT("Set rotation for spline point %d"), PointIndex));
    return true;
}

bool HandleSetSplinePointScale(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    AActor* Actor = nullptr;
    USplineComponent* SplineComp = nullptr;
    int32 PointIndex = 0;
    if (!GetSplinePointTarget(Self, RequestId, Payload, Socket, Actor, SplineComp, PointIndex))
    {
        return true;
    }

    FVector Scale = ExtractVectorField(Payload, TEXT("pointScale"), FVector::OneVector);
    SplineComp->SetScaleAtSplinePoint(PointIndex, Scale, true);
    SplineComp->UpdateSpline();
    Actor->MarkPackageDirty();

    SendPointMutationResponse(Self, RequestId, Socket, Actor, PointIndex,
        FString::Printf(TEXT("Set scale for spline point %d"), PointIndex));
    return true;
}

bool HandleSetSplineType(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    FString SplineType = GetJsonStringField(Payload, TEXT("splineType"), TEXT("Curve"));
    int32 PointIndex = GetJsonIntField(Payload, TEXT("pointIndex"), -1);

    AActor* Actor = nullptr;
    USplineComponent* SplineComp = ResolveSplineTarget(Self, RequestId, Socket, ActorName, Actor);
    if (!SplineComp)
    {
        return true;
    }

    ESplinePointType::Type PointType = ParseSplinePointType(SplineType);
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
    Result->SetStringField(TEXT("splineType"), SplineType);
    Result->SetNumberField(TEXT("pointsAffected"), PointIndex >= 0 ? 1 : SplineComp->GetNumberOfSplinePoints());
    McpHandlerUtils::AddVerification(Result, Actor);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Set spline type to %s"), *SplineType), Result);
    return true;
}
