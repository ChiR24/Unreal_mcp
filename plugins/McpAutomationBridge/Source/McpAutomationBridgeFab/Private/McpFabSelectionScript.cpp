// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpFabSelectionScript.h"

namespace McpFabSelection
{
const TCHAR* Script()
{
	// No percent sign may appear in this text: it is spliced into an FString::Printf format.
	return TEXT(R"JS(
  // A single mesh file this large is almost always a whole scene, not an asset: merged into one static mesh
  // it needed 14 GB to build. The add refuses it until the caller says how to import it, and the listing
  // details warn about it, so both read this one number.
  var SCENE_BYTES = 52428800;
  // Major and minor of an engine string ("UE_5.8", "5.8", "5.8.3") as one comparable number, or -1.
  // Fab spells its engine versions "UE_5.x"; splitting on the dot alone left that prefix in place and
  // no version ever matched the running engine.
  function engineNum(v) {
    var m = /(\d+)\.(\d+)/.exec(String(v));
    return m ? Number(m[1]) * 1000 + Number(m[2]) : -1;
  }
  // The version of an unreal-engine pack the add imports: the highest one at or below the running engine,
  // else the lowest above it. match says which -- exact (it declares the running engine), older (the
  // newest build for an earlier engine) or newer (only later engines are declared) -- and engine is the
  // engine version that decided it. A pack whose versions declare no engine at all falls back to the
  // first version, match "unknown". A null result means the format published no version.
  function pickVersion(versions, engine) {
    var want = engineNum(engine), exact = null, below = null, belowN = -1, above = null, aboveN = 1e9;
    for (var i = 0; i < versions.length; i++) {
      var evs = versions[i].engineVersions || [];
      for (var j = 0; j < evs.length; j++) {
        var n = engineNum(evs[j]), hit = { version: versions[i], engine: String(evs[j]) };
        if (n < 0) { continue; }
        if (n === want) { exact = exact || hit; }
        else if (n < want && n > belowN) { below = hit; belowN = n; }
        else if (n > want && n < aboveN) { above = hit; aboveN = n; }
      }
    }
    var pick = exact || below || above || (versions.length ? { version: versions[0], engine: "" } : null);
    if (pick) { pick.match = exact ? "exact" : below ? "older" : above ? "newer" : "unknown"; }
    return pick;
  }
  // Every engine version a pack's versions declare, lowest first, each once.
  function engineList(versions) {
    var seen = {}, all = [];
    versions.forEach(function (v) {
      (v.engineVersions || []).forEach(function (e) {
        var k = String(e);
        if (!seen[k] && engineNum(k) >= 0) { seen[k] = 1; all.push(k); }
      });
    });
    return all.sort(function (a, b) { return engineNum(a) - engineNum(b); });
  }
  // The quality tier a file name carries: raw, high, mid or low, else "". Megascans names it as a
  // suffix ("..._ue_high.zip"); the last match wins because a tier word can sit inside the asset name.
  function tierOf(name) {
    var re = /_(raw|high|mid|low)(?=[._]|$)/gi, m, last = "";
    while ((m = re.exec(String(name || ""))) !== null) { last = m[1].toLowerCase(); }
    return last;
  }
  // The tiers a set of files offers, best first.
  function qualitiesOf(files) {
    return ["raw", "high", "mid", "low"].filter(function (t) {
      return files.some(function (f) { return tierOf(f.name) === t; });
    });
  }
  function readyFiles(fmt) {
    return ((fmt && fmt.files) || []).filter(function (f) {
      return f && f.uid && String(f.status || "").toUpperCase() !== "FAILED";
    });
  }
  // The file of a source format that carries exactly this quality tier, or null.
  function pickTier(ready, tier) {
    for (var k = 0; k < ready.length; k++) {
      if (tierOf(ready[k].name) === tier) { return ready[k]; }
    }
    return null;
  }
  // The file of a source format the add downloads when the caller named no quality. Megascans publishes
  // one file per quality tier -- raw, high, mid, low -- all typed "source", so matching fileType to the
  // format picks nothing and taking the first grabs raw: the unprocessed scan, 323 MB for a campfire,
  // rather than the game-ready asset. The tier order is therefore explicit, and raw stays last.
  function pickFile(ready, code) {
    var order = ["high", "mid", "low", "raw"], chosen = null;
    for (var t = 0; t < order.length && !chosen; t++) { chosen = pickTier(ready, order[t]); }
    // Several entries can share a format and only one is the model itself.
    var want = String(code || "").toLowerCase();
    for (var m = 0; m < ready.length && !chosen; m++) {
      var ft = String(ready[m].fileType || "").toLowerCase();
      var nm = String(ready[m].name || "").toLowerCase();
      if (want && (ft.indexOf(want) !== -1 || nm.indexOf("." + want) !== -1)) { chosen = ready[m]; }
    }
    return chosen || ready[0] || null;
  }
  // Bytes of a file, or -1 when Fab publishes none: unreal-engine packs report null, and null is not zero.
  function fileBytes(f) {
    var n = f && (typeof f.fileSize === "number" ? f.fileSize : f.size);
    return typeof n === "number" && n >= 0 ? n : -1;
  }
)JS");
}
} // namespace McpFabSelection
