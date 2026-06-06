#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SplineHandlersPrivate.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
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
    UWorld*& OutWorld,
    int32& OutPointIndex)
{
    FString ActorName = GetJsonStringFieldSpline(Payload, TEXT("actorName"));
    OutPointIndex = GetJsonIntFieldSpline(Payload, TEXT("pointIndex"), 0);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorName is required"), nullptr, TEXT("MISSING_PARAM"));
        return false;
    }

    OutWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!OutWorld)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return false;
    }

    OutActor = FindActorByName(OutWorld, ActorName);
    if (!OutActor)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Actor not found: %s"), *ActorName), nullptr, TEXT("NOT_FOUND"));
        return false;
    }

    OutSplineComp = FindSplineComponent(OutActor);
    if (!OutSplineComp)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No spline component found on actor"), nullptr, TEXT("NO_SPLINE"));
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
    UWorld* World = nullptr;
    int32 PointIndex = 0;
    if (!GetSplinePointTarget(Self, RequestId, Payload, Socket, Actor, SplineComp, World, PointIndex))
    {
        return true;
    }

    FVector ArriveTangent = GetJsonVectorFieldSpline(Payload, TEXT("arriveTangent"));
    FVector LeaveTangent = GetJsonVectorFieldSpline(Payload, TEXT("leaveTangent"));
    if (!LeaveTangent.IsZero() && LeaveTangent != ArriveTangent)
    {
        UE_LOG(LogMcpSplineHandlers, Warning,
            TEXT("leaveTangent ignored for point %d - UE splines use a single tangent per point. Use arriveTangent only."),
            PointIndex);
    }

    SplineComp->SetTangentAtSplinePoint(PointIndex, ArriveTangent, ESplineCoordinateSpace::Local, true);
    SplineComp->UpdateSpline();
    World->MarkPackageDirty();

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
    UWorld* World = nullptr;
    int32 PointIndex = 0;
    if (!GetSplinePointTarget(Self, RequestId, Payload, Socket, Actor, SplineComp, World, PointIndex))
    {
        return true;
    }

    FRotator Rotation = GetJsonRotatorFieldSpline(Payload, TEXT("pointRotation"));
    SplineComp->SetRotationAtSplinePoint(PointIndex, Rotation, ESplineCoordinateSpace::Local, true);
    SplineComp->UpdateSpline();
    World->MarkPackageDirty();

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
    UWorld* World = nullptr;
    int32 PointIndex = 0;
    if (!GetSplinePointTarget(Self, RequestId, Payload, Socket, Actor, SplineComp, World, PointIndex))
    {
        return true;
    }

    FVector Scale = GetJsonVectorFieldSpline(Payload, TEXT("pointScale"), FVector::OneVector);
    SplineComp->SetScaleAtSplinePoint(PointIndex, Scale, true);
    SplineComp->UpdateSpline();
    World->MarkPackageDirty();

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
    FString ActorName = GetJsonStringFieldSpline(Payload, TEXT("actorName"));
    FString SplineType = GetJsonStringFieldSpline(Payload, TEXT("splineType"), TEXT("Curve"));
    int32 PointIndex = GetJsonIntFieldSpline(Payload, TEXT("pointIndex"), -1);

    if (ActorName.IsEmpty())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorName is required"), nullptr, TEXT("MISSING_PARAM"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    AActor* Actor = FindActorByName(World, ActorName);
    if (!Actor)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Actor not found: %s"), *ActorName), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    USplineComponent* SplineComp = FindSplineComponent(Actor);
    if (!SplineComp)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No spline component found on actor"), nullptr, TEXT("NO_SPLINE"));
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
    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("splineType"), SplineType);
    Result->SetNumberField(TEXT("pointsAffected"), PointIndex >= 0 ? 1 : SplineComp->GetNumberOfSplinePoints());
    McpHandlerUtils::AddVerification(Result, Actor);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Set spline type to %s"), *SplineType), Result);
    return true;
}
#endif
