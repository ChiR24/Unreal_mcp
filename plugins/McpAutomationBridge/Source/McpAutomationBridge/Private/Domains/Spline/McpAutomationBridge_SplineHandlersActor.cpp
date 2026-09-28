#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Spline/McpAutomationBridge_SplineHandlersPrivate.h"

#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

bool HandleCreateSplineActor(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("name"), GetJsonStringField(Payload, TEXT("actorName"), TEXT("SplineActor")));
    FVector Location = ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
    FRotator Rotation = ExtractRotatorField(Payload, TEXT("rotation"), FRotator::ZeroRotator);
    bool bClosedLoop = GetJsonBoolField(Payload, TEXT("bClosedLoop"), false);
    FString SplineType = GetJsonStringField(Payload, TEXT("splineType"), TEXT("Curve"));

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    USplineComponent* SplineComp = SpawnSplineActor(World, ActorName, Location, Rotation);
    if (!SplineComp)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn spline actor"), nullptr, TEXT("SPAWN_FAILED"));
        return true;
    }
    AActor* NewActor = SplineComp->GetOwner();
    SplineComp->SetClosedLoop(bClosedLoop);

    ESplinePointType::Type PointType = ParseSplinePointType(SplineType);
    for (int32 i = 0; i < SplineComp->GetNumberOfSplinePoints(); i++)
    {
        SplineComp->SetSplinePointType(i, PointType, true);
    }
    SplineComp->UpdateSpline();

    const TArray<TSharedPtr<FJsonValue>>* PointsArray = nullptr;
    if (!Payload->TryGetArrayField(TEXT("points"), PointsArray))
    {
        Payload->TryGetArrayField(TEXT("initialPoints"), PointsArray);
    }
    if (PointsArray)
    {
        SplineComp->ClearSplinePoints(false);
        AddSplinePointsFromJson(SplineComp, *PointsArray, PointType);
        SplineComp->UpdateSpline();
    }

    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), McpActorRef(NewActor));
    Result->SetStringField(TEXT("actorPath"), NewActor->GetPathName());
    Result->SetNumberField(TEXT("pointCount"), SplineComp->GetNumberOfSplinePoints());
    Result->SetNumberField(TEXT("splineLength"), SplineComp->GetSplineLength());
    Result->SetBoolField(TEXT("closedLoop"), SplineComp->IsClosedLoop());
    McpHandlerUtils::AddVerification(Result, NewActor);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Spline actor '%s' created with %d points"), *ActorName, SplineComp->GetNumberOfSplinePoints()), Result);
    return true;
}
