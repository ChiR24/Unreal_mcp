// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpFabProvider.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"


/**
 * Describes one Fab listing: what it is, and what it looks like.
 *
 * Search returns ids and titles, which is enough to shortlist and not enough to
 * choose. The preview arrives as imageBase64, which the protocol layer promotes
 * into a real MCP image content block, so the caller actually sees the asset and
 * no image URL crosses the boundary.
 */
bool UMcpAutomationBridgeSubsystem::HandleGetFabListingDetails(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  IMcpFabProvider *Provider = GetMcpFabProvider();
  if (Provider == nullptr || !Provider->IsFabAvailable()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("Fab support is not loaded in this editor, so listings cannot be described."),
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
  const bool bStarted = Provider->GetListingDetails(
      ListingId, [WeakThis, RequestId, Socket, ListingId](bool bSuccess, const FString &Json) {
        AsyncTask(ENamedThreads::GameThread, [WeakThis, RequestId, Socket, ListingId, bSuccess, Json]() {
          UMcpAutomationBridgeSubsystem *Self = WeakThis.Get();
          if (Self == nullptr) {
            return;
          }
          TSharedPtr<FJsonObject> Parsed;
          const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
          // Parsed regardless of bSuccess: a refusal carries the reason, and
          // gating on bSuccess discarded it and reported DETAILS_FAILED for a
          // page that had said exactly what was wrong.
          const bool bJson = FJsonSerializer::Deserialize(Reader, Parsed) && Parsed.IsValid();
          FString PageError;
          FString PageMessage;
          if (bJson) {
            Parsed->TryGetStringField(TEXT("error"), PageError);
            Parsed->TryGetStringField(TEXT("message"), PageMessage);
          }
          const bool bParsed = bSuccess && bJson && PageError.IsEmpty();
          if (!bParsed) {
            const FString Code = PageError.IsEmpty() ? TEXT("DETAILS_FAILED") : PageError;
            // The adapter words its own failures (page not ready, page busy, timed out).
            Self->SendAutomationResponse(
                Socket, RequestId, false,
                PageMessage.IsEmpty() ? FString(TEXT("Could not describe that listing.")) : PageMessage,
                nullptr, Code);
            return;
          }

          TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
          // Only what the page reported: a field Fab did not carry is left out, never filled with a
          // default, so absent means unknown rather than zero, false or empty.
          for (const TCHAR *Field : {TEXT("listingId"), TEXT("title"), TEXT("listingType"),
                                     TEXT("description"), TEXT("seller"), TEXT("mimeType"),
                                     TEXT("imageBase64"), TEXT("imageOmitted"),
                                     TEXT("category"), TEXT("categoryPath"), TEXT("currency"),
                                     TEXT("publishedAt"), TEXT("priceShape"),
                                     TEXT("distributionMethod"), TEXT("runningEngine"),
                                     TEXT("versionName"), TEXT("pickedEngineVersion"),
                                     TEXT("engineMatch"), TEXT("addFormat"), TEXT("downloadFile"),
                                     TEXT("quality"), TEXT("addBlockedCode"),
                                     TEXT("addBlockedReason")}) {
            FString Value;
            if (Parsed->TryGetStringField(Field, Value) && !Value.IsEmpty()) {
              Data->SetStringField(Field, Value);
            }
          }
          for (const TCHAR *Field : {TEXT("averageRating"), TEXT("ratingCount"), TEXT("price"),
                                     TEXT("downloadBytes")}) {
            double Number = 0.0;
            if (Parsed->TryGetNumberField(Field, Number)) {
              Data->SetNumberField(Field, Number);
            }
          }
          for (const TCHAR *List : {TEXT("tags"), TEXT("assetFormats"), TEXT("licenseNames"),
                                    TEXT("engineVersions"), TEXT("qualities"), TEXT("addWarnings"),
                                    TEXT("formats")}) {
            const TArray<TSharedPtr<FJsonValue>> *Rows = nullptr;
            if (Parsed->TryGetArrayField(List, Rows) && Rows != nullptr) {
              Data->SetArrayField(List, *Rows);
            }
          }
          // Reported even when false: "this listing has no Unreal build" is the
          // answer a caller needs before choosing between search hits. The rest are
          // present only when the page could tell.
          for (const TCHAR *Flag : {TEXT("hasUnrealBuild"), TEXT("canAddToProject"), TEXT("isFree"),
                                    TEXT("isCc0"), TEXT("attributionRequired"), TEXT("supportsRunningEngine"),
                                    TEXT("downloadSizeKnown")}) {
            bool bValue = false;
            if (Parsed->TryGetBoolField(Flag, bValue)) {
              Data->SetBoolField(Flag, bValue);
            }
          }
          // Reported only when a field could not be read, so a schema change
          // names itself instead of silently returning nothing.
          for (const TCHAR *Diag : {TEXT("descriptionKeys"), TEXT("thumbnailShape")}) {
            const TArray<TSharedPtr<FJsonValue>> *Shape = nullptr;
            if (Parsed->TryGetArrayField(Diag, Shape) && Shape != nullptr) {
              Data->SetArrayField(Diag, *Shape);
            }
          }
          Data->SetBoolField(TEXT("hasImage"), Data->HasField(TEXT("imageBase64")));

          Self->SendAutomationResponse(Socket, RequestId, true,
                                       TEXT("Listing described."), Data);
        });
      });

  if (!bStarted) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("The Fab adapter refused the request."), nullptr,
                           TEXT("NOT_SUPPORTED"));
  }
  return true;
}
