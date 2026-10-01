// Copyright (c) 2024 MCP Automation Bridge Contributors

// Catalog search, run inside the signed-in Fab page.
//
// This is what closes the loop: without it a caller has to obtain a listing uid
// some other way -- in practice by opening fab.com in a browser and reading it
// off a link -- which an agent cannot do. Search returns uids that
// add_fab_asset_to_project consumes directly.
//
// Search covers the public catalog, deliberately unfiltered by channel.
//
// It was previously pinned to channels=unreal-engine so that everything search
// returned would also add cleanly. That bought consistency at too high a price:
// the pin hid the Quixel/Megascans library -- the largest body of Unreal-ready
// content on Fab -- so the tool could not find assets a user could plainly see
// on fab.com, and the only way to reach one was to read its uid out of a
// browser by hand. That is exactly the manual step this capability exists to
// remove.
//
// Addability is therefore checked where it can actually be known: add_fab_asset
// _to_project resolves the listing's real asset formats and reports
// NO_IMPORTABLE_FORMAT only when Fab can import none of the formats a listing
// ships. A hit is a candidate, not a promise, and the response says so.
//
// A hit does carry what the search itself returned -- publisher, category, rating, price, license and
// the format codes -- so a Megascans listing can be told from a random upload without a second call.
// Those are read by the function the listing details use, from the row alone; nothing more is fetched.
// The catalog can be narrowed to one publisher and one content kind, which Fab's search takes as
// query-string values.

#include "McpFabProvider.h"
#include "McpFabBridgeDispatch.h"
#include "McpFabListingScript.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/Guid.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpFabSearch, Log, All);

namespace McpFabSearchOperation
{
namespace
{
/**
 * The text reaches a query-string value, never a path segment, and is passed
 * through encodeURIComponent on the page. Quotes and backslashes are still
 * rejected here so the value cannot terminate the JS string literal it is
 * embedded in, and control characters are refused outright. Used for the free
 * text and for the publisher's name.
 */
bool IsSafeQuery(const FString& Value)
{
	if (Value.Len() > 128)
	{
		return false;
	}
	for (const TCHAR C : Value)
	{
		if (C < 32 || C == TEXT('"') || C == TEXT('\\') || C == TEXT('\''))
		{
			return false;
		}
	}
	return true;
}

/** A content kind is one token such as 3d-model or material, so nothing else can ride along. */
bool IsSafeListingType(const FString& Value)
{
	if (Value.Len() > 40)
	{
		return false;
	}
	for (const TCHAR C : Value)
	{
		const bool bAllowed = (C >= TEXT('a') && C <= TEXT('z')) || (C >= TEXT('A') && C <= TEXT('Z')) ||
			(C >= TEXT('0') && C <= TEXT('9')) || C == TEXT('-') || C == TEXT('_');
		if (!bAllowed)
		{
			return false;
		}
	}
	return true;
}

/** Composed natively; only the free text, the two filters and the paging vary. */
FString BuildSearchScript(const FString& RequestId, const FMcpFabSearchRequest& Request, int32 Limit)
{
	return FString::Printf(TEXT(R"JS(
(function () {
  var id = "%s", q = "%s", seller = "%s", types = "%s", limit = %d, freeOnly = %s;
%s
  function send(o) { try { window.ue.mcpfab.onresult(id, JSON.stringify(o)); } catch (e) {} }
  function fail(e) { try { window.ue.mcpfab.onerror(id, String(e).slice(0, 400)); } catch (_) {} }
  try {
    // No channel filter: the whole public catalog is searched, and whether a
    // given listing has an Unreal build is settled at add time. The publisher and the
    // content kind narrow it, and are the only other values taken from the caller.
    var url = "https://www.fab.com/i/listings/search?count=" + encodeURIComponent(String(limit))
            + (freeOnly ? "&is_free=1" : "")
            + (q ? "&q=" + encodeURIComponent(q) : "")
            + (types ? "&listing_types=" + encodeURIComponent(types) : "")
            + (seller ? "&seller=" + encodeURIComponent(seller) : "");
    fetch(url, { credentials: "include" })
      .then(function (r) { if (!r.ok) { throw new Error("HTTP " + r.status); } return r.json(); })
      .then(function (j) {
        var rows = j.results || [];
        send({
          listings: rows.slice(0, limit).map(function (x) {
            var row = {
              uid: String(x.uid || ""),
              title: String(x.title || ""),
              listingType: String(x.listingType || ""),
              tags: (x.tags || []).slice(0, 8).map(function (t) {
                return String(t && t.name ? t.name : t);
              })
            };
            // Publisher, category, rating, price and the rest, read from the row alone.
            listingFacts(x, row);
            var codes = (x.assetFormats || []).map(function (f) {
              return String(f.assetFormatType ? f.assetFormatType.code : (f.code || f.type || "?"));
            });
            if (codes.length) { row.formats = codes.slice(0, 12); }
            return row;
          })
        });
      })
      .catch(fail);
  } catch (e) { fail(e); }
})();
)JS"), *RequestId, *Request.Query, *Request.Seller, *Request.ListingType, Limit,
		Request.bFreeOnly ? TEXT("true") : TEXT("false"), McpFabListing::Script());
}

/** One row of the page's reply into a listing. Facts the row did not carry stay unset. */
FMcpFabListing ReadListing(const TSharedPtr<FJsonObject>& Row)
{
	FMcpFabListing Listing;
	Row->TryGetStringField(TEXT("uid"), Listing.Uid);
	Row->TryGetStringField(TEXT("title"), Listing.Title);
	Row->TryGetStringField(TEXT("listingType"), Listing.ListingType);
	Row->TryGetBoolField(TEXT("isFree"), Listing.bIsFree);
	Row->TryGetStringField(TEXT("priceShape"), Listing.PriceShape);
	Listing.bPriceResolved = Listing.PriceShape.IsEmpty();
	Row->TryGetStringField(TEXT("seller"), Listing.Seller);
	Row->TryGetStringField(TEXT("category"), Listing.Category);
	Row->TryGetStringField(TEXT("currency"), Listing.Currency);
	Row->TryGetStringField(TEXT("publishedAt"), Listing.PublishedAt);
	double Number = 0.0;
	if (Row->TryGetNumberField(TEXT("averageRating"), Number)) { Listing.AverageRating = Number; }
	if (Row->TryGetNumberField(TEXT("ratingCount"), Number)) { Listing.RatingCount = static_cast<int32>(Number); }
	if (Row->TryGetNumberField(TEXT("price"), Number)) { Listing.Price = Number; }
	bool bCc0 = false;
	if (Row->TryGetBoolField(TEXT("isCc0"), bCc0)) { Listing.bIsCc0 = bCc0; }
	const auto ReadList = [&Row](const TCHAR* Field, TArray<FString>& Target)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (Row->TryGetArrayField(Field, Values) && Values != nullptr)
		{
			for (const TSharedPtr<FJsonValue>& Value : *Values)
			{
				FString Text;
				if (Value.IsValid() && Value->TryGetString(Text) && !Text.IsEmpty())
				{
					Target.Add(Text);
				}
			}
		}
	};
	ReadList(TEXT("formats"), Listing.Formats);
	ReadList(TEXT("tags"), Listing.Tags);
	return Listing;
}
} // namespace

bool Start(const FMcpFabSearchRequest& Request, TFunction<void(const FMcpFabSearchResult&)> OnComplete)
{
	FMcpFabSearchResult Rejected;
	if (!IsSafeQuery(Request.Query))
	{
		Rejected.ErrorCode = TEXT("INVALID_QUERY");
		Rejected.Error = TEXT("A query must be at most 128 characters and free of quotes, backslashes and control characters.");
	}
	else if (!IsSafeQuery(Request.Seller))
	{
		Rejected.ErrorCode = TEXT("INVALID_SELLER");
		Rejected.Error = TEXT("A seller must be at most 128 characters and free of quotes, backslashes and control characters.");
	}
	else if (!IsSafeListingType(Request.ListingType))
	{
		Rejected.ErrorCode = TEXT("INVALID_LISTING_TYPE");
		Rejected.Error = TEXT("A listingType is one content kind such as 3d-model or material: [A-Za-z0-9_-], at most 40 characters.");
	}
	if (!Rejected.ErrorCode.IsEmpty())
	{
		OnComplete(Rejected);
		return true;
	}

	const int32 Bounded = FMath::Clamp(Request.Limit, 1, 50);

	FString Error;
	FString ErrorCode;
	const bool bDispatched = McpFabBridgeDispatch::Dispatch(
		[&Request, Bounded](const FString& RequestId)
		{
			return BuildSearchScript(RequestId, Request, Bounded);
		},
		[OnComplete](bool bSuccess, const FString& Payload)
	{
		FMcpFabSearchResult Result;
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Payload);
		const bool bJson = FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid();

		// An error reply is itself well-formed JSON, so parsing alone proves
		// nothing: without this check a page that answered PAGE_NAVIGATING read
		// as a clean parse with no listings and was reported as a successful
		// search that found nothing, which is the one answer a caller cannot
		// tell from a real empty result.
		FString PageError;
		FString PageMessage;
		if (bJson)
		{
			Root->TryGetStringField(TEXT("error"), PageError);
			Root->TryGetStringField(TEXT("message"), PageMessage);
		}
		if (!bSuccess || !bJson || !PageError.IsEmpty())
		{
			Result.ErrorCode = PageError.IsEmpty() ? TEXT("SEARCH_FAILED") : PageError;
			// The dispatcher words its own failures (page not ready, page busy, timed out).
			Result.Error = !PageMessage.IsEmpty() ? PageMessage
				: PageError == TEXT("PAGE_NAVIGATING")
					? FString(TEXT("The Fab tab had not finished loading fab.com. It has been sent there; retry in a few seconds."))
					: FString(TEXT("The Fab page did not return a usable result."));
			OnComplete(Result);
			return;
		}
		const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
		if (Root->TryGetArrayField(TEXT("listings"), Rows) && Rows != nullptr)
		{
			for (const TSharedPtr<FJsonValue>& Row : *Rows)
			{
				const TSharedPtr<FJsonObject>* Object = nullptr;
				if (!Row.IsValid() || !Row->TryGetObject(Object) || Object == nullptr)
				{
					continue;
				}
				FMcpFabListing Listing = ReadListing(*Object);
				if (!Listing.Uid.IsEmpty())
				{
					Result.Listings.Add(MoveTemp(Listing));
				}
			}
		}
		Result.bSuccess = true;
		OnComplete(Result);
	},
		Error, ErrorCode);

	if (!bDispatched)
	{
		FMcpFabSearchResult Failed;
		Failed.ErrorCode = ErrorCode;
		Failed.Error = Error;
		OnComplete(Failed);
	}
	return true;
}
} // namespace McpFabSearchOperation
