// Copyright (c) 2024 MCP Automation Bridge Contributors

// Reads the *shape* of Fab's catalog responses so the real operation can be
// written against what they contain rather than a guess at it.
//
// Implementation surfaced this unknown: picking an asset format and a file id
// requires knowing how the listing response is structured, and that is not
// something the endpoint map recorded. It reports key names, value types and
// array lengths only -- never values -- because the responses carry signed URLs
// and account-scoped fields that must not reach a log.

#include "McpFabBrowserBridge.h"
#include "McpFabBridgeDispatch.h"

#include "HAL/IConsoleManager.h"
#include "Misc/Guid.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpFabShape, Log, All);

namespace
{
/**
 * Every endpoint here is a literal in native code.
 *
 * Nothing a caller supplies is interpolated into a path. The listing id used is
 * whatever the search returns, so no MCP-controlled string can steer a request
 * at /i/account or /i/auth even though this command is reachable through
 * console_command.
 */
FString BuildShapeScript(const FString& RequestId)
{
	return FString::Printf(TEXT(R"JS(
(function () {
  var id = "%s";
  function shape(v, depth) {
    if (v === null) return "null";
    if (Array.isArray(v)) return depth <= 0 ? "array[" + v.length + "]"
      : { array: v.length, first: v.length ? shape(v[0], depth - 1) : "empty" };
    if (typeof v === "object") {
      if (depth <= 0) return "object";
      var o = {};
      Object.keys(v).slice(0, 40).forEach(function (k) { o[k] = shape(v[k], depth - 1); });
      return o;
    }
    return typeof v;
  }
  function send(o) { try { window.ue.mcpfab.onresult(id, JSON.stringify(o)); } catch (e) {} }
  function fail(e) { try { window.ue.mcpfab.onerror(id, String(e).slice(0, 400)); } catch (_) {} }

  var out = {};
  fetch("https://www.fab.com/i/listings/search?is_free=1&channels=unreal-engine&count=1",
        { credentials: "include" })
    .then(function (r) { out.searchStatus = r.status; return r.json(); })
    .then(function (j) {
      out.searchShape = shape(j, 3);
      var results = j.results || j.items || j.listings || [];
      if (!results.length) { send(out); return null; }
      out.listingIdKeys = Object.keys(results[0]).slice(0, 40);
      var uid = results[0].uid || results[0].id || results[0].listingId;
      if (!uid) { send(out); return null; }
      out.haveListing = true;
      return fetch("https://www.fab.com/i/listings/" + encodeURIComponent(uid) +
                   "/asset-formats/unreal-engine", { credentials: "include" });
    })
    .then(function (r) {
      if (!r) { return null; }
      out.formatsStatus = r.status;
      return r.json();
    })
    .then(function (j) { if (j) { out.formatsShape = shape(j, 4); } send(out); })
    .catch(fail);
})();
)JS"), *RequestId);
}
} // namespace

/**
 * Diagnostic. Reports the structure of Fab's search and asset-format responses.
 * Values are never included, so the output is safe to read in a log.
 */
static FAutoConsoleCommand GMcpFabDescribeCatalogShape(
	TEXT("Mcp.Fab.DescribeCatalogShape"),
	TEXT("Logs the key structure of Fab's search and asset-format responses (shapes only, no values)."),
	FConsoleCommandDelegate::CreateStatic([]()
	{
		FString Error;
		FString ErrorCode;
		if (!McpFabBridgeDispatch::Dispatch(
				[](const FString& RequestId) { return BuildShapeScript(RequestId); },
				[](bool bSuccess, const FString& Payload)
				{
					UE_LOG(LogMcpFabShape, Log, TEXT("catalog shape (%s): %s"),
						bSuccess ? TEXT("ok") : TEXT("failed"), *Payload);
				},
				Error, ErrorCode))
		{
			UE_LOG(LogMcpFabShape, Warning,
				TEXT("Mcp.Fab.DescribeCatalogShape: %s (%s)"), *Error, *ErrorCode);
		}
	}));
