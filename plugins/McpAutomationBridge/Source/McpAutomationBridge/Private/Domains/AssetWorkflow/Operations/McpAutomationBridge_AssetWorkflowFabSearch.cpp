// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpFabProvider.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"

#if WITH_EDITOR

/**
 * Searches the Fab catalog through the signed-in page.
 *
 * This is the half that makes the rest usable without a human: every uid
 * returned here can be handed straight to add_fab_asset_to_project. Without it
 * a caller has to read a listing id off fab.com in a browser, which an agent
 * cannot do.
 *
 * Results carry ids and labels only. No thumbnail URL, no download URL and no
 * account-scoped field is copied out of the page, so the response stays free of
 * anything transient or credential-derived.
 */
bool UMcpAutomationBridgeSubsystem::HandleSearchFabListings(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  IMcpFabProvider *Provider = GetMcpFabProvider();
  if (Provider == nullptr || !Provider->IsFabAvailable()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("Fab support is not loaded in this editor, so the catalog cannot be searched."),
        nullptr, TEXT("NOT_SUPPORTED"));
    return true;
  }

  FString Query;
  Payload->TryGetStringField(TEXT("query"), Query);

  bool bFreeOnly = false;
  Payload->TryGetBoolField(TEXT("freeOnly"), bFreeOnly);

  double Limit = 12;
  Payload->TryGetNumberField(TEXT("limit"), Limit);
  const int32 Bounded = FMath::Clamp(static_cast<int32>(Limit), 1, 50);

  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis(this);
  const bool bStarted = Provider->SearchListings(
      Query, bFreeOnly, Bounded,
      [WeakThis, RequestId, Socket, Query](const FMcpFabSearchResult &Result) {
        AsyncTask(ENamedThreads::GameThread, [WeakThis, RequestId, Socket, Query, Result]() {
          UMcpAutomationBridgeSubsystem *Self = WeakThis.Get();
          if (Self == nullptr) {
            return;
          }
          TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
          TArray<TSharedPtr<FJsonValue>> Rows;
          for (const FMcpFabListing &Listing : Result.Listings) {
            TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("listingId"), Listing.Uid);
            Row->SetStringField(TEXT("title"), Listing.Title);
            Row->SetStringField(TEXT("listingType"), Listing.ListingType);
            Row->SetBoolField(TEXT("isFree"), Listing.bIsFree);
            // Surfaced deliberately: the listing's own isFree flag disagrees with
            // its price, so a caller that cares can see both rather than trusting
            // a single field that has already been observed to be wrong.
            Row->SetBoolField(TEXT("rawIsFree"), Listing.bRawIsFree);
            if (!Listing.bPriceResolved) {
              Row->SetStringField(TEXT("unresolvedPriceShape"), Listing.PriceShape);
            }
            TArray<TSharedPtr<FJsonValue>> Tags;
            for (const FString &Tag : Listing.Tags) {
              Tags.Add(MakeShared<FJsonValueString>(Tag));
            }
            Row->SetArrayField(TEXT("tags"), Tags);
            Rows.Add(MakeShared<FJsonValueObject>(Row));
          }
          Data->SetArrayField(TEXT("listings"), Rows);
          Data->SetNumberField(TEXT("listingCount"), Rows.Num());
          Data->SetStringField(TEXT("query"), Query);
          Data->SetStringField(
              TEXT("note"),
              TEXT("Searches the whole public Fab catalog, so a hit is a candidate rather than a promise: pass listingId to add_fab_asset_to_project, which resolves the real asset formats and imports unreal-engine, gltf, glb or fbx alike. Call get_fab_listing_details for canAddToProject up front. listingType is the content kind (3d-model, material), not that guarantee."));

          Self->SendAutomationResponse(
              Socket, RequestId, Result.bSuccess,
              Result.bSuccess
                  ? FString::Printf(TEXT("Found %d Fab listing(s)."), Rows.Num())
                  : Result.Error,
              Data, Result.bSuccess ? TEXT("") : Result.ErrorCode);
        });
      });

  if (!bStarted) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("The Fab adapter refused the search."), nullptr,
                           TEXT("NOT_SUPPORTED"));
  }
  return true;
}
#else
bool UMcpAutomationBridgeSubsystem::HandleSearchFabListings(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  SendAutomationResponse(Socket, RequestId, false, TEXT("Editor required."), nullptr,
                         TEXT("EDITOR_ONLY"));
  return true;
}
#endif
