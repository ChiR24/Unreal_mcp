// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpFabProvider.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"

#if WITH_EDITOR

/**
 * Adds one Fab listing to this project.
 *
 * The whole acquisition happens on Fab's side of the boundary: its signed-in
 * page resolves the listing, mints the download URL and hands it to Fab's own
 * importer. Nothing in this process ever sees the URL, the EOS token or a
 * cookie, so no receipt or log can carry them.
 *
 * Success is decided by the asset registry, not by Fab's reply. Fab accepting a
 * workflow only means a download started; the response below is emitted after
 * new packages actually appear and the registry goes quiet.
 *
 * Fab chooses the destination folder -- FPackImportWorkflow imports to the
 * pack's own name under /Game and honours no caller path -- so the result
 * reports where the content landed instead of pretending to place it. Use
 * asset.migrate_assets afterwards to relocate, accepting the referenceIntegrity
 * warning that capability documents.
 */
bool UMcpAutomationBridgeSubsystem::HandleAddFabAssetToProject(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  IMcpFabProvider *Provider = GetMcpFabProvider();
  if (Provider == nullptr || !Provider->IsFabAvailable()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("Fab support is not loaded in this editor, so listings cannot be added."),
        nullptr, TEXT("NOT_SUPPORTED"));
    return true;
  }

  FString ListingId;
  if (!Payload->TryGetStringField(TEXT("listingId"), ListingId) || ListingId.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("'listingId' is required."),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis(this);
  const bool bStarted = Provider->AddToProject(
      ListingId, [WeakThis, RequestId, Socket, ListingId](const FMcpFabAddResult &Result) {
        AsyncTask(ENamedThreads::GameThread, [WeakThis, RequestId, Socket, ListingId, Result]() {
          UMcpAutomationBridgeSubsystem *Self = WeakThis.Get();
          if (Self == nullptr) {
            return;
          }
          TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
          Data->SetStringField(TEXT("listingId"), ListingId);
          Data->SetBoolField(TEXT("accepted"), Result.bAccepted);
          Data->SetNumberField(TEXT("assetCount"), Result.AssetCount);
          Data->SetStringField(TEXT("importedRoot"), Result.RootPath);
          Data->SetBoolField(TEXT("engineExactMatch"), Result.bEngineExactMatch);
          Data->SetStringField(TEXT("versionName"), Result.VersionName);
          TArray<TSharedPtr<FJsonValue>> Samples;
          for (const FString &Path : Result.SamplePaths) {
            Samples.Add(MakeShared<FJsonValueString>(Path));
          }
          Data->SetArrayField(TEXT("sampleAssetPaths"), Samples);
          Data->SetStringField(
              TEXT("note"),
              TEXT("Fab chooses the destination folder; use asset.migrate_assets to relocate the tree."));

          const bool bOk = Result.bAccepted && Result.AssetCount > 0;
          Self->SendAutomationResponse(
              Socket, RequestId, bOk,
              bOk ? FString::Printf(TEXT("Imported %d asset(s) into %s."), Result.AssetCount,
                                    *Result.RootPath)
                  : Result.Error,
              Data, bOk ? TEXT("") : Result.ErrorCode);
        });
      });

  if (!bStarted) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("The Fab adapter refused the request."), nullptr,
                           TEXT("NOT_SUPPORTED"));
  }
  return true;
}
#else
bool UMcpAutomationBridgeSubsystem::HandleAddFabAssetToProject(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  SendAutomationResponse(Socket, RequestId, false, TEXT("Editor required."), nullptr,
                         TEXT("EDITOR_ONLY"));
  return true;
}
#endif
