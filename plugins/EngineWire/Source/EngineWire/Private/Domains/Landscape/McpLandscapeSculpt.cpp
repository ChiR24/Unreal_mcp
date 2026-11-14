#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"

#include "Domains/Landscape/McpLandscapeLookup.h"

#include "Dom/JsonObject.h"
#include "Landscape.h"
#include "LandscapeEdit.h"
#include "LandscapeInfo.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

bool UEngineWireSubsystem::HandleSculptLandscape(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  if (!Payload->HasTypedField<EJson::Object>(TEXT("location"))) {
    SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("location required. Example: {\"location\": {\"x\": 0, \"y\": 0, \"z\": 100}}"),
        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString ToolMode = GetJsonStringField(Payload, TEXT("tool"));
  ToolMode = ToolMode.IsEmpty() ? FString(TEXT("Raise")) : ToolMode;
  // Any other tool (Smooth, Noise) left every vertex untouched and still reported success.
  if (!ToolMode.Equals(TEXT("Raise"), ESearchCase::IgnoreCase) && !ToolMode.Equals(TEXT("Lower"), ESearchCase::IgnoreCase) &&
      !ToolMode.Equals(TEXT("Flatten"), ESearchCase::IgnoreCase)) {
    SendAutomationError(RequestingSocket, RequestId,
                        FString::Printf(TEXT("Unsupported sculpt tool '%s': use Raise, Lower or Flatten"), *ToolMode),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }
  double BrushRadius = 1000.0;
  Payload->TryGetNumberField(TEXT("radius"), BrushRadius);
  double BrushFalloff = 0.5;
  Payload->TryGetNumberField(TEXT("falloff"), BrushFalloff);
  // The outer share of the radius that fades, held to 0-1: above 1 the fade reached the centre (2 halved it there).
  BrushFalloff = FMath::Clamp(BrushFalloff, 0.0, 1.0);
  double Strength = GetJsonNumberField(Payload, TEXT("strength"), 0.1);
  bool bSkipFlush = false;
  Payload->TryGetBoolField(TEXT("skipFlush"), bSkipFlush);

  const FVector TargetLocation = ExtractVectorField(Payload, TEXT("location"), FVector::ZeroVector);
  ULandscapeInfo *LandscapeInfo = nullptr;
  ALandscape *Landscape = McpLandscapeHandlers::ResolveLandscapeOrReply(
      *this, RequestId, Payload, RequestingSocket, &LandscapeInfo);
  if (!Landscape) {
    return true;
  }

  FVector LocalPos = Landscape->GetActorTransform()
                         .InverseTransformPosition(TargetLocation);
  // Centre and radius are held to sizes no landscape reaches, so the brush box stays inside an int32 for any
  // location or radius sent.
  int32 CenterX = FMath::RoundToInt(FMath::Clamp(LocalPos.X, -1.0e8, 1.0e8));
  int32 CenterY = FMath::RoundToInt(FMath::Clamp(LocalPos.Y, -1.0e8, 1.0e8));
  const FVector LandscapeScale = Landscape->GetActorScale3D();
  const float ScaleX = LandscapeScale.X;
  const float ScaleZ = LandscapeScale.Z;
  if (FMath::IsNearlyZero(ScaleX) ||
      FMath::IsNearlyZero(ScaleZ)) {
    SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("Landscape has zero scale. Cannot perform brush operation."),
        TEXT("INVALID_SCALE"));
    return true;
  }

  int32 RadiusVerts =
      FMath::Max(1, FMath::RoundToInt(FMath::Min(BrushRadius / ScaleX, 1.0e6)));
  int32 FalloffVerts = FMath::RoundToInt(RadiusVerts * BrushFalloff);
  int32 MinX = CenterX - RadiusVerts;
  int32 MaxX = CenterX + RadiusVerts;
  int32 MinY = CenterY - RadiusVerts;
  int32 MaxY = CenterY + RadiusVerts;
  int32 LMinX, LMinY, LMaxX, LMaxY;
  // No extent (a World Partition landscape with nothing loaded) left the box sized from the radius alone, and the
  // height buffer below from that. Refused as the heightmap and erosion edits refuse it.
  if (!LandscapeInfo->GetLandscapeExtent(LMinX, LMinY, LMaxX, LMaxY)) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to get landscape extent"), TEXT("INVALID_LANDSCAPE"));
    return true;
  }
  MinX = FMath::Max(MinX, LMinX);
  MinY = FMath::Max(MinY, LMinY);
  MaxX = FMath::Min(MaxX, LMaxX);
  MaxY = FMath::Min(MaxY, LMaxY);
  if (MinX > MaxX || MinY > MaxY) {
    SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("Brush outside landscape bounds"),
        TEXT("OUT_OF_BOUNDS"));
    return true;
  }

  int32 SizeX = MaxX - MinX + 1;
  int32 SizeY = MaxY - MinY + 1;
  TArray<uint16> HeightData;
  HeightData.SetNumZeroed(SizeX * SizeY);
  // Without a layer and the upload (false), the drawn landscape and its collision kept the old shape.
  const McpLandscapeHandlers::FMcpLandscapeEditScope Edit(Landscape, TEXT("Sculpt Landscape"), bSkipFlush);
  FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
  LandscapeEdit.GetHeightData(MinX, MinY, MaxX, MaxY,
                              HeightData.GetData(), 0);

  // The tool and the scales, decided once: the tool was compared as text three times for every vertex.
  const bool bFlatten = ToolMode.Equals(TEXT("Flatten"), ESearchCase::IgnoreCase);
  const float RaiseSign = ToolMode.Equals(TEXT("Lower"), ESearchCase::IgnoreCase) ? -1.0f : 1.0f;
  const float HeightScale = 128.0f / ScaleZ;
  const float FlattenTarget = (TargetLocation.Z - Landscape->GetActorLocation().Z) / ScaleZ * 128.0f + 32768.0f;
  // modifiedVertices used to report the whole brush bounding box
  // whenever ANY vertex moved, so a brush that nudged three
  // vertices claimed thousands and a fully clamped brush was
  // indistinguishable from a real edit. Count the writes.
  int32 ModifiedVertices = 0;
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
      const int32 Index = (Y - MinY) * SizeX + (X - MinX);
      const uint16 CurrentHeight = HeightData[Index];
      const float Delta = bFlatten ? (FlattenTarget - (float)CurrentHeight) * Strength * Alpha
                                   : RaiseSign * Strength * Alpha * 100.0f * HeightScale;

      int32 NewHeight =
          FMath::Clamp((int32)(CurrentHeight + Delta), 0, 65535);
      if (NewHeight != CurrentHeight) {
        HeightData[Index] = (uint16)NewHeight;
        ++ModifiedVertices;
      }
    }
  }

  if (ModifiedVertices > 0) {
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
  Resp->SetNumberField(TEXT("modifiedVertices"), ModifiedVertices);
  Resp->SetNumberField(TEXT("brushVertices"), HeightData.Num());
  Edit.DescribeInto(Resp);
  McpHandlerUtils::AddVerification(Resp, Landscape);
  SendAutomationResponse(RequestingSocket, RequestId,
                                    true,
                                    TEXT("Landscape sculpted"),
                                    Resp, FString());
  return true;
}
