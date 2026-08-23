// Copyright (c) 2024 MCP Automation Bridge Contributors

// Listing detail: the description and preview an agent needs to choose.
//
// Search returns ids and labels, which is enough to shortlist and not enough to
// decide. This fetches one listing and returns its description plus the preview
// image INLINED as base64 rather than as a URL: McpJsonRpcImageContent promotes
// an imageBase64 field into a real MCP image content block at any depth, so the
// caller sees the asset instead of a link, and no URL crosses the boundary.

#include "McpFabProvider.h"
#include "McpFabBridgeDispatch.h"

#include "Dom/JsonObject.h"
#include "Misc/Guid.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpFabDetails, Log, All);

namespace McpFabAddOperation
{
bool IsSafeListingIdShared(const FString& Value);
}

namespace McpFabDetailsOperation
{
namespace
{
/**
 * Endpoint composed natively; only the validated uid varies.
 *
 * The thumbnail is fetched and encoded inside the page, so its URL is used and
 * discarded there. The callback caps payloads at 256 KiB, so the image is
 * dropped rather than truncated when a preview is unusually large -- a
 * half-written base64 string would decode to nothing and look like corruption.
 */
FString BuildDetailsScript(const FString& RequestId, const FString& ListingId)
{
	return FString::Printf(TEXT(R"JS(
(function () {
  var id = "%s", listing = "%s";
  function send(o) { try { window.ue.mcpfab.onresult(id, JSON.stringify(o)); } catch (e) {} }
  function fail(e) { try { window.ue.mcpfab.onerror(id, String(e).slice(0, 400)); } catch (_) {} }
  var out = { listingId: listing };
  var base = "https://www.fab.com/i/listings/" + encodeURIComponent(listing);
  fetch(base, { credentials: "include" })
    .then(function (r) { out.status = r.status; return r.json(); })
    .then(function (j) {
      out.title = String(j.title || "");
      out.listingType = String(j.listingType || "");
      // Fab has used more than one key for prose; take the first that is a
      // non-empty string rather than assuming one name.
      var keys = ["description", "longDescription", "shortDescription", "summary"];
      for (var i = 0; i < keys.length; i++) {
        if (typeof j[keys[i]] === "string" && j[keys[i]].length) { out.description = j[keys[i]].slice(0, 4000); break; }
      }
      if (!out.description) { out.descriptionKeys = Object.keys(j).slice(0, 40); }
      out.tags = (j.tags || []).slice(0, 12).map(function (t) { return String(t && t.name ? t.name : t); });
      // Read from the response already in hand: this is the same listing body
      // add_fab_asset_to_project inspects, so describing a listing can say
      // whether adding it will work instead of leaving the caller to find out
      // by attempting the import and reading NO_IMPORTABLE_FORMAT.
      var codes = (j.assetFormats || []).map(function (f) {
        return String(f.assetFormatType ? f.assetFormatType.code : (f.code || f.type || "?"));
      }).slice(0, 12);
      out.assetFormats = codes;
      out.hasUnrealBuild = codes.some(function (c) { return c === "unreal-engine"; });
      var importable = codes.some(function (c) {
        return c === "unreal-engine" || c === "gltf" || c === "glb" || c === "fbx";
      });
      // Addability is broader than a packaged build -- Fab imports gltf/glb/fbx
      // through Interchange, verified by importing a gltf-only listing -- but it
      // is narrower than the format list for Quixel. Megascans listings
      // advertise gltf and fbx like anyone else, yet download-info answers 404
      // for every identifier and URL form (checked on two listings, both
      // sellers reading "Quixel Megascans"), because that content resolves
      // through its own route rather than the Fab listing files API. Calling
      // those addable would promise an import that reliably fails.
      var quixel = /quixel/i.test(String((j.user && j.user.sellerName) || ""));
      out.canAddToProject = importable && !(quixel && !out.hasUnrealBuild);
      if (!out.canAddToProject) {
        out.addBlockedReason = !importable
          ? "ships no format Fab can import: unreal-engine, gltf, glb or fbx"
          : "Quixel/Megascans listings must be claimed before Fab will serve a download, and the claim is rejected as CSRF-protected: the page exposes no CSRF token by meta tag, form input or cookie. Claim it once in the Fab tab and this listing becomes importable";
      }
      if (j.user && j.user.sellerName) { out.seller = String(j.user.sellerName); }
      // Pick the SMALLEST usable variant, not the first.
      //
      // Fab publishes each thumbnail at several widths and lists the full-size
      // media first. Taking that one meant the fetched preview was routinely
      // larger than the reply cap, so every listing came back with hasImage
      // false -- the cap was doing its job and the caller still got no picture.
      // A preview only has to be recognisable, so the narrowest variant at or
      // above 256px is chosen instead, and the widths actually offered are
      // reported when even that does not fit.
      var thumbs = j.thumbnails || [];
      var first = thumbs.length ? thumbs[0] : null;
      var variants = ((first && first.images) || []).filter(function (i) { return i && (i.url || i.mediaUrl); });
      variants.sort(function (a, b) { return (a.width || 1e9) - (b.width || 1e9); });
      var widths = variants.map(function (i) { return String(i.width || 0); });
      var chosen = null;
      for (var v = 0; v < variants.length; v++) {
        if ((variants[v].width || 0) >= 256) { chosen = variants[v]; break; }
      }
      if (!chosen && variants.length) { chosen = variants[variants.length - 1]; }
      var media = chosen ? (chosen.url || chosen.mediaUrl) : null;
      if (!media && first) { media = first.mediaUrl || first.url || first.uploadedImageUrl; }
      if (!media) { out.thumbnailShape = first ? Object.keys(first).slice(0, 12) : ["none"]; send(out); return; }
      return fetch(media, { credentials: "omit" })
        .then(function (r) { out.imageStatus = r.status; return r.blob(); })
        .then(function (b) {
          out.mimeType = b.type || "image/jpeg";
          if (b.size > 180000) {
            out.imageOmitted = "smallest offered preview is " + b.size + " bytes, over the 180000 byte inline cap";
            if (widths.length) { out.thumbnailShape = widths; }
            send(out); return;
          }
          return new Promise(function (res) {
            var fr = new FileReader();
            fr.onloadend = function () {
              out.imageBase64 = String(fr.result).split(",")[1] || "";
              res(send(out));
            };
            fr.readAsDataURL(b);
          });
        });
    })
    .catch(fail);
})();
)JS"), *RequestId, *ListingId);
}
} // namespace

bool Start(const FString& ListingId, TFunction<void(bool, const FString&)> OnComplete)
{
	if (!McpFabAddOperation::IsSafeListingIdShared(ListingId))
	{
		OnComplete(false, TEXT("{\"error\":\"INVALID_LISTING_ID\"}"));
		return true;
	}
	FString Error;
	FString ErrorCode;
	const bool bDispatched = McpFabBridgeDispatch::Dispatch(
		[&ListingId](const FString& RequestId)
		{
			return BuildDetailsScript(RequestId, ListingId);
		},
		[OnComplete](bool bSuccess, const FString& Payload)
		{
			OnComplete(bSuccess, Payload);
		},
		Error, ErrorCode);

	if (!bDispatched)
	{
		OnComplete(false, FString::Printf(TEXT("{\"error\":\"%s\"}"), *ErrorCode));
	}
	return true;
}
} // namespace McpFabDetailsOperation
