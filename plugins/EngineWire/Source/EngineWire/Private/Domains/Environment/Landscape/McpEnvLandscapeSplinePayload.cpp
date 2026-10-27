#include "Domains/Environment/McpEnvShared.h"
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5)
#include "LandscapeEditLayer.h"
#endif

namespace McpEnvironmentHandlers {
bool McpTryGetBoolFromPayloadOrSettings(const TSharedPtr<FJsonObject> &Payload, const TCHAR *FieldName, bool &OutValue)
{
    if (!Payload.IsValid())
    {
        return false;
    }

    if (Payload->TryGetBoolField(FieldName, OutValue))
    {
        return true;
    }

    const TSharedPtr<FJsonObject> *SettingsObj = nullptr;
    return Payload->TryGetObjectField(TEXT("settings"), SettingsObj) && SettingsObj && SettingsObj->IsValid() &&
           (*SettingsObj)->TryGetBoolField(FieldName, OutValue);
}
bool McpReadLandscapeSplinePoint(const TSharedPtr<FJsonValue> &PointValue, FVector &OutPoint)
{
    if (!PointValue.IsValid())
    {
        return false;
    }

    // Accept [x, y, z] (z optional) as well as objects.
    const TArray<TSharedPtr<FJsonValue>> *PointArray = nullptr;
    if (PointValue->TryGetArray(PointArray) && PointArray && PointArray->Num() >= 2)
    {
        double Components[3] = {0.0, 0.0, 0.0};
        for (int32 Index = 0; Index < 3 && Index < PointArray->Num(); ++Index)
        {
            if (!(*PointArray)[Index].IsValid() || !(*PointArray)[Index]->TryGetNumber(Components[Index]))
            {
                return false;
            }
        }
        OutPoint = FVector(Components[0], Components[1], Components[2]);
        return true;
    }

    const TSharedPtr<FJsonObject> *PointObject = nullptr;
    if (!PointValue->TryGetObject(PointObject) || !PointObject || !PointObject->IsValid())
    {
        return false;
    }

    // {x, y[, z]} in either case; z defaults to 0 (the landscape snaps it).
    auto TryReadVector = [](const TSharedPtr<FJsonObject> &VectorObject, FVector &OutVector) -> bool
    {
        if (!VectorObject.IsValid())
        {
            return false;
        }

        double X = 0.0;
        double Y = 0.0;
        double Z = 0.0;
        const bool bHasX = VectorObject->TryGetNumberField(TEXT("x"), X) || VectorObject->TryGetNumberField(TEXT("X"), X);
        const bool bHasY = VectorObject->TryGetNumberField(TEXT("y"), Y) || VectorObject->TryGetNumberField(TEXT("Y"), Y);
        if (!bHasX || !bHasY)
        {
            return false;
        }
        if (!VectorObject->TryGetNumberField(TEXT("z"), Z))
        {
            VectorObject->TryGetNumberField(TEXT("Z"), Z);
        }

        OutVector = FVector(X, Y, Z);
        return true;
    };

    const TSharedPtr<FJsonObject> *LocationObject = nullptr;
    for (const TCHAR *Key : {TEXT("location"), TEXT("position"), TEXT("point")})
    {
        if ((*PointObject)->TryGetObjectField(Key, LocationObject) && LocationObject && LocationObject->IsValid())
        {
            return TryReadVector(*LocationObject, OutPoint);
        }
    }

    return TryReadVector(*PointObject, OutPoint);
}
TArray<TObjectPtr<ULandscapeSplineSegment>> *McpGetLandscapeSplineSegments(ULandscapeSplinesComponent *SplinesComponent)
{
    if (!SplinesComponent)
    {
        return nullptr;
    }

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
    return &SplinesComponent->GetSegments();
#else
    FArrayProperty *SegmentsProperty = FindFProperty<FArrayProperty>(SplinesComponent->GetClass(), TEXT("Segments"));
    return SegmentsProperty
               ? SegmentsProperty->ContainerPtrToValuePtr<TArray<TObjectPtr<ULandscapeSplineSegment>>>(SplinesComponent)
               : nullptr;
#endif
}
TArray<TObjectPtr<ULandscapeSplineControlPoint>> *McpGetLandscapeSplineControlPoints(ULandscapeSplinesComponent *SplinesComponent)
{
    if (!SplinesComponent)
    {
        return nullptr;
    }

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3)
    return &SplinesComponent->GetControlPoints();
#else
    FArrayProperty *ControlPointsProperty = FindFProperty<FArrayProperty>(SplinesComponent->GetClass(), TEXT("ControlPoints"));
    return ControlPointsProperty
               ? ControlPointsProperty->ContainerPtrToValuePtr<TArray<TObjectPtr<ULandscapeSplineControlPoint>>>(SplinesComponent)
               : nullptr;
#endif
}
void McpClearLandscapeSplines(ULandscapeSplinesComponent *SplinesComponent)
{
    if (!SplinesComponent)
    {
        return;
    }

    TArray<TObjectPtr<ULandscapeSplineSegment>> *Segments = McpGetLandscapeSplineSegments(SplinesComponent);
    TArray<TObjectPtr<ULandscapeSplineControlPoint>> *ControlPoints = McpGetLandscapeSplineControlPoints(SplinesComponent);
    if (!Segments || !ControlPoints)
    {
        return;
    }

    for (TObjectPtr<ULandscapeSplineSegment> &SegmentPtr : *Segments)
    {
        if (ULandscapeSplineSegment *Segment = SegmentPtr.Get())
        {
            Segment->Modify();
            Segment->DeleteSplinePoints();
        }
    }
    for (TObjectPtr<ULandscapeSplineControlPoint> &ControlPointPtr : *ControlPoints)
    {
        if (ULandscapeSplineControlPoint *ControlPoint = ControlPointPtr.Get())
        {
            ControlPoint->Modify();
            ControlPoint->DeleteSplinePoints();
        }
    }

    Segments->Empty();
    ControlPoints->Empty();
}
ULandscapeSplineSegment *McpAddLandscapeSplineSegment(ULandscapeSplinesComponent *SplinesComponent,
                                                            ULandscapeSplineControlPoint *Start,
                                                            ULandscapeSplineControlPoint *End)
{
    if (!SplinesComponent || !Start || !End || Start == End)
    {
        return nullptr;
    }

    TArray<TObjectPtr<ULandscapeSplineSegment>> *Segments = McpGetLandscapeSplineSegments(SplinesComponent);
    if (!Segments)
    {
        return nullptr;
    }

    ULandscapeSplineSegment *Segment = NewObject<ULandscapeSplineSegment>(SplinesComponent, NAME_None, RF_Transactional);
    if (!Segment)
    {
        return nullptr;
    }

    Segments->Add(Segment);
    Segment->Connections[0].ControlPoint = Start;
    Segment->Connections[1].ControlPoint = End;
    Segment->Connections[0].SocketName = Start->GetBestConnectionTo(End->Location);
    Segment->Connections[1].SocketName = End->GetBestConnectionTo(Start->Location);

    FVector StartLocation;
    FRotator StartRotation;
    FVector EndLocation;
    FRotator EndRotation;
    Start->GetConnectionLocationAndRotation(Segment->Connections[0].SocketName, StartLocation, StartRotation);
    End->GetConnectionLocationAndRotation(Segment->Connections[1].SocketName, EndLocation, EndRotation);
    Segment->Connections[0].TangentLen = static_cast<float>((EndLocation - StartLocation).Size());
    Segment->Connections[1].TangentLen = Segment->Connections[0].TangentLen;
    Segment->AutoFlipTangents();

    Start->ConnectedSegments.Add(FLandscapeSplineConnection(Segment, 0));
    End->ConnectedSegments.Add(FLandscapeSplineConnection(Segment, 1));
    return Segment;
}

namespace {
// The edit layer reserved for landscape splines: the layer of the splines class from 5.5, the reserved guid before.
const FLandscapeLayer *McpFindLandscapeSplinesLayer(ALandscape *Landscape)
{
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
    return Landscape->FindLayerOfTypeConst(ULandscapeEditLayerSplines::StaticClass());
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 5
    return Landscape->FindLayerOfType(ULandscapeEditLayerSplines::StaticClass());
#else
    return Landscape->GetLandscapeSplinesReservedLayer();
#endif
}
} // namespace

// With edit layers the engine applies splines only to the edit layer reserved for them, rebuilt from the splines at
// the next merge (bOutMerge: the caller's, after its undo step), so the splines changed no heights until that layer
// existed. It is found, else added as the Landscape mode's layer list adds it; a layer that holds sculpting is never
// reserved, which clears it. Without edit layers (5.6 and older) the splines are written into the heights, as Apply
// Splines does.
bool McpApplyLandscapeSplinesToTerrain(ALandscape *Landscape, const TSharedPtr<FJsonObject> &Resp, bool &bOutMerge)
{
    ULandscapeInfo *Info = Landscape->GetLandscapeInfo();
    bOutMerge = false;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 7
    if (!Landscape->HasLayersContent())
    {
        TSet<TObjectPtr<ULandscapeComponent>> Shaped;
        const bool bApplied = Info && Info->ApplySplines(false, &Shaped) && Shaped.Num() > 0;
        Resp->SetStringField(TEXT("terrainNote"), bApplied
            ? TEXT("The landscape has no edit layers, so the splines were written into its heights: a later call does not take them back (undo does).")
            : TEXT("The splines cross none of the landscape."));
        return bApplied;
    }
#endif
    const FLandscapeLayer *Layer = McpFindLandscapeSplinesLayer(Landscape);
    const bool bCreated = !Layer && !Landscape->IsMaxLayersReached();
    if (bCreated)
    {
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5)
        Landscape->CreateLayer(FName(TEXT("Splines")), ULandscapeEditLayerSplines::StaticClass());
#else
        const int32 LayerIndex = Landscape->CreateLayer(FName(TEXT("Splines")));
        if (LayerIndex != INDEX_NONE)
        {
            Landscape->SetLandscapeSplinesReservedLayer(LayerIndex);
        }
#endif
        Layer = McpFindLandscapeSplinesLayer(Landscape);
    }
    const TCHAR *Note = nullptr;
    if (!Layer)
    {
        Note = bCreated ? TEXT("The engine refused to add a Splines edit layer, the only layer splines shape the terrain through.")
                        : TEXT("The landscape already has as many edit layers as Project Settings > Landscape > Max Number Of Layers allows, so no Splines layer, the only layer splines shape the terrain through, could be added: delete a layer or raise that setting, then call again.");
    }
    else if (!Info || !Info->AreAllComponentsRegistered())
    {
        Note = TEXT("Part of the landscape is not loaded, and the engine shapes the terrain to splines only when all of it is: load it all (World Partition), then call again.");
    }
    else if (!FApp::CanEverRender())
    {
        Note = TEXT("The editor runs without rendering, so the edit layers are never merged into the terrain.");
    }
    if (Layer)
    {
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
        Resp->SetStringField(TEXT("splinesLayer"), Layer->EditLayer ? Layer->EditLayer->GetName().ToString() : FString());
#else
        Resp->SetStringField(TEXT("splinesLayer"), Layer->Name.ToString());
#endif
        Resp->SetBoolField(TEXT("splinesLayerCreated"), bCreated);
        Landscape->RequestSplineLayerUpdate();
    }
    if (Note)
    {
        Resp->SetStringField(TEXT("terrainNote"), Note);
    }
    bOutMerge = Layer && !Note;
    return bOutMerge;
}

} // namespace McpEnvironmentHandlers
