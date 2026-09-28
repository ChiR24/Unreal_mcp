#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Spline/McpAutomationBridge_SplineHandlersPrivate.h"

#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

bool HandleAddSplinePoint(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    FVector Position = ExtractVectorField(Payload, TEXT("position"), FVector::ZeroVector);
    int32 Index = GetJsonIntField(Payload, TEXT("index"), -1);
    FString PointType = GetJsonStringField(Payload, TEXT("pointType"), TEXT("Curve"));

    AActor* Actor = nullptr;
    USplineComponent* SplineComp = ResolveSplineTarget(Self, RequestId, Socket, ActorName, Actor);
    if (!SplineComp)
    {
        return true;
    }

    if (Index < 0 || Index >= SplineComp->GetNumberOfSplinePoints())
    {
        SplineComp->AddSplinePoint(Position, ESplineCoordinateSpace::Local, true);
        Index = SplineComp->GetNumberOfSplinePoints() - 1;
    }
    else
    {
        SplineComp->AddSplinePointAtIndex(Position, Index, ESplineCoordinateSpace::Local, true);
    }

    SplineComp->SetSplinePointType(Index, ParseSplinePointType(PointType), true);
    // The declared tangents were never read, so every added point took automatic ones.
    ApplySplinePointTangents(SplineComp, Index, Payload);
    SplineComp->UpdateSpline();
    Actor->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("pointIndex"), Index);
    Result->SetNumberField(TEXT("totalPoints"), SplineComp->GetNumberOfSplinePoints());
    McpHandlerUtils::AddVerification(Result, Actor);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Added spline point at index %d"), Index), Result);
    return true;
}

bool HandleRemoveSplinePoint(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    int32 PointIndex = GetJsonIntField(Payload, TEXT("pointIndex"), 0);

    AActor* Actor = nullptr;
    USplineComponent* SplineComp = ResolveSplineTarget(Self, RequestId, Socket, ActorName, Actor);
    if (!SplineComp)
    {
        return true;
    }

    if (PointIndex < 0 || PointIndex >= SplineComp->GetNumberOfSplinePoints())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Invalid point index: %d"), PointIndex), nullptr, TEXT("INVALID_INDEX"));
        return true;
    }

    SplineComp->RemoveSplinePoint(PointIndex, true);
    SplineComp->UpdateSpline();
    Actor->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("removedIndex"), PointIndex);
    Result->SetNumberField(TEXT("remainingPoints"), SplineComp->GetNumberOfSplinePoints());
    McpHandlerUtils::AddVerification(Result, Actor);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Removed spline point at index %d"), PointIndex), Result);
    return true;
}

bool HandleSetSplinePointPosition(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    int32 PointIndex = GetJsonIntField(Payload, TEXT("pointIndex"), 0);
    FVector Position = ExtractVectorField(Payload, TEXT("position"), FVector::ZeroVector);

    AActor* Actor = nullptr;
    USplineComponent* SplineComp = ResolveSplineTarget(Self, RequestId, Socket, ActorName, Actor);
    if (!SplineComp)
    {
        return true;
    }

    if (PointIndex < 0 || PointIndex >= SplineComp->GetNumberOfSplinePoints())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Invalid point index: %d"), PointIndex), nullptr, TEXT("INVALID_INDEX"));
        return true;
    }

    SplineComp->SetLocationAtSplinePoint(PointIndex, Position, ESplineCoordinateSpace::Local, true);
    SplineComp->UpdateSpline();
    Actor->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("pointIndex"), PointIndex);
    McpHandlerUtils::AddVerification(Result, Actor);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Set position for spline point %d"), PointIndex), Result);
    return true;
}
