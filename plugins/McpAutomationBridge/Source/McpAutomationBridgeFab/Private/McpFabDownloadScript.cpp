// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpFabDownloadScript.h"

namespace McpFabDownload
{
const TCHAR* Script()
{
	// No percent sign may appear in this text: it is spliced into an FString::Printf format.
	return TEXT(R"JS(
  // The status and text of the last download-info attempt: "id|mode|status|detail" as it was recorded.
  function lastAttempt() {
    var parts = out.attempts && out.attempts.length ? String(out.attempts[out.attempts.length - 1]).split("|") : [];
    return { status: Number(parts[2]) || 0, detail: parts.slice(3).join("|") };
  }
  // Names the first step that went wrong, with Fab's own status and words, so a refused download says
  // where it stopped instead of only that no address came back. A claim that failed comes first: Fab
  // answers download-info with 404 for a listing the account could not add to its library, so the
  // download-info failure that follows is only its shadow.
  function explainNoDownload() {
    var last = lastAttempt();
    if (!out.offerIds.length) {
      out.failedStep = "license";
      out.stepStatus = 0;
      out.stepDetail = "the listing publishes no offer to claim";
    } else if (out.entitleStatus >= 400 || out.entitleError) {
      out.failedStep = "claim";
      out.stepStatus = out.entitleStatus || 0;
      out.stepDetail = out.entitleDetail || out.entitleError || "";
    } else {
      out.failedStep = "download-info";
      out.stepStatus = last.status;
      out.stepDetail = last.detail;
    }
  }
  // The signed address in a download-info entry. The usual keys come first; Fab's shape has changed
  // before, so any url-named string inside the entry is taken next. A preview, thumbnail or image
  // address is never taken: handing one to the importer would download a picture.
  function findDownloadUrl(info) {
    var direct = info.url || info.downloadUrl || info.signedUrl || info.href;
    if (direct) { return String(direct); }
    var found = "";
    (function walk(o, depth) {
      if (found || !o || typeof o !== "object" || depth > 3) { return; }
      Object.keys(o).forEach(function (k) {
        var v = o[k];
        if (found) { return; }
        if (typeof v === "string" && /^https?:\/\//i.test(v) && /(url|link|href)$/i.test(k) && !/thumb|preview|image|icon|media/i.test(k)) {
          found = v;
        } else if (v && typeof v === "object") {
          walk(v, depth + 1);
        }
      });
    })(info, 0);
    return found;
  }
  // ?platform=Windows suits a packaged per-platform build; a source zip has no platform and the filter
  // 404s. Try the platform form, then the bare one, for the file's uid and then its artifactTag, and
  // record every status: which identifier the endpoint wants is the open question, and Fab returns its
  // reason in `detail`, which shape() hides because it reports types.
  function resolveDownload(chosen) {
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
    // Every exit must answer. When the ladder exhausts, step() resolves null, and a page that fell
    // silent left the caller waiting out its own timeout instead of being told why.
    return step(0).then(function (r) {
      if (!r) { out.error = "NO_DOWNLOAD_URL"; explainNoDownload(); send(out); return null; }
      out.downloadStatus = r.status;
      return r.json().then(function (dl) {
        // download-info answers {downloadInfo:[{...}]}, not a flat object.
        out.downloadShape = shape(dl, 3);
        var info = (dl.downloadInfo && dl.downloadInfo.length) ? dl.downloadInfo[0] : dl;
        var url = findDownloadUrl(info);
        if (!url) {
          out.error = "NO_DOWNLOAD_URL";
          out.failedStep = "download-info";
          out.stepStatus = r.status;
          out.stepDetail = "Fab answered but the entry carried no download address (keys: " + Object.keys(info).slice(0, 12).join(", ") + ")";
          send(out);
          return null;
        }
        return { url: url, bases: info.distributionPointBaseUrls || info.baseUrls || dl.distributionPointBaseUrls || [] };
      });
    });
  }
)JS");
}
} // namespace McpFabDownload
