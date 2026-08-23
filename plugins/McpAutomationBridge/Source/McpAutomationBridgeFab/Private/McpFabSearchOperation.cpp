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
// ships -- unreal-engine, gltf, glb and fbx are all importable. A hit is a
// candidate,
// not a promise, and the response says so.

#include "McpFabProvider.h"
#include "McpFabBridgeDispatch.h"

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
 * The query reaches a query-string value, never a path segment, and is passed
 * through encodeURIComponent on the page. Quotes and backslashes are still
 * rejected here so the value cannot terminate the JS string literal it is
 * embedded in, and control characters are refused outright.
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

/** Composed natively; only the free text and paging vary. */
FString BuildSearchScript(
	const FString& RequestId, const FString& Query, bool bFreeOnly, int32 Limit)
{
	return FString::Printf(TEXT(R"JS(
(function () {
  var id = "%s", q = "%s", limit = %d, freeOnly = %s;
  function send(o) { try { window.ue.mcpfab.onresult(id, JSON.stringify(o)); } catch (e) {} }
  function fail(e) { try { window.ue.mcpfab.onerror(id, String(e).slice(0, 400)); } catch (_) {} }
  try {
    // No channel filter: the whole public catalog is searched, and whether a
    // given listing has an Unreal build is settled at add time.
    var url = "https://www.fab.com/i/listings/search?count=" + encodeURIComponent(String(limit))
            + (freeOnly ? "&is_free=1" : "")
            + (q ? "&q=" + encodeURIComponent(q) : "");
    fetch(url, { credentials: "include" })
      .then(function (r) { if (!r.ok) { throw new Error("HTTP " + r.status); } return r.json(); })
      .then(function (j) {
        var rows = j.results || [];
        function priceOf(x) {
          // x.isFree disagrees with reality: a listing titled "... (Free)" that
          // only appears under is_free=1 still reports isFree false, so the flag
          // means something narrower than "costs nothing". Price is the honest
          // signal; the shape is reported when it cannot be resolved so the next
          // run says what the field actually is instead of being guessed at.
          var p = x.startingPrice;
          if (p === null || p === undefined) return { cents: 0, resolved: true };
          if (typeof p === "number") return { cents: p, resolved: true };
          if (typeof p === "object") {
            var keys = ["price", "amount", "basePrice", "finalPrice", "discountPrice"];
            for (var i = 0; i < keys.length; i++) {
              if (typeof p[keys[i]] === "number") return { cents: p[keys[i]], resolved: true };
            }
            return { cents: -1, resolved: false, shape: Object.keys(p).slice(0, 12) };
          }
          return { cents: -1, resolved: false, shape: typeof p };
        }
        send({
          listings: rows.slice(0, limit).map(function (x) {
            var pr = priceOf(x);
            return {
              uid: String(x.uid || ""),
              title: String(x.title || ""),
              listingType: String(x.listingType || ""),
              isFree: pr.resolved ? pr.cents === 0 : !!x.isFree,
              priceResolved: pr.resolved,
              priceShape: pr.resolved ? "" : JSON.stringify(pr.shape),
              rawIsFree: !!x.isFree,
              tags: (x.tags || []).slice(0, 8).map(function (t) {
                return String(t && t.name ? t.name : t);
              })
            };
          })
        });
      })
      .catch(fail);
  } catch (e) { fail(e); }
})();
)JS"), *RequestId, *Query, Limit, bFreeOnly ? TEXT("true") : TEXT("false"));
}
} // namespace

bool Start(const FString& Query, bool bFreeOnly, int32 Limit,
	TFunction<void(const FMcpFabSearchResult&)> OnComplete)
{
	if (!IsSafeQuery(Query))
	{
		FMcpFabSearchResult Rejected;
		Rejected.ErrorCode = TEXT("INVALID_QUERY");
		Rejected.Error = TEXT("A query must be at most 128 characters and free of quotes, backslashes and control characters.");
		OnComplete(Rejected);
		return true;
	}

	const int32 Bounded = FMath::Clamp(Limit, 1, 50);

	FString Error;
	FString ErrorCode;
	const bool bDispatched = McpFabBridgeDispatch::Dispatch(
		[&Query, bFreeOnly, Bounded](const FString& RequestId)
		{
			return BuildSearchScript(RequestId, Query, bFreeOnly, Bounded);
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
		if (bJson) { Root->TryGetStringField(TEXT("error"), PageError); }
		if (!bSuccess || !bJson || !PageError.IsEmpty())
		{
			Result.ErrorCode = PageError.IsEmpty() ? TEXT("SEARCH_FAILED") : PageError;
			Result.Error = PageError == TEXT("PAGE_NAVIGATING")
				? TEXT("The Fab tab had not finished loading fab.com. It has been sent there; retry in a few seconds.")
				: TEXT("The Fab page did not return a usable result.");
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
				FMcpFabListing Listing;
				(*Object)->TryGetStringField(TEXT("uid"), Listing.Uid);
				(*Object)->TryGetStringField(TEXT("title"), Listing.Title);
				(*Object)->TryGetStringField(TEXT("listingType"), Listing.ListingType);
				(*Object)->TryGetBoolField(TEXT("isFree"), Listing.bIsFree);
				(*Object)->TryGetBoolField(TEXT("priceResolved"), Listing.bPriceResolved);
				(*Object)->TryGetStringField(TEXT("priceShape"), Listing.PriceShape);
				(*Object)->TryGetBoolField(TEXT("rawIsFree"), Listing.bRawIsFree);
				const TArray<TSharedPtr<FJsonValue>>* TagRows = nullptr;
				if ((*Object)->TryGetArrayField(TEXT("tags"), TagRows) && TagRows != nullptr)
				{
					for (const TSharedPtr<FJsonValue>& Tag : *TagRows)
					{
						FString Text;
						if (Tag.IsValid() && Tag->TryGetString(Text) && !Text.IsEmpty())
						{
							Listing.Tags.Add(Text);
						}
					}
				}
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
