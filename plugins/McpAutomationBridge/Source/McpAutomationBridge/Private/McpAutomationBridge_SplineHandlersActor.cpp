#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SplineHandlersPrivate.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

bool HandleCreateSplineActor(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringFieldSpline(Payload, TEXT("actorName"), TEXT("SplineActor"));
    FVector Location = GetJsonVectorFieldSpline(Payload, TEXT("location"));
    FRotator Rotation = GetJsonRotatorFieldSpline(Payload, TEXT("rotation"));
    bool bClosedLoop = GetJsonBoolFieldSpline(Payload, TEXT("bClosedLoop"), false);
    FString SplineType = GetJsonStringFieldSpline(Payload, TEXT("splineType"), TEXT("Curve"));

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *ActorName;
    SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* NewActor = World->SpawnActor<AActor>(AActor::StaticClass(), Location, Rotation, SpawnParams);
    if (!NewActor)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn spline actor"), nullptr, TEXT("SPAWN_FAILED"));
        return true;
    }

    NewActor->SetActorLabel(*ActorName);

    USplineComponent* SplineComp = NewObject<USplineComponent>(NewActor, TEXT("SplineComponent"));
    if (!SplineComp)
    {
        NewActor->Destroy();
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to create spline component"), nullptr, TEXT("COMPONENT_FAILED"));
        return true;
    }

    SplineComp->RegisterComponent();
    NewActor->AddInstanceComponent(SplineComp);
    SplineComp->SetClosedLoop(bClosedLoop);
    NewActor->SetRootComponent(SplineComp);

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
        for (int32 i = 0; i < PointsArray->Num(); i++)
        {
            const TSharedPtr<FJsonObject>* PointObj;
            if ((*PointsArray)[i]->TryGetObject(PointObj))
            {
                FVector PointLocation = GetJsonVectorFieldSpline(*PointObj, TEXT("location"));
                SplineComp->AddSplinePoint(PointLocation, ESplineCoordinateSpace::Local, true);
                SplineComp->SetSplinePointType(i, PointType, false);
            }
        }
        SplineComp->UpdateSpline();
    }

    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), NewActor->GetActorLabel());
    Result->SetStringField(TEXT("actorPath"), NewActor->GetPathName());
    Result->SetNumberField(TEXT("pointCount"), SplineComp->GetNumberOfSplinePoints());
    Result->SetNumberField(TEXT("splineLength"), SplineComp->GetSplineLength());
    Result->SetBoolField(TEXT("closedLoop"), SplineComp->IsClosedLoop());
    McpHandlerUtils::AddVerification(Result, NewActor);

    Self->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Spline actor '%s' created with %d points"), *ActorName, SplineComp->GetNumberOfSplinePoints()), Result);
    return true;
}
#endif
