#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Landscape/McpLandscapeLookup.h"
#include "Domains/Landscape/McpLandscapeHeightmapNullRhi.h"

#include "Dom/JsonObject.h"
#include "Landscape.h"
#include "LandscapeEdit.h"
#include "LandscapeInfo.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ScopedSlowTask.h"

bool UEngineWireSubsystem::HandleModifyHeightmap(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  FString Operation = TEXT("set");
  Payload->TryGetStringField(TEXT("operation"), Operation);
  Operation = Operation.ToLower();
  Operation = Operation == TEXT("add") ? FString(TEXT("raise")) : Operation == TEXT("subtract") ? FString(TEXT("lower")) : Operation;
  // An unknown operation (smooth and subtract among them) ran as set and wrote heightData[0] over the whole region.
  if (Operation != TEXT("set") && Operation != TEXT("raise") && Operation != TEXT("lower") && Operation != TEXT("flatten") &&
      Operation != TEXT("smooth")) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("operation must be set, raise (add), lower (subtract), flatten or smooth."),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }
  // raise and lower moved by |heightData[0] - 32768| / 10 raw units, which nothing said; they take amount in cm.
  const bool bRaiseLower = Operation == TEXT("raise") || Operation == TEXT("lower");
  double AmountCm = 0.0;
  if (bRaiseLower && (!Payload->TryGetNumberField(TEXT("amount"), AmountCm) || AmountCm <= 0.0)) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("raise and lower need amount: the centimetres to move the region, above 0."),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  int32 RegionMinX = -1, RegionMinY = -1, RegionMaxX = -1, RegionMaxY = -1;
  const TSharedPtr<FJsonObject> *RegionObj = nullptr;
  if (Payload->TryGetObjectField(TEXT("region"), RegionObj) && RegionObj) {
    (*RegionObj)->TryGetNumberField(TEXT("minX"), RegionMinX);
    (*RegionObj)->TryGetNumberField(TEXT("minY"), RegionMinY);
    (*RegionObj)->TryGetNumberField(TEXT("maxX"), RegionMaxX);
    (*RegionObj)->TryGetNumberField(TEXT("maxY"), RegionMaxY);
  } else {
    Payload->TryGetNumberField(TEXT("minX"), RegionMinX);
    Payload->TryGetNumberField(TEXT("minY"), RegionMinY);
    Payload->TryGetNumberField(TEXT("maxX"), RegionMaxX);
    Payload->TryGetNumberField(TEXT("maxY"), RegionMaxY);
  }

  const TArray<TSharedPtr<FJsonValue>> *HeightDataArray = nullptr;
  const bool bHasHeightData =
      Payload->TryGetArrayField(TEXT("heightData"), HeightDataArray) &&
      HeightDataArray && HeightDataArray->Num() > 0;
  if (!bHasHeightData &&
      Operation.Equals(TEXT("set"), ESearchCase::IgnoreCase)) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("heightData array required for 'set' operation"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  bool bSkipFlush = false;
  Payload->TryGetBoolField(TEXT("skipFlush"), bSkipFlush);
  bool bUpdateNormals = false;
  Payload->TryGetBoolField(TEXT("updateNormals"), bUpdateNormals);

  TArray<uint16> HeightValues;
  if (bHasHeightData) {
    for (const TSharedPtr<FJsonValue> &Val : *HeightDataArray) {
      if (Val.IsValid() && Val->Type == EJson::Number) {
        HeightValues.Add(
            static_cast<uint16>(FMath::Clamp(Val->AsNumber(), 0.0, 65535.0)));
      }
    }
  }

  ULandscapeInfo *LandscapeInfo = nullptr;
  ALandscape *Landscape = McpLandscapeHandlers::ResolveLandscapeOrReply(
      *this, RequestId, Payload, RequestingSocket, &LandscapeInfo);
  if (!Landscape) {
    return true;
  }

  if (FParse::Param(FCommandLine::Get(), TEXT("NullRHI"))) {
    McpLandscapeHeightmap::SendNullRhiValidatedResponse(
        *this, RequestId, RequestingSocket, Landscape,
        Operation, {RegionMinX, RegionMinY, RegionMaxX, RegionMaxY});
    return true;
  }

  FScopedSlowTask SlowTask(
      3.0f, FText::FromString(TEXT("Modifying heightmap...")));
  int32 FullMinX, FullMinY, FullMaxX, FullMaxY;
  if (!LandscapeInfo->GetLandscapeExtent(FullMinX, FullMinY,
                                         FullMaxX, FullMaxY)) {
    SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("Failed to get landscape extent"),
        TEXT("INVALID_LANDSCAPE"));
    return true;
  }

  int32 MinX = (RegionMinX >= 0) ? RegionMinX : FullMinX;
  int32 MinY = (RegionMinY >= 0) ? RegionMinY : FullMinY;
  int32 MaxX = (RegionMaxX >= 0) ? RegionMaxX : FullMaxX;
  int32 MaxY = (RegionMaxY >= 0) ? RegionMaxY : FullMaxY;
  MinX = FMath::Clamp(MinX, FullMinX, FullMaxX);
  MinY = FMath::Clamp(MinY, FullMinY, FullMaxY);
  MaxX = FMath::Clamp(MaxX, FullMinX, FullMaxX);
  MaxY = FMath::Clamp(MaxY, FullMinY, FullMaxY);

  if (MinX > MaxX || MinY > MaxY) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(
            TEXT("Invalid heightmap region: min (%d, %d) must not exceed max (%d, %d)"),
            MinX, MinY, MaxX, MaxY),
        TEXT("INVALID_REGION"));
    return true;
  }

  const int32 SizeX = (MaxX - MinX + 1);
  const int32 SizeY = (MaxY - MinY + 1);
  const int32 RegionSize = SizeX * SizeY;
  // Any count but one value or one per vertex filled the whole region with the first value.
  if (Operation == TEXT("set") && HeightValues.Num() != 1 && HeightValues.Num() != RegionSize) {
    SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("heightData has %d values; the region (%d x %d) takes 1 for all of "
        "it or %d, one per vertex row by row. Nothing was changed."), HeightValues.Num(), SizeX, SizeY, RegionSize), TEXT("INVALID_ARGUMENT"));
    return true;
  }
  SlowTask.EnterProgressFrame(
      1.0f,
      FText::FromString(TEXT("Reading current heightmap data")));
  const McpLandscapeHandlers::FMcpLandscapeEditScope Edit(Landscape, TEXT("Modify Landscape Heightmap"), bSkipFlush);
  TArray<uint16> CurrentHeights;
  CurrentHeights.SetNumZeroed(RegionSize);
  FLandscapeEditDataInterface LandscapeEditRead(LandscapeInfo,
                                                false);
  LandscapeEditRead.GetHeightData(MinX, MinY, MaxX, MaxY,
                                  CurrentHeights.GetData(), 0);
  TArray<uint16> OutputHeights;
  OutputHeights.SetNumUninitialized(RegionSize);
  const uint16 SingleValue =
      HeightValues.Num() > 0 ? HeightValues[0] : 32768;
  // 128 raw units are one landscape Z-scale step (ScaleZ cm).
  const int32 DeltaRaw = (Operation == TEXT("lower") ? -1 : 1) *
      FMath::RoundToInt(AmountCm * 128.0 / FMath::Max(static_cast<double>(Landscape->GetActorScale3D().Z), 0.001));
  int32 ModifiedCount = 0;
  for (int32 i = 0; i < RegionSize; ++i) {
    int32 NewHeight = CurrentHeights[i];
    if (bRaiseLower) {
      NewHeight += DeltaRaw;
    } else if (Operation == TEXT("flatten")) {
      NewHeight = SingleValue;
    } else if (Operation == TEXT("smooth")) {
      // One 3x3 averaging pass over the region's own vertices.
      int32 Sum = 0, Count = 0;
      for (int32 DY = -1; DY <= 1; ++DY) {
        for (int32 DX = -1; DX <= 1; ++DX) {
          const int32 NX = i % SizeX + DX, NY = i / SizeX + DY;
          if (NX >= 0 && NX < SizeX && NY >= 0 && NY < SizeY) {
            Sum += CurrentHeights[NY * SizeX + NX];
            ++Count;
          }
        }
      }
      NewHeight = Sum / Count;
    } else {
      NewHeight = HeightValues.Num() == RegionSize ? HeightValues[i] : SingleValue;
    }
    OutputHeights[i] = static_cast<uint16>(FMath::Clamp(NewHeight, 0, 65535));
    // Counted every vertex in the region, so a no-op edit still
    // reported the full region as modified.
    ModifiedCount += OutputHeights[i] != CurrentHeights[i] ? 1 : 0;
  }

  SlowTask.EnterProgressFrame(
      1.0f,
      FText::FromString(TEXT("Writing heightmap data")));
  // Not uploading (false) left the GPU copy, and so the drawn landscape and every layer merge, on the old heights.
  FLandscapeEditDataInterface LandscapeEditWrite(LandscapeInfo);
  LandscapeEditWrite.SetHeightData(
      MinX, MinY, MaxX, MaxY, OutputHeights.GetData(), SizeX,
      bUpdateNormals);
  if (!bSkipFlush) {
    SlowTask.EnterProgressFrame(
        1.0f,
        FText::FromString(TEXT("Flushing changes to GPU")));
    LandscapeEditWrite.Flush();
  }
  Landscape->MarkPackageDirty();

  TSharedPtr<FJsonObject> Resp =
      McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("landscapePath"),
                       Landscape->GetPackage()->GetPathName());
  Resp->SetStringField(TEXT("landscapeName"),
                       Landscape->GetActorLabel());
  Resp->SetStringField(TEXT("operation"), Operation);
  if (bRaiseLower) {
    Resp->SetNumberField(TEXT("amountCm"), AmountCm);
  }
  Resp->SetNumberField(TEXT("modifiedVertices"), ModifiedCount);
  Resp->SetNumberField(TEXT("regionVertices"), RegionSize);
  Resp->SetNumberField(TEXT("regionSizeX"), SizeX);
  Resp->SetNumberField(TEXT("regionSizeY"), SizeY);
  Resp->SetBoolField(TEXT("flushSkipped"), bSkipFlush);
  Resp->SetBoolField(TEXT("updateNormals"), bUpdateNormals);
  Edit.DescribeInto(Resp);
  McpHandlerUtils::AddVerification(Resp, Landscape);
  SendAutomationResponse(
      RequestingSocket, RequestId, true,
      TEXT("Heightmap modified successfully"), Resp, FString());
  return true;
}
