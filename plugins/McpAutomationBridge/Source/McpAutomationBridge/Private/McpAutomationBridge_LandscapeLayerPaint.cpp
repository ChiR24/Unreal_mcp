#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_LandscapeLookup.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Landscape.h"
#include "LandscapeEdit.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"
#include "Misc/ScopedSlowTask.h"

bool UMcpAutomationBridgeSubsystem::HandlePaintLandscapeLayer(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("paint_landscape_layer"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("paint_landscape_layer payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString LandscapePath;
  Payload->TryGetStringField(TEXT("landscapePath"), LandscapePath);
  FString LandscapeName;
  Payload->TryGetStringField(TEXT("landscapeName"), LandscapeName);
  if (!LandscapePath.IsEmpty()) {
    FString SafePath = SanitizeProjectRelativePath(LandscapePath);
    if (SafePath.IsEmpty()) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Invalid or unsafe landscape path: %s"),
                          *LandscapePath),
          TEXT("SECURITY_VIOLATION"));
      return true;
    }
    LandscapePath = SafePath;
  }

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

  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(this);
  AsyncTask(ENamedThreads::GameThread,
            [WeakSubsystem, RequestId, RequestingSocket, LandscapePath,
             LandscapeName, LayerName, MinX, MinY, MaxX, MaxY, Strength,
             bSkipFlush]() {
              UMcpAutomationBridgeSubsystem *Subsystem = WeakSubsystem.Get();
              if (!Subsystem)
                return;

              ALandscape *Landscape =
                  McpLandscapeHandlers::FindLandscapeForEdit(LandscapePath,
                                                             LandscapeName);
              if (!Landscape) {
                const FString ErrorMessage =
                    McpLandscapeHandlers::MakeLandscapeNotFoundMessage(
                        LandscapePath, LandscapeName);
                Subsystem->SendAutomationError(RequestingSocket, RequestId,
                                               *ErrorMessage,
                                               TEXT("LANDSCAPE_NOT_FOUND"));
                return;
              }

              ULandscapeInfo *LandscapeInfo = Landscape->GetLandscapeInfo();
              if (!LandscapeInfo) {
                Subsystem->SendAutomationError(RequestingSocket, RequestId,
                                               TEXT("Landscape has no info"),
                                               TEXT("INVALID_LANDSCAPE"));
                return;
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
                  Subsystem->SendAutomationError(
                      RequestingSocket, RequestId,
                      FString::Printf(TEXT("Failed to create layer '%s'"),
                                      *LayerName),
                      TEXT("LAYER_CREATION_FAILED"));
                  return;
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
                Subsystem->SendAutomationError(
                    RequestingSocket, RequestId,
                    TEXT("Invalid paint region: min > max after clamping"),
                    TEXT("INVALID_REGION"));
                return;
              }

              FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo, false);
              const uint8 PaintValue =
                  static_cast<uint8>(Strength * 255.0);
              const int32 RegionSizeX = PaintMaxX - PaintMinX + 1;
              const int32 RegionSizeY = PaintMaxY - PaintMinY + 1;
              constexpr int32 MaxRegionPixels = 16777216;
              if (RegionSizeX * RegionSizeY > MaxRegionPixels) {
                Subsystem->SendAutomationError(
                    RequestingSocket, RequestId,
                    FString::Printf(
                        TEXT("Paint region too large: %dx%d (%d pixels). Maximum: %d"),
                        RegionSizeX, RegionSizeY, RegionSizeX * RegionSizeY,
                        MaxRegionPixels),
                    TEXT("REGION_TOO_LARGE"));
                return;
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
              Subsystem->SendAutomationResponse(
                  RequestingSocket, RequestId, true,
                  TEXT("Layer painted successfully"), Resp, FString());
            });
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("paint_landscape_layer requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
