# MCP AUTOMATION BRIDGE PLUGIN

Editor-only UE 5.0-5.8 Preview plugin. It owns the WebSocket automation bridge and the optional native `/mcp` HTTP/SSE server. `McpAutomationBridge.uplugin` is the plugin version source (`0.5.30`); server package versions are separate.

## SCOPE MAP
| Area | Owner | Notes |
|------|-------|-------|
| Manifest/config/docs | plugin root | `.uplugin`, `Config/`, plugin `README.md` and `CHANGELOG.md` |
| Module dependencies | `Source/McpAutomationBridge/McpAutomationBridge.Build.cs` | Preserve UE-version probes and optional-module detection |
| Public API/settings | `Source/McpAutomationBridge/Public/` | Subsystem contract, settings, connection manager API |
| Core lifecycle/routing | `Private/Core/` | Queue, game-thread dispatch, registration shards, settings, responses |
| Automation domains | `Private/Domains/` | Domain handlers grouped by responsibility |
| Shared helpers | `Private/Foundation/` | Reflection, Blueprint, path, response, and handler primitives |
| Native MCP | `Private/MCP/` | Read its nested `AGENTS.md`; separate registry/session/transport lifecycle |
| Hazardous UE operations | `Private/Safety/` | Save/load/delete/material wrappers and verification |
| WebSocket transport | `Private/Transport/` | Connection auth, sockets, framing, TLS, rate limits, telemetry |
| Status UI | `Private/UI/` | Keep Slate presentation thin; do not move transport work here |

## CROSS-SURFACE RULES
- WebSocket actions enter through `UMcpAutomationBridgeSubsystem`, queue to the game thread, and resolve through `InitializeHandlers()` registration shards.
- Native MCP metadata does not implement editor behavior; accepted tools dispatch back through the same subsystem queue.
- A new behavior normally needs a domain implementation, Core registration, the TypeScript parent-tool/action contract, and tests. Add native metadata only when the native surface should expose it.
- Keep editor API work off socket threads. Preserve deferral during package save, garbage collection, async load, and unsafe map transitions.
- Optional engine features must compile away or fail clearly when their module/plugin is unavailable; retain the compatibility macros in `Build.cs`.
- Keep action dispatchers thin. Put behavior in the matching `Private/Domains/<Domain>/<Responsibility>/` implementation and register it through the appropriate `Private/Core/Subsystem/*Registration.cpp` shard.
- Reuse `Private/Foundation/` for shared reflection, path, Blueprint, JSON, response, and object-resolution behavior; do not grow domain-local copies.
- Use `McpSafeAssetSave`, `McpSafeLevelSave`, `McpSafeLoadMap`, and the existing delete wrappers. Never call `UPackage::SavePackage()` directly from a domain handler.
- WebSocket clients must complete `bridge_hello` before automation requests. Preserve request/socket correlation, heartbeat cleanup, frame-size limits, and delegate unbinding during shutdown.

## VERSION AND SECURITY
- `McpAutomationBridge.uplugin` owns bridge version fields; coordinated releases must also follow the repo-wide server version workflow.
- Defaults are `127.0.0.1`, ports `8090,8091`, multi-listen enabled, and non-loopback disabled.
- LAN binding requires explicit `bAllowNonLoopback`; never introduce an implicit `0.0.0.0` fallback.
- **Fail-closed LAN coupling**: the native `/mcp` transport refuses to bind non-loopback unless `bRequireCapabilityToken` is also enabled, so a LAN-exposed surface can never start without auth. The plugin's non-loopback setting governs both server-side listeners it owns (WebSocket bridge listen socket and native MCP HTTP/SSE transport).
- `bRequireCapabilityToken` protects both transports. WebSocket uses the bridge hello token; native MCP uses `X-MCP-Capability-Token`.
- **Constant-time token checks**: both transports compare capability tokens with `McpConstantTimeTokenEquals` (`Private/Foundation/McpSecureTokenCompare.h`), XOR-accumulating over the full UTF-8 byte span with no data-dependent early exit, so timing never leaks how much of a token matched.
- **Session-scoped bounded cancellation**: native `notifications/cancelled` correlates only to the caller's in-flight request (scoped by session id and client JSON-RPC id key), and the cancel-marker maps are capped with oldest-first eviction; a late response for a cancelled request is suppressed. See the nested MCP `AGENTS.md` for lifecycle detail.
- TLS settings belong to the WebSocket transport. Preserve certificate/key validation, rate limits, heartbeat handling, and response sanitization.

## PACKAGING
- Package with `./scripts/package-plugin.sh <UnrealEngineRoot> [output-dir]` or `scripts/package-plugin.bat`.
- The scripts run `RunUAT BuildPlugin`, set `Installed: true` in staged output, exclude `Intermediate/` content and debug symbols from the archive, and write the archive under root `build/` by default.
- `Config/FilterPlugin.ini` explicitly includes plugin `README.md` and `CHANGELOG.md`; keep distribution-only additions there.
- `npm run automation:sync` copies this source plugin into an Engine or Project plugin directory; it is not a build.
- Never edit generated `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, root `build/`, or uppercase staging mirrors.

## VALIDATION
```bash
npm run test:native-parity
npm run test:params
./scripts/package-plugin.sh /data/UnrealEngine /tmp/mcp-plugin-package
```

- Use the packaging build for C++/UBT validation across the intended engine version.
- Keep the worktree's unrelated generated or user changes intact.
