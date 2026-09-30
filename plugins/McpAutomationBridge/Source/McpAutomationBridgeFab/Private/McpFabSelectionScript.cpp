// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpFabSelectionScript.h"

namespace McpFabSelection
{
const TCHAR* Script()
{
	// No percent sign may appear in this text: it is spliced into an FString::Printf format.
	return TEXT(R"JS(
  // Major and minor of an engine string ("UE_5.8", "5.8", "5.8.3") as one comparable number, or -1.
  // Fab spells its engine versions "UE_5.x"; splitting on the dot alone left that prefix in place and
  // no version ever matched the running engine.
  function engineNum(v) {
    var m = /(\d+)\.(\d+)/.exec(String(v));
    return m ? Number(m[1]) * 1000 + Number(m[2]) : -1;
  }
  // The version of an unreal-engine pack the add imports: the one declaring the running engine, else
  // the first. A null result means the format published no version at all.
  function pickVersion(versions, engine) {
    var want = engineNum(engine);
    for (var i = 0; i < versions.length; i++) {
      var evs = versions[i].engineVersions || [];
      for (var j = 0; j < evs.length; j++) {
        if (engineNum(evs[j]) === want) { return { version: versions[i], exact: true }; }
      }
    }
    return versions.length ? { version: versions[0], exact: false } : null;
  }
  // The quality tier a file name carries: raw, high, mid or low, else "". Megascans names it as a
  // suffix ("..._ue_high.zip"); the last match wins because a tier word can sit inside the asset name.
  function tierOf(name) {
    var re = /_(raw|high|mid|low)(?=[._]|$)/gi, m, last = "";
    while ((m = re.exec(String(name || ""))) !== null) { last = m[1].toLowerCase(); }
    return last;
  }
  function readyFiles(fmt) {
    return ((fmt && fmt.files) || []).filter(function (f) {
      return f && f.uid && String(f.status || "").toUpperCase() !== "FAILED";
    });
  }
  // The file of a source format the add downloads. Megascans publishes one file per quality tier --
  // raw, high, mid, low -- all typed "source", so matching fileType to the format picks nothing and
  // taking the first grabs raw: the unprocessed scan, 323 MB for a campfire, rather than the
  // game-ready asset. The tier order is therefore explicit, and raw stays last.
  function pickFile(ready, code) {
    var order = ["high", "mid", "low", "raw"], chosen = null;
    for (var t = 0; t < order.length && !chosen; t++) {
      for (var k = 0; k < ready.length; k++) {
        if (tierOf(ready[k].name) === order[t]) { chosen = ready[k]; break; }
      }
    }
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
