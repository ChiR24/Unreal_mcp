#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Landscape/McpAutomationBridge_LandscapeLookup.h"
#include "Domains/Landscape/McpAutomationBridge_LandscapeHeightmapNullRhi.h"

#include "Dom/JsonObject.h"
#include "Landscape.h"
#include "LandscapeEdit.h"
#include "LandscapeInfo.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ScopedSlowTask.h"

bool UMcpAutomationBridgeSubsystem::HandleModifyHeightmap(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  FString Operation = TEXT("set");
  Payload->TryGetStringField(TEXT("operation"), Operation);
  if (Operation.Equals(TEXT("add"), ESearchCase::IgnoreCase)) {
    Operation = TEXT("raise");
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
  SlowTask.EnterProgressFrame(
      1.0f,
      FText::FromString(TEXT("Reading current heightmap data")));
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
  const int16 Delta =
      static_cast<int16>(SingleValue) - 32768;
  int32 ModifiedCount = 0;
  for (int32 i = 0; i < RegionSize; ++i) {
    uint16 NewHeight = CurrentHeights[i];
    if (Operation.Equals(TEXT("raise"), ESearchCase::IgnoreCase)) {
      NewHeight = FMath::Clamp(
          static_cast<int32>(CurrentHeights[i]) +
              FMath::Abs(Delta) / 10,
          0, 65535);
    } else if (Operation.Equals(TEXT("lower"),
                                ESearchCase::IgnoreCase)) {
      NewHeight = FMath::Clamp(
          static_cast<int32>(CurrentHeights[i]) -
              FMath::Abs(Delta) / 10,
          0, 65535);
    } else if (Operation.Equals(TEXT("flatten"),
                                ESearchCase::IgnoreCase)) {
      NewHeight = SingleValue;
    } else {
      NewHeight =
          HeightValues.Num() == RegionSize ? HeightValues[i]
                                           : SingleValue;
    }
    // Counted every vertex in the region, so a no-op edit still
    // reported the full region as modified.
    if (NewHeight != CurrentHeights[i]) {
      ModifiedCount++;
    }
    OutputHeights[i] = NewHeight;
  }

  SlowTask.EnterProgressFrame(
      1.0f,
      FText::FromString(TEXT("Writing heightmap data")));
  FLandscapeEditDataInterface LandscapeEditWrite(LandscapeInfo,
                                                 false);
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
  Resp->SetNumberField(TEXT("modifiedVertices"), ModifiedCount);
  Resp->SetNumberField(TEXT("regionVertices"), RegionSize);
  Resp->SetNumberField(TEXT("regionSizeX"), SizeX);
  Resp->SetNumberField(TEXT("regionSizeY"), SizeY);
  Resp->SetBoolField(TEXT("flushSkipped"), bSkipFlush);
  Resp->SetBoolField(TEXT("updateNormals"), bUpdateNormals);
  McpHandlerUtils::AddVerification(Resp, Landscape);
  SendAutomationResponse(
      RequestingSocket, RequestId, true,
      TEXT("Heightmap modified successfully"), Resp, FString());
  return true;
}
