// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpFabProvider.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"


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
 * anything transient or credential-derived. A row says who published it, which
 * category it is in, how it is rated and what it costs, as far as the search
 * itself returned: a fact the row did not carry is left out, never filled in.
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

  FMcpFabSearchRequest Request;
  Payload->TryGetStringField(TEXT("query"), Request.Query);
  Payload->TryGetStringField(TEXT("seller"), Request.Seller);
  Payload->TryGetStringField(TEXT("listingType"), Request.ListingType);
  Payload->TryGetBoolField(TEXT("freeOnly"), Request.bFreeOnly);
  double Limit = 12;
  Payload->TryGetNumberField(TEXT("limit"), Limit);
  Request.Limit = FMath::Clamp(static_cast<int32>(Limit), 1, 50);

  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis(this);
  const bool bStarted = Provider->SearchListings(
      Request,
      [WeakThis, RequestId, Socket, Request](const FMcpFabSearchResult &Result) {
        AsyncTask(ENamedThreads::GameThread, [WeakThis, RequestId, Socket, Request, Result]() {
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
            // Derived from price. The listing's own flag disagrees with it, so it is not passed on.
            Row->SetBoolField(TEXT("isFree"), Listing.bIsFree);
            if (!Listing.bPriceResolved) {
              Row->SetStringField(TEXT("unresolvedPriceShape"), Listing.PriceShape);
            }
            if (!Listing.Seller.IsEmpty()) {
              Row->SetStringField(TEXT("seller"), Listing.Seller);
            }
            if (!Listing.Category.IsEmpty()) {
              Row->SetStringField(TEXT("category"), Listing.Category);
            }
            if (Listing.AverageRating.IsSet()) {
              Row->SetNumberField(TEXT("averageRating"), Listing.AverageRating.GetValue());
            }
            if (Listing.RatingCount.IsSet()) {
              Row->SetNumberField(TEXT("ratingCount"), Listing.RatingCount.GetValue());
            }
            if (Listing.Price.IsSet()) {
              Row->SetNumberField(TEXT("price"), Listing.Price.GetValue());
            }
            if (!Listing.Currency.IsEmpty()) {
              Row->SetStringField(TEXT("currency"), Listing.Currency);
            }
            if (Listing.bIsCc0.IsSet()) {
              Row->SetBoolField(TEXT("isCc0"), Listing.bIsCc0.GetValue());
            }
            if (!Listing.PublishedAt.IsEmpty()) {
              Row->SetStringField(TEXT("publishedAt"), Listing.PublishedAt);
            }
            // tags is always present, as before; formats only when the row named some.
            const auto AddStrings = [&Row](const TCHAR *Field, const TArray<FString> &Texts, bool bAlways) {
              TArray<TSharedPtr<FJsonValue>> Values;
              for (const FString &Text : Texts) {
                Values.Add(MakeShared<FJsonValueString>(Text));
              }
              if (bAlways || Values.Num() > 0) {
                Row->SetArrayField(Field, Values);
              }
            };
            AddStrings(TEXT("formats"), Listing.Formats, false);
            AddStrings(TEXT("tags"), Listing.Tags, true);
            Rows.Add(MakeShared<FJsonValueObject>(Row));
          }
          Data->SetArrayField(TEXT("listings"), Rows);
          Data->SetNumberField(TEXT("listingCount"), Rows.Num());
          Data->SetStringField(TEXT("query"), Request.Query);
          if (!Request.Seller.IsEmpty()) {
            Data->SetStringField(TEXT("seller"), Request.Seller);
          }
          if (!Request.ListingType.IsEmpty()) {
            Data->SetStringField(TEXT("listingType"), Request.ListingType);
          }
          Data->SetStringField(
              TEXT("note"),
              TEXT("Searches the whole public Fab catalog, so a hit is a candidate rather than a promise: pass listingId to add_fab_asset_to_project, which resolves the real asset formats and imports unreal-engine, gltf, glb, fbx, obj or usdz alike. seller and listingType narrow the search; formats lists what a hit ships. Call get_fab_listing_details for canAddToProject, the engine build and the download size up front. listingType is the content kind (3d-model, material), not that guarantee."));

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
