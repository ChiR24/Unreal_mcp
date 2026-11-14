#pragma once

// The landscape work build_environment routes here (heightmap files, layer info assets, landscape splines, streaming
// proxies) and the spline helpers those files share. The landscape a payload names comes from McpLandscapeLookup.h.
#include "Domains/Landscape/McpLandscapeLookup.h"
#include "UObject/ObjectPtr.h"

class FJsonValue;
class ULandscapeLayerInfoObject;
class UPhysicalMaterial;
class ULandscapeSplineControlPoint;
class ULandscapeSplineSegment;
class ULandscapeSplinesComponent;

namespace McpLandscapeHandlers {
// A heightmap action's region in Out*, from `region` {minX, minY, maxX, maxY} or the same four at the top level, each
// unset one at the landscape's full extent and all clamped to it. A negative vertex (a landscape grown with the Add
// tool) is a vertex like any other.
void McpApplyHeightmapRegionFromPayload(const TSharedPtr<FJsonObject> &Payload, int32 FullMinX, int32 FullMinY,
                                        int32 FullMaxX, int32 FullMaxY, int32 &OutMinX, int32 &OutMinY,
                                        int32 &OutMaxX, int32 &OutMaxY);
bool McpExportLandscapeHeightmap(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                 FString &OutMessage, FString &OutErrorCode);
bool McpImportLandscapeHeightmap(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                 FString &OutMessage, FString &OutErrorCode);
bool McpCreateLandscapeLayerInfo(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                 FString &OutMessage, FString &OutErrorCode);
// Names a layer info object's layer: SetLayerName from 5.7, the LayerName field before it.
void SetLayerInfoName(ULandscapeLayerInfoObject *LayerInfo, FName LayerName);
// A new layer info object named LayerName at PackagePath (a path its caller validated), with Payload's hardness and
// noWeightBlend and PhysicalMaterial (null for none), announced to the asset registry and not saved; null with
// McpCreatePackageForNewAsset's refusal in OutError and OutCode when the package cannot be made.
ULandscapeLayerInfoObject *McpNewLandscapeLayerInfo(const FString &PackagePath, const FString &LayerName,
                                                    const TSharedPtr<FJsonObject> &Payload, UPhysicalMaterial *PhysicalMaterial,
                                                    FString &OutError, FString &OutCode);
bool McpConfigureLandscapeSplines(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                  FString &OutMessage, FString &OutErrorCode);
bool McpCreateLandscapeStreamingProxy(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                      FString &OutMessage, FString &OutErrorCode);
bool McpReadLandscapeSplinePoint(const TSharedPtr<FJsonValue> &PointValue, FVector &OutPoint);
TArray<TObjectPtr<ULandscapeSplineControlPoint>> *McpGetLandscapeSplineControlPoints(ULandscapeSplinesComponent *SplinesComponent);
void McpClearLandscapeSplines(ULandscapeSplinesComponent *SplinesComponent);
ULandscapeSplineSegment *McpAddLandscapeSplineSegment(ULandscapeSplinesComponent *SplinesComponent,
                                                      ULandscapeSplineControlPoint *Start,
                                                      ULandscapeSplineControlPoint *End);
// Applies the landscape's splines to its terrain (inside the caller's undo step) and says how in Resp; false when they
// reach none. bOutMerge: the layers must merge, after that undo step closes, for the terrain to show it.
bool McpApplyLandscapeSplinesToTerrain(ALandscape *Landscape, const TSharedPtr<FJsonObject> &Resp, bool &bOutMerge);
} // namespace McpLandscapeHandlers
