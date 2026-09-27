#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Landscape/McpAutomationBridge_LandscapeLookup.h"

#include "Dom/JsonObject.h"
#include "Landscape.h"
#include "LandscapeEdit.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Misc/ScopedSlowTask.h"

bool UMcpAutomationBridgeSubsystem::HandlePaintLandscapeLayer(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  FString LayerName;
  if (!Payload->TryGetStringField(TEXT("layerName"), LayerName) ||
      LayerName.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("layerName required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  int32 MinX = -1, MinY = -1, MaxX = -1, MaxY = -1;
  const TSharedPtr<FJsonObject> *RegionObj = nullptr;
  if (Payload->TryGetObjectField(TEXT("region"), RegionObj) && RegionObj) {
    (*RegionObj)->TryGetNumberField(TEXT("minX"), MinX);
    (*RegionObj)->TryGetNumberField(TEXT("minY"), MinY);
    (*RegionObj)->TryGetNumberField(TEXT("maxX"), MaxX);
    (*RegionObj)->TryGetNumberField(TEXT("maxY"), MaxY);
  } else {
    Payload->TryGetNumberField(TEXT("minX"), MinX);
    Payload->TryGetNumberField(TEXT("minY"), MinY);
    Payload->TryGetNumberField(TEXT("maxX"), MaxX);
    Payload->TryGetNumberField(TEXT("maxY"), MaxY);
  }

  double Strength = 1.0;
  Payload->TryGetNumberField(TEXT("strength"), Strength);
  Strength = FMath::Clamp(Strength, 0.0, 1.0);
  bool bSkipFlush = false;
  Payload->TryGetBoolField(TEXT("skipFlush"), bSkipFlush);

  ULandscapeInfo *LandscapeInfo = nullptr;
  ALandscape *Landscape = McpLandscapeHandlers::ResolveLandscapeOrReply(
      *this, RequestId, Payload, RequestingSocket, &LandscapeInfo);
  if (!Landscape) {
    return true;
  }

  ULandscapeLayerInfoObject *LayerInfo = nullptr;
  for (const FLandscapeInfoLayerSettings &Layer :
       LandscapeInfo->Layers) {
    if (Layer.LayerName == FName(*LayerName)) {
      LayerInfo = Layer.LayerInfoObj;
      break;
    }
  }
  if (!LayerInfo) {
    ULandscapeLayerInfoObject *NewLayerInfo =
        NewObject<ULandscapeLayerInfoObject>(
            Landscape,
            FName(*FString::Printf(TEXT("LayerInfo_%s"),
                                   *LayerName)),
            RF_Public | RF_Transactional);
    if (!NewLayerInfo) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Failed to create layer '%s'"),
                          *LayerName),
          TEXT("LAYER_CREATION_FAILED"));
      return true;
    }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
    NewLayerInfo->SetLayerName(FName(*LayerName), true);
#else
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    NewLayerInfo->LayerName = FName(*LayerName);
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
    LandscapeInfo->Layers.Add(
        FLandscapeInfoLayerSettings(NewLayerInfo, Landscape));
    LayerInfo = NewLayerInfo;
  }

  FScopedSlowTask SlowTask(
      1.0f,
      FText::FromString(TEXT("Painting landscape layer...")));
  int32 PaintMinX = MinX;
  int32 PaintMinY = MinY;
  int32 PaintMaxX = MaxX;
  int32 PaintMaxY = MaxY;
  int32 LMinX, LMinY, LMaxX, LMaxY;
  if (LandscapeInfo->GetLandscapeExtent(LMinX, LMinY, LMaxX,
                                        LMaxY)) {
    PaintMinX = FMath::Clamp(PaintMinX, LMinX, LMaxX);
    PaintMinY = FMath::Clamp(PaintMinY, LMinY, LMaxY);
    PaintMaxX = FMath::Clamp(PaintMaxX, LMinX, LMaxX);
    PaintMaxY = FMath::Clamp(PaintMaxY, LMinY, LMaxY);
  }
  if (PaintMinX > PaintMaxX || PaintMinY > PaintMaxY) {
    SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("Invalid paint region: min > max after clamping"),
        TEXT("INVALID_REGION"));
    return true;
  }

  FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo, false);
  const uint8 PaintValue =
      static_cast<uint8>(Strength * 255.0);
  const int32 RegionSizeX = PaintMaxX - PaintMinX + 1;
  const int32 RegionSizeY = PaintMaxY - PaintMinY + 1;
  constexpr int32 MaxRegionPixels = 16777216;
  if (RegionSizeX * RegionSizeY > MaxRegionPixels) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(
            TEXT("Paint region too large: %dx%d (%d pixels). Maximum: %d"),
            RegionSizeX, RegionSizeY, RegionSizeX * RegionSizeY,
            MaxRegionPixels),
        TEXT("REGION_TOO_LARGE"));
    return true;
  }

  TArray<uint8> AlphaData;
  AlphaData.Init(PaintValue, RegionSizeX * RegionSizeY);
  LandscapeEdit.SetAlphaData(LayerInfo, PaintMinX, PaintMinY,
                             PaintMaxX, PaintMaxY,
                             AlphaData.GetData(), RegionSizeX);
  if (!bSkipFlush) {
    LandscapeEdit.Flush();
  }
  Landscape->MarkPackageDirty();

  TSharedPtr<FJsonObject> Resp =
      McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("landscapePath"),
                       Landscape->GetPackage()->GetPathName());
  Resp->SetStringField(TEXT("landscapeName"),
                       Landscape->GetActorLabel());
  Resp->SetStringField(TEXT("layerName"), LayerName);
  Resp->SetNumberField(TEXT("strength"), Strength);
  SendAutomationResponse(
      RequestingSocket, RequestId, true,
      TEXT("Layer painted successfully"), Resp, FString());
  return true;
}
