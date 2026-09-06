#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Spline/McpAutomationBridge_SplineHandlersPrivate.h"

#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#if WITH_EDITOR
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
    SplineComp->UpdateSpline();
    World->MarkPackageDirty();

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

    if (PointIndex < 0 || PointIndex >= SplineComp->GetNumberOfSplinePoints())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Invalid point index: %d"), PointIndex), nullptr, TEXT("INVALID_INDEX"));
        return true;
    }

    SplineComp->RemoveSplinePoint(PointIndex, true);
    SplineComp->UpdateSpline();
    World->MarkPackageDirty();

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

    if (PointIndex < 0 || PointIndex >= SplineComp->GetNumberOfSplinePoints())
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Invalid point index: %d"), PointIndex), nullptr, TEXT("INVALID_INDEX"));
        return true;
    }

    SplineComp->SetLocationAtSplinePoint(PointIndex, Position, ESplineCoordinateSpace::Local, true);
    SplineComp->UpdateSpline();
    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("pointIndex"), PointIndex);
    McpHandlerUtils::AddVerification(Result, Actor);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Set position for spline point %d"), PointIndex), Result);
    return true;
}
#endif
