#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_SplineHandlersPrivate.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

bool HandleGetSplinesInfo(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString ActorName = GetJsonStringFieldSpline(Payload, TEXT("actorName"));

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Self->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();

    if (!ActorName.IsEmpty())
    {
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

        Result->SetStringField(TEXT("actorName"), ActorName);
        Result->SetNumberField(TEXT("pointCount"), SplineComp->GetNumberOfSplinePoints());
        Result->SetNumberField(TEXT("splineLength"), SplineComp->GetSplineLength());
        Result->SetBoolField(TEXT("closedLoop"), SplineComp->IsClosedLoop());

        TArray<TSharedPtr<FJsonValue>> PointsArray;
        for (int32 i = 0; i < SplineComp->GetNumberOfSplinePoints(); i++)
        {
            TSharedPtr<FJsonObject> PointObj = McpHandlerUtils::CreateResultObject();
            FVector Loc = SplineComp->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::Local);

            PointObj->SetNumberField(TEXT("index"), i);

            TSharedPtr<FJsonObject> LocObj = McpHandlerUtils::CreateResultObject();
            LocObj->SetNumberField(TEXT("x"), Loc.X);
            LocObj->SetNumberField(TEXT("y"), Loc.Y);
            LocObj->SetNumberField(TEXT("z"), Loc.Z);
            PointObj->SetObjectField(TEXT("location"), LocObj);
            PointObj->SetStringField(TEXT("type"), SplinePointTypeToString(SplineComp->GetSplinePointType(i)));

            PointsArray.Add(MakeShared<FJsonValueObject>(PointObj));
        }
        Result->SetArrayField(TEXT("points"), PointsArray);
    }
    else
    {
        TArray<TSharedPtr<FJsonValue>> SplinesArray;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Actor = *It;
            TArray<USplineComponent*> SplineComponents;
            Actor->GetComponents<USplineComponent>(SplineComponents);

            if (SplineComponents.Num() > 0)
            {
                TSharedPtr<FJsonObject> ActorObj = McpHandlerUtils::CreateResultObject();
                ActorObj->SetStringField(TEXT("actorName"), Actor->GetActorLabel());
                ActorObj->SetNumberField(TEXT("splineComponentCount"), SplineComponents.Num());

                if (SplineComponents[0])
                {
                    ActorObj->SetNumberField(TEXT("pointCount"), SplineComponents[0]->GetNumberOfSplinePoints());
                    ActorObj->SetNumberField(TEXT("splineLength"), SplineComponents[0]->GetSplineLength());
                }

                SplinesArray.Add(MakeShared<FJsonValueObject>(ActorObj));
            }
        }
        Result->SetArrayField(TEXT("splines"), SplinesArray);
        Result->SetNumberField(TEXT("totalSplineActors"), SplinesArray.Num());
    }

    Self->SendAutomationResponse(Socket, RequestId, true,
        TEXT("Spline info retrieved"), Result);
    return true;
}
#endif
