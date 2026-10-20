#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Spline/McpSplinePrivate.h"

#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/McpScopedEditorTransaction.h"

#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

bool HandleAddSplinePoint(
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    FVector Position = ExtractVectorField(Payload, TEXT("position"), FVector::ZeroVector);
    int32 Index = GetJsonIntField(Payload, TEXT("index"), -1);
    FString PointType = GetJsonStringField(Payload, TEXT("pointType"), TEXT("Curve"));
    ESplinePointType::Type ParsedType;
    if (!ParseSplinePointType(PointType, ParsedType))
    {
        return RefuseSplinePointType(Self, RequestId, Socket, PointType);
    }
    // Left out, the point landed on the actor's own origin.
    if (!Payload->HasField(TEXT("position")))
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("position is required. Nothing was changed."), nullptr,
                                     TEXT("INVALID_ARGUMENT"));
        return true;
    }

    AActor* Actor = nullptr;
    USplineComponent* SplineComp = ResolveSplineTarget(Self, RequestId, Socket, ActorName, Actor);
    if (!SplineComp)
    {
        return true;
    }

    FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Add Spline Point")), EMcpMutationDurability::EditorStateOnly,
                                            {SplineComp});
    if (Index < 0 || Index >= SplineComp->GetNumberOfSplinePoints())
    {
        SplineComp->AddSplinePoint(Position, ESplineCoordinateSpace::Local, true);
        Index = SplineComp->GetNumberOfSplinePoints() - 1;
    }
    else
    {
        SplineComp->AddSplinePointAtIndex(Position, Index, ESplineCoordinateSpace::Local, true);
    }

    SplineComp->SetSplinePointType(Index, ParsedType, true);
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
    UEngineWireSubsystem* Self,
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

    FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Remove Spline Point")), EMcpMutationDurability::EditorStateOnly,
                                            {SplineComp});
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
    UEngineWireSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    int32 PointIndex = GetJsonIntField(Payload, TEXT("pointIndex"), 0);
    FVector Position = ExtractVectorField(Payload, TEXT("position"), FVector::ZeroVector);
    // Left out, these moved point 0, or moved the point to the actor's origin.
    if (!Payload->HasField(TEXT("pointIndex")) || !Payload->HasField(TEXT("position")))
    {
        Self->SendAutomationResponse(Socket, RequestId, false, TEXT("pointIndex and position are required. Nothing was changed."),
                                     nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

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

    FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Move Spline Point")), EMcpMutationDurability::EditorStateOnly,
                                            {SplineComp});
    SplineComp->SetLocationAtSplinePoint(PointIndex, Position, ESplineCoordinateSpace::Local, true);
    SplineComp->UpdateSpline();
    Actor->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("pointIndex"), PointIndex);
    Transaction.DescribeInto(Result);
    McpHandlerUtils::AddVerification(Result, Actor);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Set position for spline point %d"), PointIndex), Result);
    return true;
}
