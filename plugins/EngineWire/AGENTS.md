# MCP AUTOMATION BRIDGE PLUGIN

Editor-only UE 5.0-5.8 plugin. It owns the native `/mcp` HTTP/SSE server (the only gateway; the npm package is a stdio proxy in front of it), the WebSocket automation bridge, the delay-loaded Fab adapter module, a Data Validation module that runs lint_blueprint's rules, and a game features module. It is the sole authority on security. Version: `EngineWire.uplugin` (`0.6.0`), rewritten by the `bump-version.yml` workflow with the other version files.

## SCOPE MAP
| Area | Owner | Notes |
|------|-------|-------|
| Manifest/config/docs | plugin root | `.uplugin`, `Config/`, `README.md`, `CHANGELOG.md` |
| Module dependencies | `Source/EngineWire/EngineWire.Build.cs` | Optional-module detection; keep the compatibility macros |
| Fab adapter | `Source/EngineWireFab/` | Delay-loaded; compiles away when Fab/Megascans are absent |
| Lint validator | `Source/EngineWireValidation/` | lint_blueprint's rules as a Data Validation validator (Validate Assets, saves, commandlets); its own module because a validator UCLASS needs DataValidation at compile time; LoadingPhase None, loaded by EngineWire's startup only where DataValidation is on |
| Game features adapter | `Source/EngineWireGameFeatures/` | list_game_features, set_game_feature_active and create_plugin's game_feature template behind `Public/McpGameFeaturesProvider.h` (the Game Features subsystem has no reflected functions); depends on GameFeatures, named in the .uplugin as an optional plugin it does not turn on (`Enabled: false`); LoadingPhase None, loaded by EngineWire's startup only where GameFeatures is on; registers nothing on 5.0 (state enum private) |
| Public API/settings | `Source/EngineWire/Public/` | Subsystem, settings, connection manager, queue fairness |
| Core | `Private/Core/` (41) | Queue, game-thread drain, the one handler table, pre-queue gate — nested `AGENTS.md` |
| Domains | `Private/Domains/` (1,463 files, 58 domains; counts include subfolders, 2026-10-29) | Action implementations — nested `AGENTS.md` |
| Shared helpers | `Private/Foundation/` (142) | Reflection, Blueprint, path, JSON, response helpers — nested `AGENTS.md` |
| Native MCP | `Private/MCP/` (160) | Registry/session/transport, generated shards — nested `AGENTS.md` |
| Hazardous UE ops | `Private/Safety/` (19) | Save/load/delete wrappers — nested `AGENTS.md` |
| WebSocket transport | `Private/Transport/` (23) | Auth, sockets, framing, TLS, rate limits — nested `AGENTS.md` |
| Native C++ tests | `Private/Tests/` (38) | Automation tests |
| Status UI | `Private/UI/` (2) | Thin Slate presentation |

## CROSS-SURFACE RULES
- Both transports authorize through the pre-queue gate, enqueue into `UEngineWireSubsystem`, and resolve through the handler table in `Core/Subsystem/...HandlerRegistration.cpp`.
- Native MCP tool metadata is generated from the TS capability records (`Private/MCP/Generated/`, `Private/MCP/Tools/McpGeneratedParentRegistry.cpp`); accepted calls dispatch back through the same queue.
- A new behaviour needs a domain implementation, a route in the handler table, a capability record, and an integration case.
- Exactly one game-thread dequeuer drains the queue (`Public/McpQueueFairness.h`).
- A capability id never becomes a metric label (`Core/Security/McpPrequeueGate.h`).
- Use `McpSafeAssetSave`, `McpSafeLevelSave`, `McpSafeLoadMap` and the delete wrappers; never `UPackage::SavePackage()` directly.
- WebSocket clients must complete `bridge_hello` before automation requests.

## VERSION AND SECURITY
- Defaults: `127.0.0.1`, native `/mcp` on at port `3000`, WebSocket ports `8090,8091`, non-loopback disabled.
- LAN binding requires explicit `bAllowNonLoopback`; never add an implicit `0.0.0.0` fallback. Both listeners refuse non-loopback unless `bRequireCapabilityToken` is also on.
- `bRequireCapabilityToken` is on by default: the plugin generates a per-install token at `<ProjectRoot>/Saved/MCP/capability-token` (64 lowercase hex chars); the stdio proxy reads it and never writes it. A token set in Project Settings wins. Both transports compare with `McpConstantTimeTokenEquals`.
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
