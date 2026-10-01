// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Domains/AssetWorkflow/Fab/McpAutomationBridge_FabImportJson.h"
#include "Domains/AssetWorkflow/Fab/McpAutomationBridge_FabPostImport.h"
#include "McpFabProvider.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"


/**
 * Adds one Fab listing to this project.
 *
 * The whole acquisition happens on Fab's side of the boundary: its signed-in
 * page resolves the listing, mints the download URL and hands it to Fab's own
 * importer. Nothing in this process ever sees the URL, the EOS token or a
 * cookie, so no receipt or log can carry them.
 *
 * This answers as soon as Fab accepts the download, not when the import ends.
 * The download and Fab's importer take minutes and hold the game thread, far
 * past the client's request timeout, and while they do every call answers
 * EDITOR_BLOCKED, so no reply could be sent then. The import runs in the
 * background under an operation id; get_fab_import_status reports it, and it
 * is the asset registry there, not Fab's word, that decides whether content
 * arrived.
 *
 * Fab chooses the destination folder -- FPackImportWorkflow imports to the
 * pack's own name under /Game and honours no caller path -- so the status read
 * reports where the content landed instead of pretending to place it.
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

  FMcpFabAddOptions Options;
  // Once the import settles the packages it left dirty are saved; the status read reports how that went.
  Options.PostImport = &McpFabPostImport::Run;
  bool bCombineMeshes = false;
  if (Payload->TryGetBoolField(TEXT("combineMeshes"), bCombineMeshes)) {
    Options.CombineMeshes = bCombineMeshes;
  }
  // The tier a Megascans listing is fetched at; anything but the four tiers is refused here, before the
  // adapter is asked, so no other text ever reaches the page.
  FString Quality;
  if (Payload->TryGetStringField(TEXT("quality"), Quality) && !Quality.IsEmpty()) {
    Quality = Quality.ToLower();
    if (Quality != TEXT("raw") && Quality != TEXT("high") && Quality != TEXT("mid") &&
        Quality != TEXT("low")) {
      SendAutomationResponse(Socket, RequestId, false,
                             TEXT("'quality' is one of raw, high, mid or low."), nullptr,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Options.Quality = Quality;
  }

  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis(this);
  const bool bStarted = Provider->AddToProject(
      ListingId, Options, [WeakThis, RequestId, Socket, ListingId](const FMcpFabAddResult &Result) {
        AsyncTask(ENamedThreads::GameThread, [WeakThis, RequestId, Socket, ListingId, Result]() {
          UMcpAutomationBridgeSubsystem *Self = WeakThis.Get();
          if (Self == nullptr) {
            return;
          }
          TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
          Data->SetStringField(TEXT("listingId"), ListingId);
          Data->SetBoolField(TEXT("accepted"), Result.bAccepted);
          if (!Result.OperationId.IsEmpty()) {
            Data->SetStringField(TEXT("operationId"), Result.OperationId);
          }
          if (!Result.bAccepted) {
            // A full queue names the import at its head and the call that reads it.
            if (Result.ErrorCode == TEXT("QUEUE_FULL") && !Result.OperationId.IsEmpty()) {
              Data->SetObjectField(TEXT("nextCall"), McpFabImportJson::MakeStatusNextCall(Result.OperationId));
            }
            Self->SendAutomationResponse(Socket, RequestId, false, Result.Error, Data, Result.ErrorCode);
            return;
          }
          McpFabImportJson::SetAddFacts(Data, Result);
          if (!Result.Phase.IsEmpty()) {
            Data->SetStringField(TEXT("phase"), Result.Phase);
          }
          if (Result.QueuePosition > 0) {
            Data->SetNumberField(TEXT("queuePosition"), Result.QueuePosition);
          }
          Data->SetBoolField(TEXT("alreadyRunning"), Result.bAlreadyRunning);
          const bool bQueued = Result.Phase == TEXT("queued");
          Data->SetObjectField(
              TEXT("task"), McpFabImportJson::MakeTask(Result.OperationId, bQueued ? TEXT("queued") : TEXT("running")));
          Data->SetStringField(
              TEXT("note"),
              TEXT("Not imported yet. Poll asset.query_marketplace with lookup=fab_import_status and this operationId until phase is done or failed; do not call this add again. "
                   "One import runs at a time: an add made while another runs is queued and starts by itself, in order, and that read lists the queue. "
                   "While Fab imports, the editor is held and every call, the status read included, answers EDITOR_BLOCKED: keep polling. "
                   "Fab chooses the destination folder; the status read reports importedRoot, and asset.move relocates a folder."));
          Self->SendAutomationResponse(
              Socket, RequestId, true,
              bQueued
                  ? FString::Printf(TEXT("Fab is busy with another import, so %s is queued (position %d) as operation %s; it starts by itself when the running import ends."),
                                    *ListingId, Result.QueuePosition, *Result.OperationId)
                  : FString::Printf(TEXT("Fab accepted the download of %s; operation %s continues in the background."),
                                    *ListingId, *Result.OperationId),
              Data, FString());
        });
      });

  if (!bStarted) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("The Fab adapter refused the request."), nullptr,
                           TEXT("NOT_SUPPORTED"));
  }
  return true;
}
