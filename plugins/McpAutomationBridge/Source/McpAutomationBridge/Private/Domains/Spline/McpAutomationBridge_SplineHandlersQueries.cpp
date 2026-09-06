#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Spline/McpAutomationBridge_SplineHandlersPrivate.h"

#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

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
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));

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
        // The contract requires splines[] in every reply (dogfood #211): wrap the single actor.
        TSharedPtr<FJsonObject> SelfObj = McpHandlerUtils::CreateResultObject();
        SelfObj->SetStringField(TEXT("actorName"), ActorName);
        SelfObj->SetNumberField(TEXT("pointCount"), SplineComp->GetNumberOfSplinePoints());
        SelfObj->SetNumberField(TEXT("splineLength"), SplineComp->GetSplineLength());
        SelfObj->SetBoolField(TEXT("closedLoop"), SplineComp->IsClosedLoop());
        SelfObj->SetArrayField(TEXT("points"), PointsArray);
        TArray<TSharedPtr<FJsonValue>> SingleSpline;
        SingleSpline.Add(MakeShared<FJsonValueObject>(SelfObj));
        Result->SetArrayField(TEXT("splines"), SingleSpline);
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
                // Same label can live in the persistent level and a streamed sub-level: identify each actor (dogfood #211).
                ActorObj->SetStringField(TEXT("actorPath"), Actor->GetPathName());
                ActorObj->SetStringField(TEXT("level"), Actor->GetLevel() ? Actor->GetLevel()->GetOutermost()->GetName() : TEXT(""));
                ActorObj->SetNumberField(TEXT("splineComponentCount"), SplineComponents.Num());

                if (SplineComponents[0])
                {
                    ActorObj->SetNumberField(TEXT("pointCount"), SplineComponents[0]->GetNumberOfSplinePoints());
                    ActorObj->SetNumberField(TEXT("splineLength"), SplineComponents[0]->GetSplineLength());
                    TArray<TSharedPtr<FJsonValue>> Points;
                    const int32 PointTotal = SplineComponents[0]->GetNumberOfSplinePoints();
                    for (int32 PointIndex = 0; PointIndex < PointTotal && PointIndex < 64; ++PointIndex)
                    {
                        const FVector Location = SplineComponents[0]->GetLocationAtSplinePoint(PointIndex, ESplineCoordinateSpace::World);
                        TSharedPtr<FJsonObject> PointObj = MakeShared<FJsonObject>();
                        PointObj->SetNumberField(TEXT("index"), PointIndex);
                        PointObj->SetNumberField(TEXT("x"), Location.X);
                        PointObj->SetNumberField(TEXT("y"), Location.Y);
                        PointObj->SetNumberField(TEXT("z"), Location.Z);
                        Points.Add(MakeShared<FJsonValueObject>(PointObj));
                    }
                    ActorObj->SetArrayField(TEXT("points"), Points);
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
