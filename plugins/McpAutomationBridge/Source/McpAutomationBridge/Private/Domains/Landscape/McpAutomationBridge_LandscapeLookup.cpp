#include "Domains/Landscape/McpAutomationBridge_LandscapeLookup.h"

#include "EngineUtils.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "McpAutomationBridgeSubsystem.h"

namespace McpLandscapeHandlers {
static ALandscape *FindLandscapeForEdit(const FString &LandscapePath,
                                        const FString &LandscapeName) {
  if (UWorld *World = McpHandlerUtils::GetEditorWorld()) {
    for (TActorIterator<ALandscape> It(World); It; ++It) {
      if ((!LandscapeName.IsEmpty() &&
           It->GetActorLabel().Equals(LandscapeName, ESearchCase::IgnoreCase)) ||
          (!LandscapePath.IsEmpty() &&
           It->GetPackage()->GetPathName().Equals(LandscapePath,
                                                  ESearchCase::IgnoreCase))) {
        return *It;
      }
    }
  }
  return LandscapePath.IsEmpty()
             ? nullptr
             : Cast<ALandscape>(StaticLoadObject(ALandscape::StaticClass(),
                                                 nullptr, *LandscapePath));
}

ALandscape *ResolveLandscapeOrReply(UMcpAutomationBridgeSubsystem &Bridge,
                                    const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
                                    ULandscapeInfo **OutInfo) {
  FString LandscapePath = GetJsonStringField(Payload, TEXT("landscapePath"));
  const FString LandscapeName = GetJsonStringField(Payload, TEXT("landscapeName"));
  if (!LandscapePath.IsEmpty()) {
    const FString SafePath = SanitizeProjectRelativePath(LandscapePath);
    if (SafePath.IsEmpty()) {
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Invalid or unsafe landscape path: %s"), *LandscapePath),
          TEXT("SECURITY_VIOLATION"));
      return nullptr;
    }
    LandscapePath = SafePath;
  }
  ALandscape *Landscape = FindLandscapeForEdit(LandscapePath, LandscapeName);
  if (!Landscape) {
    Bridge.SendAutomationError(
        RequestingSocket, RequestId,
        LandscapeName.IsEmpty()
            ? FString::Printf(TEXT("Landscape not found at path: %s"), *LandscapePath)
            : FString::Printf(TEXT("Landscape '%s' not found (path: %s)"), *LandscapeName, *LandscapePath),
        TEXT("LANDSCAPE_NOT_FOUND"));
    return nullptr;
  }
  if (OutInfo && !(*OutInfo = Landscape->GetLandscapeInfo())) {
    Bridge.SendAutomationError(RequestingSocket, RequestId,
                               TEXT("Landscape has no info"),
                               TEXT("INVALID_LANDSCAPE"));
    return nullptr;
  }
  return Landscape;
}
}
