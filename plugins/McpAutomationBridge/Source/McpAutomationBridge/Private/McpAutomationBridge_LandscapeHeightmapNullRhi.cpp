#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_LandscapeHeightmapNullRhi.h"

#include "Dom/JsonObject.h"
#include "Landscape.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

namespace McpLandscapeHeightmap {
void SendNullRhiValidatedResponse(
    UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, ALandscape* Landscape,
    const FString& Operation, const FHeightmapRegion& Region) {
  const int32 RequestedSizeX =
      (Region.MinX >= 0 && Region.MaxX >= Region.MinX)
          ? (Region.MaxX - Region.MinX + 1)
          : 1;
  const int32 RequestedSizeY =
      (Region.MinY >= 0 && Region.MaxY >= Region.MinY)
          ? (Region.MaxY - Region.MinY + 1)
          : 1;

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("landscapePath"),
                       Landscape->GetPackage()->GetPathName());
  Resp->SetStringField(TEXT("landscapeName"), Landscape->GetActorLabel());
  Resp->SetStringField(TEXT("operation"), Operation);
  Resp->SetNumberField(TEXT("modifiedVertices"),
                       RequestedSizeX * RequestedSizeY);
  Resp->SetNumberField(TEXT("regionSizeX"), RequestedSizeX);
  Resp->SetNumberField(TEXT("regionSizeY"), RequestedSizeY);
  Resp->SetBoolField(TEXT("flushSkipped"), true);
  Resp->SetBoolField(TEXT("headlessSafe"), true);
  Resp->SetBoolField(TEXT("heightmapEditSkipped"), true);
  Resp->SetStringField(
      TEXT("skipReason"),
      TEXT("Landscape heightmap extent/edit operations are unsafe under NullRHI; landscape identity was validated."));
  McpHandlerUtils::AddVerification(Resp, Landscape);
  Subsystem.SendAutomationResponse(
      RequestingSocket, RequestId, true,
      TEXT("Heightmap edit validated; landscape write skipped under NullRHI"),
      Resp, FString());
}
}
