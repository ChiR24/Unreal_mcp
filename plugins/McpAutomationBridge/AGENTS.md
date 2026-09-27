# MCP AUTOMATION BRIDGE PLUGIN

Editor-only UE 5.0-5.8 plugin. It owns the WebSocket automation bridge, the optional native `/mcp` HTTP/SSE server, and the delay-loaded Fab adapter module. It is the sole authority on security: it re-checks everything the TypeScript server checks. Version: `McpAutomationBridge.uplugin` (`0.6.0-beta-b`), rewritten by the `bump-version.yml` workflow with the other version files.

## SCOPE MAP
| Area | Owner | Notes |
|------|-------|-------|
| Manifest/config/docs | plugin root | `.uplugin`, `Config/`, `README.md`, `CHANGELOG.md` |
| Module dependencies | `Source/McpAutomationBridge/McpAutomationBridge.Build.cs` | Optional-module detection; keep the compatibility macros |
| Fab adapter | `Source/McpAutomationBridgeFab/` | Delay-loaded; compiles away when Fab/Megascans are absent |
| Public API/settings | `Source/McpAutomationBridge/Public/` | Subsystem, settings, connection manager, queue fairness |
| Core | `Private/Core/` (26) | Queue, game-thread drain, the one handler table, pre-queue gate — nested `AGENTS.md` |
| Domains | `Private/Domains/` (1,016 files, 55 domains) | Action implementations — nested `AGENTS.md` |
| Shared helpers | `Private/Foundation/` (92) | Reflection, Blueprint, path, JSON, response helpers — nested `AGENTS.md` |
| Native MCP | `Private/MCP/` (137) | Registry/session/transport, generated shards — nested `AGENTS.md` |
| Hazardous UE ops | `Private/Safety/` (18) | Save/load/delete wrappers — nested `AGENTS.md` |
| WebSocket transport | `Private/Transport/` (21) | Auth, sockets, framing, TLS, rate limits — nested `AGENTS.md` |
| Native C++ tests | `Private/Tests/` (25) | Automation tests |
| Status UI | `Private/UI/` (2) | Thin Slate presentation |

## CROSS-SURFACE RULES
- Both transports authorize through the pre-queue gate, enqueue into `UMcpAutomationBridgeSubsystem`, and resolve through the handler table in `Core/Subsystem/...HandlerRegistration.cpp`.
- Native MCP tool metadata is generated from the TS capability records (`Private/MCP/Generated/`, `Private/MCP/Tools/McpGeneratedParentRegistry.cpp`); accepted calls dispatch back through the same queue.
- A new behaviour needs a domain implementation, a route in the handler table, a capability record, and an integration case.
- Exactly one game-thread dequeuer drains the queue (`Public/McpQueueFairness.h`).
- A capability id never becomes a metric label (`Core/Security/McpPrequeueGate.h`).
- Use `McpSafeAssetSave`, `McpSafeLevelSave`, `McpSafeLoadMap` and the delete wrappers; never `UPackage::SavePackage()` directly.
- WebSocket clients must complete `bridge_hello` before automation requests.

## VERSION AND SECURITY
- Defaults: `127.0.0.1`, WebSocket ports `8090,8091`, native `/mcp` off (port `3000` when enabled), non-loopback disabled.
- LAN binding requires explicit `bAllowNonLoopback`; never add an implicit `0.0.0.0` fallback. Both listeners refuse non-loopback unless `bRequireCapabilityToken` is also on.
- `bRequireCapabilityToken` is on by default: the plugin generates a per-install token at `<ProjectRoot>/Saved/MCP/capability-token` (64 lowercase hex chars); the TS bridge reads it and never writes it. A token set in Project Settings wins. Both transports compare with `McpConstantTimeTokenEquals`.
- Cancellation is advisory for in-flight work: queued requests are dropped, an executing editor operation runs to completion.

## PACKAGING
- `node scripts/package-plugin.mjs <UnrealEngineRoot> [output-dir]` runs `RunUAT BuildPlugin`, stages with `Installed: true`, and excludes `Intermediate/` and debug symbols.
- `npm run automation:sync` copies this plugin into an Engine or Project plugin directory; it is not a build.
- Never edit `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/` or root `build/`.

## VALIDATION
```bash
npm run test:unit      # source-contract gates in tests/unit/plugin/
npm run test:params
node scripts/package-plugin.mjs <UnrealEngineRoot> /tmp/mcp-plugin-package
```
