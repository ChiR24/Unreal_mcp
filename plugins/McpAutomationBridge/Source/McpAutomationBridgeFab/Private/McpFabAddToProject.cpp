// Copyright (c) 2024 MCP Automation Bridge Contributors

// The whole add-to-project chain, run inside Fab's authenticated page.
//
// Resolve and add are ONE page-side operation on purpose. Handing the signed URL
// back to C++ so C++ could pass it down again would put transient
// authentication-derived material into this process, where it could reach a log,
// a receipt or JSON-RPC. Here it is minted and consumed without ever leaving the
// page: C++ learns only that the workflow was accepted.
//
// UFabBrowserApi::AddToProject shows what actually matters for a UE listing --
// AssetType must be "unreal-engine", and it then uses AssetId, AssetName and
// DistributionPointBaseUrls to build an FPackImportWorkflow. Everything else on
// FFabAssetMetadata belongs to the Quixel branch.

#include "McpFabAddScript.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpFabAdd, Log, All);

namespace
{
/**
 * A listing id is an opaque Fab uid, so anything outside this set is a caller
 * trying to steer the path. Rejecting here rather than escaping later keeps the
 * guarantee simple: no request can be aimed at /i/account or /i/auth even though
 * console commands are reachable through MCP's console_command.
 */
bool IsSafeListingId(const FString& Value)
{
	if (Value.IsEmpty() || Value.Len() > 64)
	{
		return false;
	}
	for (const TCHAR C : Value)
	{
		const bool bAllowed = FChar::IsAlnum(C) || C == TEXT('-') || C == TEXT('_');
		if (!bAllowed)
		{
			return false;
		}
	}
	return true;
}

/**
 * Composed entirely in native code. ListingId is validated above and passed
 * through encodeURIComponent as well; the format and file segments come from
 * Fab's own response, not from any caller.
 */
FString BuildAddScriptImpl(const FString& RequestId, const FString& ListingId, const FString& EngineVersion)
{
	return FString::Printf(TEXT(R"JS(
(function () {
  var id = "%s", listing = "%s", engine = "%s";
  function shape(v, d) {
    if (v === null) return "null";
    if (Array.isArray(v)) return d <= 0 ? "array[" + v.length + "]" : { array: v.length, first: v.length ? shape(v[0], d - 1) : "empty" };
    if (typeof v === "object") { var o = {}; if (d <= 0) return "object"; Object.keys(v).slice(0, 40).forEach(function (k) { o[k] = shape(v[k], d - 1); }); return o; }
    return typeof v;
  }
  // Mirrors the C++/TS receipt redaction for the two fields that echo
  // third-party response text (a failed claim body and download-info detail).
  // The reply is logged in full on refusal, so secret-shaped values must not
  // survive in it; only the value AFTER masking crosses the boundary.
  function scrub(v) {
    return String(v == null ? "" : v)
      .replace(/(token|secret|passwords?|passwd|pwd|api[_-]?key|authorization)\s*[:=]\s*[^\s,;]+/gi, "$1=[REDACTED]")
      .slice(0, 300);
  }
  function send(o) { try { window.ue.mcpfab.onresult(id, JSON.stringify(o)); } catch (e) {} }
  function fail(e) { try { window.ue.mcpfab.onerror(id, String(e).slice(0, 400)); } catch (_) {} }

  var out = { listing: listing, engine: engine };
  var base = "https://www.fab.com/i/listings/" + encodeURIComponent(listing);

  // The format segment is resolved from the listing, not assumed. Requesting
  // /asset-formats/unreal-engine directly returned 404 for a listing the site
  // tags as Unreal: the site's listing_types filter and the asset-format code
  // are not the same thing.
  fetch(base, { credentials: "include" })
    .then(function (r) { out.listingStatus = r.status; return r.json(); })
    .then(function (listingJson) {
      var formats = (listingJson && listingJson.assetFormats) || [];
      out.formatShape = formats.length ? shape(formats[0], 2) : "none";
      out.formatCodes = formats.map(function (f) {
        return f.assetFormatType ? f.assetFormatType.code : (f.code || f.type || "?");
      }).slice(0, 8);

      // The publisher decides which importer runs: FabBrowserApi::AddToProject
      // tests IsQuixel BEFORE it looks at gltf/fbx, routing Megascans content
      // to FQuixelImportWorkflow instead of the generic Interchange one.
      var seller = (listingJson.user && listingJson.user.sellerName) || "";
      out.seller = String(seller);
      out.isQuixel = /quixel/i.test(String(seller));

      // Preference order mirrors that same dispatch: a packaged unreal-engine
      // build goes through FPackImportWorkflow, and gltf/glb/fbx go through the
      // Quixel or generic Interchange workflow. Demanding unreal-engine here
      // rejected every listing that ships source formats -- the whole
      // Megascans library -- even though Fab's own UI imports them through
      // these very paths. The format is what selects the importer, not a
      // precondition for importing at all.
      var preferred = ["unreal-engine", "gltf", "glb", "fbx"];
      var code = null;
      for (var p = 0; p < preferred.length && !code; p++) {
        for (var i = 0; i < out.formatCodes.length; i++) {
          if (out.formatCodes[i] === preferred[p]) { code = preferred[p]; break; }
        }
      }
      if (!code) { out.error = "NO_IMPORTABLE_FORMAT"; send(out); return null; }
      out.formatCode = code;

      // Entitlement first. download-info answers 404 for a listing the account
      // does not own, which is why an unowned Quixel asset failed at that step
      // while an already-owned one resolved: the miss was never the identifier
      // or the URL shape, it was that nothing had claimed the listing. The Fab
      // UI does this when you press Add to Project; add-to-library is the same
      // call, observed in the plugin's own traffic. Free listings claim without
      // charge, and an already-entitled one is a no-op, so this is safe to run
      // unconditionally -- but it DOES add the listing to the signed-in Fab
      // library, which the capability description states.
      // The listing publishes no `offers` array -- 30 top-level keys and none
      // of them is that -- so the offer id is nested. Rather than hardcode a
      // guess at licenses[].offerId, walk for it and report where it was
      // found, so the path is learned from the response instead of assumed.
      function findOffer(o, depth, path) {
        if (!o || typeof o !== "object" || depth > 4) { return null; }
        for (var k in o) {
          if (!Object.prototype.hasOwnProperty.call(o, k)) { continue; }
          var v = o[k];
          if (typeof v === "string" && v && /^offer(_?id)?$|offerid$/i.test(k)) {
            return { id: v, at: path + "." + k };
          }
          if (v && typeof v === "object") {
            var hit = findOffer(v, depth + 1, path + "." + k);
            if (hit) { return hit; }
          }
        }
        return null;
      }
      var found = findOffer(listingJson, 0, "listing");
      out.offerIds = found ? [String(found.id)] : [];
      out.offerFoundAt = found ? found.at : "";
      if (!found) {
        out.offerShape = Object.keys(listingJson).slice(0, 40);
        out.licenseShape = shape(listingJson.licenses, 3);
        out.priceShape2 = shape(listingJson.startingPrice, 2);
      }
      // Fab answers the claim with {"detail":"CSRF Failed: CSRF token missing."}
      // -- Django REST Framework guarding an unsafe method.
      //
      // The token is taken from the DOM when the page publishes it there, and
      // otherwise from the csrftoken cookie. That cookie read is deliberate and
      // narrow: Django sets csrftoken non-HttpOnly precisely so page script can
      // echo it back as a header. It is an anti-forgery nonce rather than a
      // credential -- it authenticates nothing by itself and is worthless
      // without the session cookie, which is never read, returned or sent. The
      // value is used inline for one same-origin header and is never assigned
      // to `out`, so it cannot reach a reply or a log; only csrfSource, a
      // source name, is reported. The contract suite pins that: cookie access
      // is allowed in this one helper and nowhere else, and a separate rule
      // asserts the value never reaches send/onresult/onerror or a UE_LOG --
      // proving no leakage rather than merely proving an API went uncalled.
      function readCsrfCookie() {
        var parts = String(document.cookie || "").split(";");
        for (var c = 0; c < parts.length; c++) {
          var kv = parts[c].split("=");
          if (kv.length > 1 && kv[0].trim() === "csrftoken") {
            return decodeURIComponent(kv.slice(1).join("=").trim());
          }
        }
        return "";
      }

      var csrf = "";
      var csrfFrom = "none";
      var meta = document.querySelector(
        "meta[name='csrf-token'], meta[name='csrfmiddlewaretoken'], meta[name='csrf_token']");
      if (meta) { csrf = String(meta.getAttribute("content") || ""); csrfFrom = "meta"; }
      if (!csrf) {
        var input = document.querySelector("input[name='csrfmiddlewaretoken']");
        if (input) { csrf = String(input.value || ""); csrfFrom = "form-input"; }
      }
      if (!csrf) { csrf = readCsrfCookie(); csrfFrom = csrf ? "cookie" : "none"; }
      // A source name, never the value.
      out.csrfSource = csrfFrom;

      var claim = Promise.resolve(null);
      if (out.offerIds.length) {
        var form = new FormData();
        form.append("offer_id", out.offerIds[0]);
        if (csrf) { form.append("csrfmiddlewaretoken", csrf); }
        var headers = csrf ? { "X-CSRFToken": csrf } : {};
        claim = fetch(base + "/add-to-library", { method: "POST", credentials: "include", headers: headers, body: form })
          .then(function (r) {
            out.entitleStatus = r.status;
            if (r.ok) { return null; }
            // A 403 here has two opposite readings -- a missing CSRF header,
            // which is fixable, or first-party content that simply cannot be
            // claimed, which is not -- and the reason string is what tells
            // them apart. Reading it costs one field and saves a guess.
            return r.text().then(function (t) {
              out.entitleDetail = scrub(t);
              return null;
            }).catch(function () { return null; });
          })
          .catch(function (e) { out.entitleError = String(e).slice(0, 120); return null; });
      }
      return claim.then(function () {
        return fetch(base + "/asset-formats/" + encodeURIComponent(code), { credentials: "include" });
      });
    })
    .then(function (r) { if (!r) return null; out.formatsStatus = r.status; return r.json(); })
    .then(function (fmt) {
      if (!fmt) return null;
      var versions = fmt.versions || [];
      var chosen = null;
      // Exact major.minor comparison, not substring: "5.1" must not match a
      // listing pinned to "5.10" by containment.
      function sameEngine(v) {
        var parts = String(v).split(".");
        return parts.length >= 2 && parts[0] + "." + parts[1] === engine;
      }
      for (var i = 0; i < versions.length; i++) {
        var evs = versions[i].engineVersions || [];
        for (var j = 0; j < evs.length; j++) {
          if (sameEngine(evs[j])) { chosen = versions[i]; break; }
        }
        if (chosen) break;
      }
      out.engineExactMatch = !!chosen;
      if (!chosen && versions.length) { chosen = versions[0]; }

      // A source format publishes no versions array at all: its downloadable
      // units sit directly under files, each carrying the uid the
      // download-info path wants. Only the packaged unreal-engine format is
      // versioned by engine, so this is a second shape to read rather than a
      // field that went missing -- treating it as missing is what produced
      // NO_VERSION for every gltf and fbx listing.
      if (!chosen) {
        var files = fmt.files || [];
        var ready = files.filter(function (f) {
          return f && f.uid && String(f.status || "").toUpperCase() !== "FAILED";
        });
        out.fileChoices = ready.slice(0, 6).map(function (f) {
          return String(f.name || "") + "|" + String(f.fileType || "") + "|" +
                 String(f.artifactTag || "") + "|" + String(f.size || 0);
        });
        // Prefer the file whose own type names the chosen format; several
        // entries can share a format and only one is the model itself.
        // Megascans publishes one file per quality tier -- raw, high, mid, low
        // -- all typed "source", so matching fileType to the format picks
        // nothing and falling through to files[0] grabs raw: the unprocessed
        // scan, 323 MB for a campfire, rather than the game-ready asset.
        // Preference order is therefore explicit, and raw stays last.
        var tiers = ["_high", "_mid", "_low", "_raw"];
        for (var t = 0; t < tiers.length && !chosen; t++) {
          for (var k = 0; k < ready.length; k++) {
            if (String(ready[k].name || "").toLowerCase().indexOf(tiers[t]) !== -1) {
              chosen = ready[k]; break;
            }
          }
        }
        // out.formatCode, not code: `code` is a local of the previous then()
        // callback and is out of scope here.
        var want = String(out.formatCode || "").toLowerCase();
        for (var m = 0; m < ready.length && !chosen; m++) {
          var ft = String(ready[m].fileType || "").toLowerCase();
          var nm = String(ready[m].name || "").toLowerCase();
          if (want && (ft.indexOf(want) !== -1 || nm.indexOf("." + want) !== -1)) { chosen = ready[m]; }
        }
        if (!chosen && ready.length) { chosen = ready[0]; }
      }
      if (!chosen) {
        out.error = "NO_VERSION";
        out.versionShape = shape(fmt, 3);
        send(out); return null;
      }
      out.versionName = chosen.name || "";
      // ?platform=Windows suits a packaged per-platform build; a source zip
      // has no platform and the filter 404s. Try the platform form, then the
      // bare one, and report both statuses so a future 404 says which shape
      // the endpoint actually wanted.
      // Both the platform-filtered and bare forms 404 for a source format, so
      // the failing segment is an identifier rather than the query. A file
      // entry carries both uid and artifactTag; which one the download-info
      // path wants is the open question, so try each and record every status.
      // Fab returns its reason in `detail`, which shape() hides because it
      // reports types -- that message is the thing worth reading.
      var attempts = [];
      var fmtSeg = encodeURIComponent(out.formatCode);
      [chosen.uid, chosen.artifactTag].forEach(function (ident) {
        if (!ident) { return; }
        var b = base + "/asset-formats/" + fmtSeg + "/files/" + encodeURIComponent(ident) + "/download-info";
        attempts.push({ id: String(ident), url: b + "?platform=Windows" });
        attempts.push({ id: String(ident), url: b });
      });
      out.attempts = [];
      var step = function (i) {
        if (i >= attempts.length) { return Promise.resolve(null); }
        return fetch(attempts[i].url, { credentials: "include" }).then(function (r) {
          return (r.ok ? Promise.resolve(null) : r.json().catch(function () { return {}; }))
            .then(function (body) {
              out.attempts.push(attempts[i].id + "|" + (attempts[i].url.indexOf("platform=") !== -1 ? "platform" : "bare") +
                                "|" + r.status + (body && body.detail ? "|" + scrub(body.detail) : ""));
              if (r.ok) { return r; }
              return step(i + 1);
            });
        });
      };
      // Every exit must answer. When the ladder exhausts, step() resolves null
      // and the handlers below simply return, so the page fell silent and the
      // caller waited out its own timeout instead of being told the download
      // could not be resolved.
      return step(0).then(function (r) {
        if (!r) { out.error = "NO_DOWNLOAD_URL"; send(out); }
        return r;
      });
    })
    .then(function (r) { if (!r) return null; out.downloadStatus = r.status; return r.json(); })
    .then(function (dl) {
      if (!dl) return;
      // download-info answers {downloadInfo:[{...}]}, not a flat object -- the
      // first live run reported NO_DOWNLOAD_URL with downloadInfo present, which
      // is exactly the correction a shape-only diagnostic is for.
      out.downloadShape = shape(dl, 3);
      var info = (dl.downloadInfo && dl.downloadInfo.length) ? dl.downloadInfo[0] : dl;
      var url = info.url || info.downloadUrl || info.signedUrl || info.href;
      var bases = info.distributionPointBaseUrls || info.baseUrls
               || dl.distributionPointBaseUrls || [];
      if (!url) { out.error = "NO_DOWNLOAD_URL"; send(out); return; }
      // AssetType carries the resolved format, because it is the value the
      // plugin switches on. Sending a constant "unreal-engine" for a gltf
      // download would hand a Megascans archive to the pack workflow.
      window.ue.fab.addtoproject(url, {
        AssetId: listing,
        AssetName: out.versionName || listing,
        AssetType: out.formatCode,
        ListingType: out.formatCode,
        AssetNamespace: "",
        DistributionPointBaseUrls: bases,
        IsQuixel: out.isQuixel
      });
      out.accepted = true;
      send(out);
    })
    .catch(fail);
})();
)JS"), *RequestId, *ListingId, *EngineVersion);
}
} // namespace

namespace McpFabAddOperation
{
/** The single script this module ever runs against Fab's page. */
FString BuildAddScript(const FString& RequestId, const FString& ListingId, const FString& EngineVersion)
{
	return BuildAddScriptImpl(RequestId, ListingId, EngineVersion);
}

bool IsSafeListingIdShared(const FString& Value) { return IsSafeListingId(Value); }
} // namespace McpFabAddOperation
