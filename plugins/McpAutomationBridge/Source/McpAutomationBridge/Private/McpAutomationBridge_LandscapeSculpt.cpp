#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_LandscapeLookup.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Landscape.h"
#include "LandscapeEdit.h"
#include "LandscapeInfo.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpLandscapeHandlers, Log, All);

bool UMcpAutomationBridgeSubsystem::HandleSculptLandscape(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("sculpt_landscape"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("sculpt_landscape payload missing"),
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

  UE_LOG(LogMcpLandscapeHandlers, Warning,
         TEXT("HandleSculptLandscape: RequestId=%s Path='%s' Name='%s'"),
         *RequestId, *LandscapePath, *LandscapeName);

  double LocX = 0, LocY = 0, LocZ = 0;
  const TSharedPtr<FJsonObject> *LocObj = nullptr;
  if (Payload->TryGetObjectField(TEXT("location"), LocObj) && LocObj) {
    (*LocObj)->TryGetNumberField(TEXT("x"), LocX);
    (*LocObj)->TryGetNumberField(TEXT("y"), LocY);
    (*LocObj)->TryGetNumberField(TEXT("z"), LocZ);
  } else if (Payload->TryGetObjectField(TEXT("position"), LocObj) && LocObj) {
    (*LocObj)->TryGetNumberField(TEXT("x"), LocX);
    (*LocObj)->TryGetNumberField(TEXT("y"), LocY);
    (*LocObj)->TryGetNumberField(TEXT("z"), LocZ);
  } else {
    SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("location or position required. Example: {\"location\": {\"x\": 0, \"y\": 0, \"z\": 100}}"),
        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString ToolMode = TEXT("Raise");
  if (!Payload->TryGetStringField(TEXT("toolMode"), ToolMode)) {
    Payload->TryGetStringField(TEXT("tool"), ToolMode);
  }
  double BrushRadius = 1000.0;
  if (!Payload->TryGetNumberField(TEXT("brushRadius"), BrushRadius)) {
    Payload->TryGetNumberField(TEXT("radius"), BrushRadius);
  }
  double BrushFalloff = 0.5;
  if (!Payload->TryGetNumberField(TEXT("brushFalloff"), BrushFalloff)) {
    Payload->TryGetNumberField(TEXT("falloff"), BrushFalloff);
  }
  double Strength = 0.1;
  Payload->TryGetNumberField(TEXT("strength"), Strength);
  bool bSkipFlush = false;
  Payload->TryGetBoolField(TEXT("skipFlush"), bSkipFlush);

  const FVector TargetLocation(LocX, LocY, LocZ);
  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(this);
  AsyncTask(ENamedThreads::GameThread,
            [WeakSubsystem, RequestId, RequestingSocket, LandscapePath,
             LandscapeName, TargetLocation, ToolMode, BrushRadius, BrushFalloff,
             Strength, bSkipFlush]() {
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

              FVector LocalPos = Landscape->GetActorTransform()
                                     .InverseTransformPosition(TargetLocation);
              int32 CenterX = FMath::RoundToInt(LocalPos.X);
              int32 CenterY = FMath::RoundToInt(LocalPos.Y);
              const FVector LandscapeScale = Landscape->GetActorScale3D();
              const float ScaleX = LandscapeScale.X;
              const float ScaleZ = LandscapeScale.Z;
              if (FMath::IsNearlyZero(ScaleX) ||
                  FMath::IsNearlyZero(ScaleZ)) {
                Subsystem->SendAutomationError(
                    RequestingSocket, RequestId,
                    TEXT("Landscape has zero scale. Cannot perform brush operation."),
                    TEXT("INVALID_SCALE"));
                return;
              }

              int32 RadiusVerts =
                  FMath::Max(1, FMath::RoundToInt(BrushRadius / ScaleX));
              int32 FalloffVerts = FMath::RoundToInt(RadiusVerts * BrushFalloff);
              int32 MinX = CenterX - RadiusVerts;
              int32 MaxX = CenterX + RadiusVerts;
              int32 MinY = CenterY - RadiusVerts;
              int32 MaxY = CenterY + RadiusVerts;
              int32 LMinX, LMinY, LMaxX, LMaxY;
              if (LandscapeInfo->GetLandscapeExtent(LMinX, LMinY, LMaxX,
                                                    LMaxY)) {
                MinX = FMath::Max(MinX, LMinX);
                MinY = FMath::Max(MinY, LMinY);
                MaxX = FMath::Min(MaxX, LMaxX);
                MaxY = FMath::Min(MaxY, LMaxY);
              }
              if (MinX > MaxX || MinY > MaxY) {
                Subsystem->SendAutomationResponse(
                    RequestingSocket, RequestId, false,
                    TEXT("Brush outside landscape bounds"), nullptr,
                    TEXT("OUT_OF_BOUNDS"));
                return;
              }

              int32 SizeX = MaxX - MinX + 1;
              int32 SizeY = MaxY - MinY + 1;
              TArray<uint16> HeightData;
              HeightData.SetNumZeroed(SizeX * SizeY);
              FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo, false);
              LandscapeEdit.GetHeightData(MinX, MinY, MaxX, MaxY,
                                          HeightData.GetData(), 0);

              bool bModified = false;
              for (int32 Y = MinY; Y <= MaxY; ++Y) {
                for (int32 X = MinX; X <= MaxX; ++X) {
                  float Dist = FMath::Sqrt(
                      FMath::Square((float)(X - CenterX)) +
                      FMath::Square((float)(Y - CenterY)));
                  if (Dist > RadiusVerts)
                    continue;
                  float Alpha = 1.0f;
                  if (Dist > (RadiusVerts - FalloffVerts)) {
                    Alpha =
                        1.0f -
                        ((Dist - (RadiusVerts - FalloffVerts)) /
                         (float)FalloffVerts);
                  }
                  Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
                  int32 Index = (Y - MinY) * SizeX + (X - MinX);
                  if (Index < 0 || Index >= HeightData.Num())
                    continue;

                  uint16 CurrentHeight = HeightData[Index];
                  float HeightScale = 128.0f / ScaleZ;
                  float Delta = 0.0f;
                  if (ToolMode.Equals(TEXT("Raise"),
                                      ESearchCase::IgnoreCase)) {
                    Delta = Strength * Alpha * 100.0f * HeightScale;
                  } else if (ToolMode.Equals(TEXT("Lower"),
                                             ESearchCase::IgnoreCase)) {
                    Delta = -Strength * Alpha * 100.0f * HeightScale;
                  } else if (ToolMode.Equals(TEXT("Flatten"),
                                             ESearchCase::IgnoreCase)) {
                    float Target = (TargetLocation.Z -
                                    Landscape->GetActorLocation().Z) /
                                       ScaleZ * 128.0f +
                                   32768.0f;
                    Delta =
                        (Target - (float)CurrentHeight) * Strength * Alpha;
                  }

                  int32 NewHeight =
                      FMath::Clamp((int32)(CurrentHeight + Delta), 0, 65535);
                  if (NewHeight != CurrentHeight) {
                    HeightData[Index] = (uint16)NewHeight;
                    bModified = true;
                  }
                }
              }

              if (bModified) {
                LandscapeEdit.SetHeightData(MinX, MinY, MaxX, MaxY,
                                            HeightData.GetData(), 0, true);
                if (!bSkipFlush) {
                  LandscapeEdit.Flush();
                }
                Landscape->MarkPackageDirty();
              }

              TSharedPtr<FJsonObject> Resp =
                  McpHandlerUtils::CreateResultObject();
              Resp->SetBoolField(TEXT("success"), true);
              Resp->SetStringField(TEXT("toolMode"), ToolMode);
              Resp->SetNumberField(TEXT("modifiedVertices"),
                                   bModified ? HeightData.Num() : 0);
              Subsystem->SendAutomationResponse(RequestingSocket, RequestId,
                                                true,
                                                TEXT("Landscape sculpted"),
                                                Resp, FString());
            });
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("sculpt_landscape requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
