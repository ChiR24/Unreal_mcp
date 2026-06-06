#include "McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
namespace McpEnvironmentHandlers {

bool McpPayloadHasWaterWaveSettings(const TSharedPtr<FJsonObject> &Payload)
{
    double NumberValue = 0.0;
    const TSharedPtr<FJsonObject> *DirectionObj = nullptr;
    return McpGetFirstNumberField(Payload, {TEXT("waveHeight"), TEXT("waveLength"), TEXT("amplitude")}, NumberValue) ||
           (Payload.IsValid() && Payload->TryGetObjectField(TEXT("direction"), DirectionObj) && DirectionObj && DirectionObj->IsValid());
}
bool McpTryGetNumberFromPayloadOrSettings(const TSharedPtr<FJsonObject> &Payload, const TCHAR *FieldName, double &OutValue)
{
    if (!Payload.IsValid())
    {
        return false;
    }

    if (Payload->TryGetNumberField(FieldName, OutValue))
    {
        return true;
    }

    const TSharedPtr<FJsonObject> *SettingsObj = nullptr;
    return Payload->TryGetObjectField(TEXT("settings"), SettingsObj) && SettingsObj && SettingsObj->IsValid() &&
           (*SettingsObj)->TryGetNumberField(FieldName, OutValue);
}
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
    const TSharedPtr<FJsonObject> *PointObject = nullptr;
    if (!PointValue.IsValid() || !PointValue->TryGetObject(PointObject) || !PointObject || !PointObject->IsValid())
    {
        return false;
    }

    auto TryReadVector = [](const TSharedPtr<FJsonObject> &VectorObject, FVector &OutVector) -> bool
    {
        if (!VectorObject.IsValid())
        {
            return false;
        }

        double X = 0.0;
        double Y = 0.0;
        double Z = 0.0;
        if (!VectorObject->TryGetNumberField(TEXT("x"), X) ||
            !VectorObject->TryGetNumberField(TEXT("y"), Y) ||
            !VectorObject->TryGetNumberField(TEXT("z"), Z))
        {
            return false;
        }

        OutVector = FVector(X, Y, Z);
        return true;
    };

    const TSharedPtr<FJsonObject> *LocationObject = nullptr;
    if ((*PointObject)->TryGetObjectField(TEXT("location"), LocationObject) && LocationObject && LocationObject->IsValid())
    {
        return TryReadVector(*LocationObject, OutPoint);
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

} // namespace McpEnvironmentHandlers
#endif
