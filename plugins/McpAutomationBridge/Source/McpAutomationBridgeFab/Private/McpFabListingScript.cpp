// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpFabListingScript.h"

namespace McpFabListing
{
const TCHAR* Script()
{
	// No percent sign may appear in this text: it is spliced into an FString::Printf format.
	return TEXT(R"JS(
  // The price of a listing row or page, as a number in Fab's own units, or unresolved when the field has a
  // shape this does not know. The listing's own isFree flag disagrees with price (City Sample Buildings has
  // isFree false and a price of 0), so price is the honest signal and the flag is never reported.
  function priceOf(x) {
    var p = x.startingPrice;
    if (p === null || p === undefined) { return { value: 0, resolved: true }; }
    if (typeof p === "number") { return { value: p, resolved: true }; }
    if (typeof p === "object") {
      var keys = ["price", "amount", "basePrice", "finalPrice", "discountPrice"];
      for (var i = 0; i < keys.length; i++) {
        if (typeof p[keys[i]] === "number") {
          return { value: p[keys[i]], resolved: true, currency: String(p.currencyCode || p.currency || "") };
        }
      }
      return { value: -1, resolved: false, shape: Object.keys(p).slice(0, 12) };
    }
    return { value: -1, resolved: false, shape: typeof p };
  }
  function numberOf(v) { return typeof v === "number" && isFinite(v) ? v : null; }
  function labelOf(v) { return typeof v === "string" ? v : (v && typeof v.name === "string" ? v.name : ""); }
  // Writes the facts a listing row or page carries onto o. Anything the listing does not carry is left out,
  // so an absent field means Fab said nothing, not that the answer is zero or false.
  function listingFacts(x, o) {
    if (x.user && x.user.sellerName) { o.seller = String(x.user.sellerName); }
    var cat = x.category;
    if (cat && cat.name) { o.category = String(cat.name); }
    if (cat && cat.path) {
      o.categoryPath = Array.isArray(cat.path) ? cat.path.map(labelOf).filter(Boolean).join("/") : String(cat.path);
    }
    var ratings = x.ratings && typeof x.ratings === "object" ? x.ratings : {};
    var average = numberOf(x.averageRating);
    if (average === null) { average = numberOf(ratings.averageRating); }
    if (average !== null) { o.averageRating = average; }
    var count = numberOf(ratings.total);
    if (count === null) { count = numberOf(x.reviewCount); }
    if (count !== null) { o.ratingCount = count; }
    var licenses = Array.isArray(x.licenses) ? x.licenses : [];
    var names = licenses.map(function (l) { return labelOf(l); }).filter(Boolean);
    if (names.length) { o.licenseNames = names.slice(0, 6); }
    if (licenses.length) { o.isCc0 = licenses.some(function (l) { return !!(l && l.isCc0); }); }
    var price = priceOf(x);
    if (price.resolved) {
      o.price = price.value;
      o.isFree = price.value === 0;
      if (price.currency) { o.currency = price.currency; }
    } else {
      // No honest answer: the raw flag is all that is left, and the shape says what the price looked like.
      o.isFree = !!x.isFree;
      o.priceShape = JSON.stringify(price.shape);
    }
    var published = x.publishedAt || x.firstPublishedAt;
    if (published) { o.publishedAt = String(published); }
  }
)JS");
}
} // namespace McpFabListing
