#include "McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
namespace McpEnvironmentHandlers {

bool McpConfigureLandscapeSplines(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                         FString &OutMessage, FString &OutErrorCode)
{
    ALandscape *Landscape = McpFindLandscapeForEnvironmentAction(Payload);
    if (!Landscape)
    {
        OutMessage = TEXT("Landscape not found for spline configuration");
        OutErrorCode = TEXT("LANDSCAPE_NOT_FOUND");
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>> *Points = nullptr;
    if (!Payload->TryGetArrayField(TEXT("points"), Points) || !Points || Points->Num() < 2)
    {
        OutMessage = TEXT("configure_landscape_splines requires at least two points");
        OutErrorCode = TEXT("INVALID_ARGUMENT");
        return false;
    }

    double WidthValue = 256.0;
    Payload->TryGetNumberField(TEXT("width"), WidthValue);
    const float Width = FMath::Max(0.0f, static_cast<float>(WidthValue));

    double SideFalloffValue = WidthValue * 0.5;
    Payload->TryGetNumberField(TEXT("sideFalloff"), SideFalloffValue);
    const float SideFalloff = FMath::Max(0.0f, static_cast<float>(SideFalloffValue));

    bool bClosedLoop = false;
    McpTryGetBoolFromPayloadOrSettings(Payload, TEXT("closedLoop"), bClosedLoop);
    McpTryGetBoolFromPayloadOrSettings(Payload, TEXT("ClosedLoop"), bClosedLoop);

    const FString LayerName = McpGetFirstStringField(Payload, {TEXT("layerName"), TEXT("splineLayerName")});
    bool bRaiseTerrain = true;
    bool bLowerTerrain = true;
    McpTryGetBoolFromPayloadOrSettings(Payload, TEXT("raiseTerrain"), bRaiseTerrain);
    McpTryGetBoolFromPayloadOrSettings(Payload, TEXT("lowerTerrain"), bLowerTerrain);

    TArray<FVector> WorldPoints;
    WorldPoints.Reserve(Points->Num());
    for (int32 Index = 0; Index < Points->Num(); ++Index)
    {
        FVector WorldPoint = FVector::ZeroVector;
        if (!McpReadLandscapeSplinePoint((*Points)[Index], WorldPoint))
        {
            OutMessage = FString::Printf(TEXT("Invalid landscape spline point at index %d"), Index);
            OutErrorCode = TEXT("INVALID_ARGUMENT");
            return false;
        }
        WorldPoints.Add(WorldPoint);
    }

    ULandscapeSplinesComponent *SplinesComponent = Landscape->GetSplinesComponent();
    if (!SplinesComponent)
    {
        Landscape->Modify();
        Landscape->CreateSplineComponent();
        SplinesComponent = Landscape->GetSplinesComponent();
    }
    if (!SplinesComponent)
    {
        OutMessage = TEXT("Failed to create landscape splines component");
        OutErrorCode = TEXT("COMPONENT_CREATION_FAILED");
        return false;
    }

    TArray<ULandscapeSplineControlPoint *> ControlPoints;
    ControlPoints.Reserve(WorldPoints.Num());
    for (const FVector &WorldPoint : WorldPoints)
    {
        ULandscapeSplineControlPoint *ControlPoint = NewObject<ULandscapeSplineControlPoint>(SplinesComponent, NAME_None, RF_Transactional);
        if (!ControlPoint)
        {
            OutMessage = TEXT("Failed to create landscape spline control point");
            OutErrorCode = TEXT("CREATION_FAILED");
            return false;
        }

        ControlPoint->Location = Landscape->GetTransform().InverseTransformPosition(WorldPoint);
        ControlPoint->Width = Width;
        ControlPoint->SideFalloff = SideFalloff;
        ControlPoint->EndFalloff = SideFalloff;
        ControlPoint->bRaiseTerrain = bRaiseTerrain;
        ControlPoint->bLowerTerrain = bLowerTerrain;
        if (!LayerName.IsEmpty())
        {
            ControlPoint->LayerName = FName(*LayerName);
        }
        ControlPoints.Add(ControlPoint);
    }

    Landscape->Modify();
    SplinesComponent->Modify();
    McpClearLandscapeSplines(SplinesComponent);
    TArray<TObjectPtr<ULandscapeSplineControlPoint>> *SplineControlPoints = McpGetLandscapeSplineControlPoints(SplinesComponent);
    if (!SplineControlPoints)
    {
        OutMessage = TEXT("Landscape splines component control points are unavailable");
        OutErrorCode = TEXT("COMPONENT_UNAVAILABLE");
        return false;
    }
    for (ULandscapeSplineControlPoint *ControlPoint : ControlPoints)
    {
        ControlPoint->Modify();
        SplineControlPoints->Add(ControlPoint);
    }

    TArray<ULandscapeSplineSegment *> Segments;
    for (int32 Index = 0; Index < ControlPoints.Num() - 1; ++Index)
    {
        if (ULandscapeSplineSegment *Segment = McpAddLandscapeSplineSegment(SplinesComponent, ControlPoints[Index], ControlPoints[Index + 1]))
        {
            Segment->bRaiseTerrain = bRaiseTerrain;
            Segment->bLowerTerrain = bLowerTerrain;
            if (!LayerName.IsEmpty())
            {
                Segment->LayerName = FName(*LayerName);
            }
            Segments.Add(Segment);
        }
    }
    if (bClosedLoop && ControlPoints.Num() > 2)
    {
        if (ULandscapeSplineSegment *Segment = McpAddLandscapeSplineSegment(SplinesComponent, ControlPoints.Last(), ControlPoints[0]))
        {
            Segment->bRaiseTerrain = bRaiseTerrain;
            Segment->bLowerTerrain = bLowerTerrain;
            if (!LayerName.IsEmpty())
            {
                Segment->LayerName = FName(*LayerName);
            }
            Segments.Add(Segment);
        }
    }

    for (ULandscapeSplineControlPoint *ControlPoint : ControlPoints)
    {
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4)
        ControlPoint->AutoCalcRotation(false);
#else
        ControlPoint->AutoCalcRotation();
#endif
        ControlPoint->UpdateSplinePoints(false, true, false);
    }
    for (ULandscapeSplineSegment *Segment : Segments)
    {
        Segment->UpdateSplinePoints(false, false);
    }

    if (!SplinesComponent->IsRegistered())
    {
        SplinesComponent->RegisterComponent();
    }
    SplinesComponent->RebuildAllSplines(false);
    SplinesComponent->MarkRenderStateDirty();
    SplinesComponent->MarkPackageDirty();
    Landscape->MarkPackageDirty();
    Landscape->PostEditChange();

    Resp->SetStringField(TEXT("landscapeName"), Landscape->GetActorLabel());
    Resp->SetStringField(TEXT("landscapePath"), Landscape->GetPackage() ? Landscape->GetPackage()->GetPathName() : Landscape->GetPathName());
    Resp->SetStringField(TEXT("actorPath"), Landscape->GetPathName());
    Resp->SetStringField(TEXT("componentName"), SplinesComponent->GetName());
    Resp->SetStringField(TEXT("componentPath"), SplinesComponent->GetPathName());
    Resp->SetNumberField(TEXT("pointCount"), ControlPoints.Num());
    Resp->SetNumberField(TEXT("segmentCount"), Segments.Num());
    Resp->SetNumberField(TEXT("width"), Width);
    Resp->SetBoolField(TEXT("closedLoop"), bClosedLoop);
    Resp->SetBoolField(TEXT("raiseTerrain"), bRaiseTerrain);
    Resp->SetBoolField(TEXT("lowerTerrain"), bLowerTerrain);
    if (!LayerName.IsEmpty())
    {
        Resp->SetStringField(TEXT("layerName"), LayerName);
    }
    McpHandlerUtils::AddVerification(Resp, Landscape);
    OutMessage = TEXT("Landscape spline configuration updated");
    return true;
}
bool McpCreateLandscapeStreamingProxy(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                            FString &OutMessage, FString &OutErrorCode)
{
    ALandscape *Landscape = McpFindLandscapeForEnvironmentAction(Payload);
    if (!Landscape)
    {
        OutMessage = TEXT("Landscape not found for streaming proxy creation");
        OutErrorCode = TEXT("LANDSCAPE_NOT_FOUND");
        return false;
    }

    const FString ActorName = McpGetFirstStringField(Payload, {TEXT("targetActor"), TEXT("actorName"), TEXT("name")});
    const FVector Location = McpGetVectorField(Payload, TEXT("location"), Landscape->GetActorLocation());
    const FRotator Rotation = McpGetRotatorField(Payload, TEXT("rotation"), Landscape->GetActorRotation());

    ALandscapeStreamingProxy *Proxy = Cast<ALandscapeStreamingProxy>(
        McpFindOrSpawnActor(ALandscapeStreamingProxy::StaticClass(), ActorName.IsEmpty() ? TEXT("LandscapeStreamingProxy") : ActorName, Location, Rotation));
    if (!Proxy)
    {
        OutMessage = TEXT("Failed to create landscape streaming proxy");
        OutErrorCode = TEXT("SPAWN_FAILED");
        return false;
    }

    Landscape->Modify();
    Proxy->Modify();
    Proxy->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
    Proxy->SetLandscapeGuid(Landscape->GetLandscapeGuid(), false);
    Proxy->SetLandscapeActor(Landscape);
    Proxy->CopySharedProperties(Landscape);
    Proxy->CreateLandscapeInfo(false, false);
#else
    Proxy->SetLandscapeGuid(Landscape->GetLandscapeGuid());
    Proxy->LandscapeActor = Landscape;
    Proxy->GetSharedProperties(Landscape);
    Proxy->CreateLandscapeInfo(false);
#endif
    Proxy->MarkPackageDirty();
    Proxy->PostEditChange();

    Resp->SetStringField(TEXT("actorName"), Proxy->GetActorLabel());
    Resp->SetStringField(TEXT("actorPath"), Proxy->GetPathName());
    Resp->SetStringField(TEXT("sourceLandscapeName"), Landscape->GetActorLabel());
    Resp->SetStringField(TEXT("sourceLandscapePath"), Landscape->GetPackage() ? Landscape->GetPackage()->GetPathName() : Landscape->GetPathName());
    Resp->SetStringField(TEXT("sourceLandscapeActorPath"), Landscape->GetPathName());
    Resp->SetStringField(TEXT("landscapeGuid"), Landscape->GetLandscapeGuid().ToString());
    Resp->SetBoolField(TEXT("linkedToLandscape"), Proxy->GetLandscapeActor() == Landscape);
    McpHandlerUtils::AddVerification(Resp, Proxy);
    OutMessage = TEXT("Landscape streaming proxy created");
    return true;
}

} // namespace McpEnvironmentHandlers
#endif
