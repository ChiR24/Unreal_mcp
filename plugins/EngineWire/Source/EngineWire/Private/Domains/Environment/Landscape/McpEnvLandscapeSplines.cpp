#include "Domains/Environment/McpEnvShared.h"
#include "Foundation/McpScopedEditorTransaction.h"

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
    const float SideFalloff = FMath::Max(0.0f, static_cast<float>(SideFalloffValue));

    bool bClosedLoop = false;
    McpTryGetBoolFromPayloadOrSettings(Payload, TEXT("closedLoop"), bClosedLoop);
    McpTryGetBoolFromPayloadOrSettings(Payload, TEXT("ClosedLoop"), bClosedLoop);

    const FString LayerName = McpGetFirstStringField(Payload, {TEXT("layerName")});
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

    // One undo step for the splines and the edit layer they shape the terrain through. The landscape actor joins it
    // only when the splines component or that layer is added: after an undo the engine rebuilds the layer from the
    // splines, clearing where it last drew them, and a restored copy of that record would miss the newest drawing.
    TUniquePtr<FMcpScopedEditorTransaction> Undo = MakeUnique<FMcpScopedEditorTransaction>(
        FText::FromString(TEXT("Configure Landscape Splines")), EMcpMutationDurability::EditorStateOnly, TArray<UObject *>());
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
    if (!SplinesComponent->IsRegistered())
    {
        SplinesComponent->RegisterComponent();
    }
    // Control points live in the splines component's space, as the Landscape mode's spline tool places them: the
    // landscape's origin and rotation without its scale, which CreateSplineComponent undoes. The landscape's own
    // transform put each point a hundredth of the way from the landscape's origin, and a hundredth as deep.
    const FTransform SplineToWorld = SplinesComponent->GetComponentTransform();

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

        ControlPoint->Location = SplineToWorld.InverseTransformPosition(WorldPoint);
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

    SplinesComponent->RebuildAllSplines(false);
    SplinesComponent->MarkRenderStateDirty();
    SplinesComponent->MarkPackageDirty();
    Landscape->MarkPackageDirty();
    Landscape->PostEditChange();
    bool bMerge = false;
    const bool bApplied = McpApplyLandscapeSplinesToTerrain(Landscape, Resp, bMerge);
    const bool bShaped = bApplied && (bRaiseTerrain || bLowerTerrain);
    if (bApplied && !bShaped)
    {
        Resp->SetStringField(TEXT("terrainNote"), TEXT("raiseTerrain and lowerTerrain are both false, so the heights stay as they are."));
    }
    Resp->SetBoolField(TEXT("terrainShaped"), bShaped);

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
    Undo->DescribeInto(Resp);
    // The merge draws the splines into their layer and the layers into the terrain, outside the undo step, as the
    // Landscape mode does on its next tick: so the reply and the next raycast already see the shaped ground.
    Undo.Reset();
    if (bMerge)
    {
        Landscape->ForceUpdateLayersContent();
    }
    McpHandlerUtils::AddVerification(Resp, Landscape);
    OutMessage = bShaped ? FString(TEXT("Landscape splines replaced; the terrain under them is shaped to them"))
                         : FString(TEXT("Landscape splines replaced; the terrain is not shaped (terrainNote says why)"));
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
    const FVector Location = ExtractVectorField(Payload, TEXT("location"), Landscape->GetActorLocation());
    const FRotator Rotation = ExtractRotatorField(Payload, TEXT("rotation"), Landscape->GetActorRotation());

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
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 2
    Proxy->SetLandscapeGuid(Landscape->GetLandscapeGuid());
    Proxy->SetLandscapeActor(Landscape);
    Proxy->GetSharedProperties(Landscape);
    Proxy->CreateLandscapeInfo(false);
#else
    Proxy->SetLandscapeGuid(Landscape->GetLandscapeGuid());
    Proxy->LandscapeActor = Landscape;
    Proxy->GetSharedProperties(Landscape);
    Proxy->CreateLandscapeInfo(false);
#endif
    Proxy->MarkPackageDirty();
    Proxy->PostEditChange();

    Resp->SetStringField(TEXT("actorName"), McpActorRef(Proxy));
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
