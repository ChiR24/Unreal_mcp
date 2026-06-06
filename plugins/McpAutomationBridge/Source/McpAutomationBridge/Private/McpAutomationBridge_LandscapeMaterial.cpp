#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_LandscapeLookup.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "EditorAssetLibrary.h"
#include "Landscape.h"
#include "Materials/Material.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

bool UMcpAutomationBridgeSubsystem::HandleSetLandscapeMaterial(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("set_landscape_material"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("set_landscape_material payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString LandscapePath;
  Payload->TryGetStringField(TEXT("landscapePath"), LandscapePath);
  FString LandscapeName;
  Payload->TryGetStringField(TEXT("landscapeName"), LandscapeName);
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

  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(this);
  AsyncTask(ENamedThreads::GameThread,
            [WeakSubsystem, RequestId, RequestingSocket, LandscapePath,
             LandscapeName, MaterialPath]() {
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

              UMaterialInterface *Mat = Cast<UMaterialInterface>(
                  StaticLoadObject(UMaterialInterface::StaticClass(), nullptr,
                                   *MaterialPath, nullptr, LOAD_NoWarn));
              if (!Mat) {
                if (!UEditorAssetLibrary::DoesAssetExist(MaterialPath)) {
                  Subsystem->SendAutomationError(
                      RequestingSocket, RequestId,
                      FString::Printf(TEXT("Material asset not found: %s"),
                                      *MaterialPath),
                      TEXT("ASSET_NOT_FOUND"));
                } else {
                  Subsystem->SendAutomationError(
                      RequestingSocket, RequestId,
                      TEXT("Failed to load material (invalid type?)"),
                      TEXT("LOAD_FAILED"));
                }
                return;
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
              Subsystem->SendAutomationResponse(
                  RequestingSocket, RequestId, true,
                  TEXT("Landscape material set"), Resp, FString());
            });
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("set_landscape_material requires editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
