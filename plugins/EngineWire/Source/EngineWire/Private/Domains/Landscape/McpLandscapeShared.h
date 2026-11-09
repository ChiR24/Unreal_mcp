#pragma once

// The landscape work build_environment routes here (heightmap files, layer info assets, landscape splines, streaming
// proxies) and the spline helpers those files share. The landscape a payload names comes from McpLandscapeLookup.h.
#include "Domains/Landscape/McpLandscapeLookup.h"
#include "UObject/ObjectPtr.h"

class FJsonValue;
class ULandscapeLayerInfoObject;
class ULandscapeSplineControlPoint;
class ULandscapeSplineSegment;
class ULandscapeSplinesComponent;

namespace McpLandscapeHandlers {
bool McpExportLandscapeHeightmap(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                 FString &OutMessage, FString &OutErrorCode);
bool McpImportLandscapeHeightmap(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                 FString &OutMessage, FString &OutErrorCode);
bool McpCreateLandscapeLayerInfo(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                 FString &OutMessage, FString &OutErrorCode);
// Names a layer info object's layer: SetLayerName from 5.7, the LayerName field before it.
void SetLayerInfoName(ULandscapeLayerInfoObject *LayerInfo, FName LayerName);
bool McpConfigureLandscapeSplines(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                  FString &OutMessage, FString &OutErrorCode);
bool McpCreateLandscapeStreamingProxy(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                      FString &OutMessage, FString &OutErrorCode);
bool McpTryGetBoolFromPayloadOrSettings(const TSharedPtr<FJsonObject> &Payload, const TCHAR *FieldName, bool &OutValue);
bool McpReadLandscapeSplinePoint(const TSharedPtr<FJsonValue> &PointValue, FVector &OutPoint);
TArray<TObjectPtr<ULandscapeSplineSegment>> *McpGetLandscapeSplineSegments(ULandscapeSplinesComponent *SplinesComponent);
TArray<TObjectPtr<ULandscapeSplineControlPoint>> *McpGetLandscapeSplineControlPoints(ULandscapeSplinesComponent *SplinesComponent);
void McpClearLandscapeSplines(ULandscapeSplinesComponent *SplinesComponent);
ULandscapeSplineSegment *McpAddLandscapeSplineSegment(ULandscapeSplinesComponent *SplinesComponent,
                                                      ULandscapeSplineControlPoint *Start,
                                                      ULandscapeSplineControlPoint *End);
// Applies the landscape's splines to its terrain (inside the caller's undo step) and says how in Resp; false when they
// reach none. bOutMerge: the layers must merge, after that undo step closes, for the terrain to show it.
bool McpApplyLandscapeSplinesToTerrain(ALandscape *Landscape, const TSharedPtr<FJsonObject> &Resp, bool &bOutMerge);
} // namespace McpLandscapeHandlers
