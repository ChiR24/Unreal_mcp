// Copyright (c) 2024 MCP Automation Bridge Contributors

// Listing detail: what an agent needs to choose, and what it needs to know before adding.
//
// Search returns ids and labels, which is enough to shortlist and not enough to decide. This fetches one
// listing and returns its description, its publisher, category, rating, licenses and price, and the preview
// image INLINED as base64 rather than as a URL: McpJsonRpcImageContent promotes an imageBase64 field into a
// real MCP image content block at any depth, so the caller sees the asset instead of a link, and no URL
// crosses the boundary.
//
// It also answers what the add would do, with the add's own functions rather than a copy of them: which
// format it imports, which build of a pack or file of a source format, how large, whether a build of the
// pack exists for the running engine, and when it would refuse (a complete project, a MetaHuman listing,
// no importable format). Those come from the asset-formats resources, read without claiming anything.

#include "McpFabProvider.h"
#include "McpFabBridgeDispatch.h"
#include "McpFabListingScript.h"
#include "McpFabSelectionScript.h"

#include "Dom/JsonObject.h"
#include "McpFabAddScript.h"
#include "Misc/Guid.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpFabDetails, Log, All);

namespace McpFabDetailsOperation
{
namespace
{
/**
 * Composed natively; only the validated uid and the running engine version vary.
 *
 * The thumbnail is fetched and encoded inside the page, so its URL is used and
 * discarded there. The callback caps payloads at 256 KiB, so the image is
 * dropped rather than truncated when a preview is unusually large -- a
 * half-written base64 string would decode to nothing and look like corruption.
 */
FString BuildDetailsScript(const FString& RequestId, const FString& ListingId, const FString& EngineVersion)
{
	return FString::Printf(TEXT(R"JS(
(function () {
  var id = "%s", listing = "%s", engine = "%s";
%s
%s
  function send(o) { try { window.ue.mcpfab.onresult(id, JSON.stringify(o)); } catch (e) {} }
  function fail(e) { try { window.ue.mcpfab.onerror(id, String(e).slice(0, 400)); } catch (_) {} }
  var out = { listingId: listing };
  var base = "https://www.fab.com/i/listings/" + encodeURIComponent(listing);
  // One resource of the listing: its JSON, or null when Fab answers anything but 200. A listing that lacks
  // one is an answer, not an error.
  function read(path) {
    return fetch(base + path, { credentials: "include" })
      .then(function (r) { return r.ok ? r.json() : null; })
      .catch(function () { return null; });
  }
  // The order the add prefers formats in, so the format named here is the one it would import.
  var preferred = ["unreal-engine", "gltf", "glb", "fbx", "obj", "usdz"];
  function fileFacts(f) {
    var o = { name: String((f && f.name) || "").slice(0, 120) }, bytes = fileBytes(f), tier = tierOf(f && f.name);
    if (bytes >= 0) { o.bytes = bytes; }
    if (tier) { o.quality = tier; }
    return o;
  }
  // What the add would do with this listing, read the way the add reads it.
  function planAdd(j, codes) {
    var code = null, warnings = [], blocked = "", why = "";
    for (var p = 0; p < preferred.length && !code; p++) {
      if (codes.indexOf(preferred[p]) >= 0) { code = preferred[p]; }
    }
    return read("/asset-formats").then(function (all) {
      var list = Array.isArray(all) ? all : ((all && (all.results || all.formats)) || []);
      out.formats = list.slice(0, 8).map(function (f) {
        return {
          code: String((f.assetFormatType && f.assetFormatType.code) || f.code || "?"),
          files: (f.files || []).slice(0, 12).map(fileFacts)
        };
      });
      return code ? read("/asset-formats/" + encodeURIComponent(code)) : null;
    }).then(function (fmt) {
      var chosen = null;
      if (code) { out.addFormat = code; }
      if (code && !fmt) {
        warnings.push("Fab answered no file list for the " + code + " format, so the version and size could not be read.");
      }
      if (code === "unreal-engine" && fmt) {
        // The pack's build for this engine: what the add would import, and whether it is the engine's own.
        var versions = fmt.versions || [], builds = engineList(versions), picked = pickVersion(versions, engine);
        out.distributionMethod = String(fmt.distributionMethod || "");
        out.runningEngine = engine;
        if (builds.length) { out.engineVersions = builds; }
        if (picked) {
          chosen = picked.version;
          out.versionName = String(chosen.name || "");
          if (picked.engine) { out.pickedEngineVersion = picked.engine; }
          out.supportsRunningEngine = picked.match === "exact";
          if (picked.match !== "unknown") { out.engineMatch = picked.match; }
          if (picked.match === "older" || picked.match === "newer") {
            warnings.push("No build of this pack declares the running engine " + engine + ": the add would import the " +
              picked.engine + " build, made for " + (picked.match === "older" ? "an older" : "a newer") + " engine.");
          }
        }
      } else if (code && fmt) {
        // A source format: one file, or one file per quality tier. Megascans offers raw, high, mid and low.
        var ready = readyFiles(fmt), tiers = qualitiesOf(ready);
        chosen = pickFile(ready, code);
        if (tiers.length) { out.qualities = tiers; }
        if (chosen) {
          out.downloadFile = String(chosen.name || "").slice(0, 120);
          if (tierOf(chosen.name)) { out.quality = tierOf(chosen.name); }
        }
      }
      if (chosen) {
        // Unknown is not zero: a pack publishes no size, so the bytes are left out and the flag says why.
        var bytes = fileBytes(chosen), merged = !/quixel/i.test(String(out.seller || "")) &&
          ["gltf", "glb", "fbx", "obj", "usdz"].indexOf(code) >= 0;
        out.downloadSizeKnown = bytes >= 0;
        if (bytes >= 0) { out.downloadBytes = bytes; }
        if (merged && bytes >= SCENE_BYTES) {
          warnings.push("The file is " + Math.round(bytes / 1e6) + " MB. Fab merges every mesh of a file into ONE static mesh, so the add " +
            "refuses a file this large (LARGE_SCENE_FILE) until combineMeshes says how to import it: false makes each mesh its " +
            "own asset, true accepts the single merged mesh.");
        }
      }
      if (!code) {
        var mh = /metahuman/i;
        if (codes.some(function (c) { return mh.test(c); }) || mh.test(String(j.listingType || ""))) {
          blocked = "METAHUMAN_FORMAT";
          why = "MetaHuman listing: Fab hands it to its own MetaHuman import workflow, which the add does not drive";
        } else {
          blocked = "NO_IMPORTABLE_FORMAT";
          why = "ships no format Fab can import: unreal-engine, gltf, glb, fbx, obj or usdz";
        }
      } else if (out.distributionMethod === "complete_project") {
        blocked = "COMPLETE_PROJECT";
        why = "complete project: create it as a new project from Fab, then migrate its content";
      }
      out.canAddToProject = !blocked;
      if (blocked) { out.addBlockedCode = blocked; out.addBlockedReason = why; }
      if (warnings.length) { out.addWarnings = warnings; }
    });
  }
)JS") /* MSVC C2026 caps ONE wide literal near 8190 chars; adjacent literals join into the same string */ TEXT(R"JS(  // Pick the SMALLEST usable thumbnail variant, not the first.
  //
  // Fab publishes each thumbnail at several widths and lists the full-size
  // media first. Taking that one meant the fetched preview was routinely
  // larger than the reply cap, so every listing came back with hasImage
  // false -- the cap was doing its job and the caller still got no picture.
  // A preview only has to be recognisable, so the narrowest variant at or
  // above 256px is chosen instead, and the widths actually offered are
  // reported when even that does not fit.
  function preview(j) {
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
  }
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
      var codes = (j.assetFormats || []).map(function (f) {
        return String(f.assetFormatType ? f.assetFormatType.code : (f.code || f.type || "?"));
      }).slice(0, 12);
      out.assetFormats = codes;
      out.hasUnrealBuild = codes.some(function (c) { return c === "unreal-engine"; });
      listingFacts(j, out);
      return planAdd(j, codes).then(function () { return preview(j); });
    })
    .catch(fail);
})();
)JS"), *RequestId, *ListingId, *EngineVersion, McpFabSelection::Script(), McpFabListing::Script());
}
} // namespace

bool Start(const FString& ListingId, const FString& EngineVersion, TFunction<void(bool, const FString&)> OnComplete)
{
	if (!McpFabAddOperation::IsSafeListingId(ListingId))
	{
		OnComplete(false, TEXT("{\"error\":\"INVALID_LISTING_ID\"}"));
		return true;
	}
	FString Error;
	FString ErrorCode;
	const bool bDispatched = McpFabBridgeDispatch::Dispatch(
		[&ListingId, &EngineVersion](const FString& RequestId)
		{
			return BuildDetailsScript(RequestId, ListingId, EngineVersion);
		},
		OnComplete,
		Error, ErrorCode);

	if (!bDispatched)
	{
		OnComplete(false, McpFabBridgeDispatch::FailurePayload(*ErrorCode, Error));
	}
	return true;
}
} // namespace McpFabDetailsOperation
