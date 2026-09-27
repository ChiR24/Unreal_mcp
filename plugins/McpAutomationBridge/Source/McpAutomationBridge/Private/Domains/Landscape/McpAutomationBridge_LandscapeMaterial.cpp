#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Landscape/McpAutomationBridge_LandscapeLookup.h"

#include "Dom/JsonObject.h"
#include "EditorAssetLibrary.h"
#include "Landscape.h"
#include "Materials/Material.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

bool UMcpAutomationBridgeSubsystem::HandleSetLandscapeMaterial(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  FString MaterialPath;
  if (!Payload->TryGetStringField(TEXT("materialPath"), MaterialPath) ||
      MaterialPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("materialPath required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString SafeMaterialPath = SanitizeProjectRelativePath(MaterialPath);
  if (SafeMaterialPath.IsEmpty()) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Invalid or unsafe material path: %s"),
                        *MaterialPath),
        TEXT("SECURITY_VIOLATION"));
    return true;
  }
  MaterialPath = SafeMaterialPath;

  ALandscape *Landscape = McpLandscapeHandlers::ResolveLandscapeOrReply(
      *this, RequestId, Payload, RequestingSocket);
  if (!Landscape) {
    return true;
  }

  UMaterialInterface *Mat = Cast<UMaterialInterface>(
      StaticLoadObject(UMaterialInterface::StaticClass(), nullptr,
                       *MaterialPath, nullptr, LOAD_NoWarn));
  if (!Mat) {
    if (!UEditorAssetLibrary::DoesAssetExist(MaterialPath)) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Material asset not found: %s"),
                          *MaterialPath),
          TEXT("ASSET_NOT_FOUND"));
    } else {
      SendAutomationError(
          RequestingSocket, RequestId,
          TEXT("Failed to load material (invalid type?)"),
          TEXT("LOAD_FAILED"));
    }
    return true;
  }

  Landscape->LandscapeMaterial = Mat;
  Landscape->PostEditChange();
  TSharedPtr<FJsonObject> Resp =
      McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("landscapePath"),
                       Landscape->GetPackage()->GetPathName());
  Resp->SetStringField(TEXT("landscapeName"),
                       Landscape->GetActorLabel());
  Resp->SetStringField(TEXT("materialPath"), MaterialPath);
  SendAutomationResponse(
      RequestingSocket, RequestId, true,
      TEXT("Landscape material set"), Resp, FString());
  return true;
}
