# NATIVE MCP

Direct plugin MCP implementation for Streamable HTTP/SSE at `/mcp`. This subtree owns protocol metadata, sessions, dynamic tool visibility, and translation into the bridge subsystem; it does not own editor action implementations.

## STRUCTURE
| Area | Responsibility |
|------|----------------|
| `DynamicTools/` | Enabled state, categories, protected tools, list-changed notification |
| `Protocol/` | JSON-RPC parse/build helpers and MCP tool-result envelopes |
| `Registry/` | Canonical-name gate, static definitions, cached schemas |
| `Routing/` | Consolidated parent-tool action routing helpers |
| `Tools/<Category>/` | (Historical per-tool `MCP_REGISTER_TOOL` classes were removed) Native MCP tool definitions are now generated into the native registry from the canonical records; the registry reads canonical name/description/category/schema/dispatch metadata |
| `Transport/` | Bind/listen, HTTP parsing, sessions, SSE, pending requests, shutdown |

## CANONICAL SURFACE
`FMcpToolRegistry::Register()` accepts exactly these 23 names:

```text
manage_tools, manage_asset, manage_blueprint, control_actor, control_editor, manage_level
build_environment, animation_physics, system_control, manage_sequence, inspect
manage_audio, manage_geometry, manage_effect, manage_gas, manage_character, manage_combat
manage_ai, manage_inventory, manage_interaction, manage_networking, manage_level_structure, manage_pcg
```

- The native registration is **generated** from the canonical tool/action records (the TypeScript `consolidated-tool-definitions.ts` is the canonical facade over that metadata); the handwritten per-tool `MCP_REGISTER_TOOL` classes have been removed. `Registry/McpToolRegistry.cpp` is authoritative for the runtime registry; only canonical names survive, and duplicate names are ignored.
- Do not infer the exposed native surface from the number of `McpTool_*.cpp` files — the per-tool C++ files no longer exist. `Registry/McpToolRegistry.cpp` is authoritative.
- Adding a canonical registrar entry alone cannot expose a new parent tool. Update the canonical gate deliberately, keep TS/native parity, and justify context growth.
- `tools/list` filters accepted registry entries by dynamic enabled state; `tools/call` enforces the same state before dispatch.

## TOOL DEFINITIONS
- Tool definitions are metadata only. Build schemas with `McpSchemaBuilder`; do not hand-assemble repetitive schema JSON.
- Pattern A returns the parent tool name from `GetDispatchAction()` and lets the handler read the sub-action.
- Pattern B returns an empty dispatch action; transport extracts `GetActionFieldName()` from arguments and dispatches that value.
- Transport mirrors `action` into `subAction` for handlers that still require the older payload field. Do not spread additional alias normalization.
- Keep definition names, action enums, required fields, routing helpers, TS schemas, and handler payload expectations aligned.
- `manage_tools` is intercepted locally and returns a one-shot response; other tool calls queue through `UMcpAutomationBridgeSubsystem` and complete over SSE.

## DYNAMIC TOOLS
- Startup enables all accepted tools when `bLoadAllToolsOnStart` is true; otherwise it enables the `core` category.
- `manage_tools` and `inspect` are protected tools. The `core` category cannot be disabled.
- State changes must emit `notifications/tools/list_changed`; preserve locking around tool/category state and cached registry schemas.

## TRANSPORT LIFECYCLE
- `POST /mcp` handles JSON-RPC; `GET /mcp` opens the persistent notification SSE stream; `DELETE /mcp` terminates a session and its streams.
- `initialize` must carry an id and returns `Mcp-Session-Id`. All later requests and notification streams require a valid session header.
- Client notifications receive HTTP 202 after validation. `tools/call` owns its socket until the streamed result completes.
- Return JSON-RPC errors through `McpJsonRpc` and tool outcomes through MCP `content[]` plus `isError`; never leak raw handler JSON as the top-level response.
- Do not block socket threads on Unreal work. Shutdown intentionally pumps game-thread tasks while draining active connections and async writes.

## SECURITY
- Empty/`localhost` listen hosts normalize to loopback. A disallowed non-loopback host falls back to `127.0.0.1`.
- **Fail-closed LAN coupling**: the native transport refuses to bind non-loopback unless `bRequireCapabilityToken` is also enabled (`SECURITY: refusing to bind native MCP to non-loopback` in `Transport/McpNativeTransportLifecycle.cpp`). A LAN-exposed surface can never start without auth.
- When capability auth is enabled, require `X-MCP-Capability-Token` before method dispatch.
- **Constant-time token checks**: `McpConstantTimeTokenEquals` (`Private/Foundation/McpSecureTokenCompare.h`) compares the token with no data-dependent early exit, so timing never leaks how much of a token matched.
- **Session-scoped bounded cancellation**: `notifications/cancelled` correlates only to the caller's in-flight request, keyed by the client JSON-RPC id and the owning session id, so one session cannot cancel another. The cancel-marker maps (`CancelledInternalRequestIds` + `CancelledMarkerOrder`) are capped by `MaxCancelledMarkers` with oldest-first eviction, and a late response for a cancelled request is suppressed (the SSE socket closes without a result). See `Transport/McpNativeTransportCancellation.cpp` and the C4 contract test.
- Browser Origin/CORS access is allowed only under capability-token protection; preserve origin rejection and preflight behavior.
- Keep request-size limits, session expiry, method/path checks, write serialization, and socket ownership accounting intact.

## PROTOCOL VERSION NEGOTIATION (intentional legacy asymmetry)
The native transport supports **exactly the three modern MCP versions**:
`2025-11-25` (latest), `2025-06-18`, and `2025-03-26` (see `McpSupportedProtocolVersions` in `Transport/McpNativeTransportPrivate.h`). At `initialize` it echoes the highest mutually supported version, or the latest for an unknown well-formed request; `McpDefaultProtocolVersion()` (`2025-03-26`) backs post-initialize requests that omit the `MCP-Protocol-Version` header.

- **The native surface deliberately does NOT implement the later `2026-07-28` release-candidate version.** That RC is fictional for this codebase and is explicitly excluded from `McpSupportedProtocolVersions`; the contract test asserts it never appears as a listed/implemented version.
- **Asymmetry with the TS SDK**: the TypeScript stdio server negotiates through the MCP SDK's `SUPPORTED_PROTOCOL_VERSIONS`, which also accepts two older legacy versions (`2024-11-05` and `2024-10-07`). The native `/mcp` transport is intentionally stricter (modern versions only), so a client pinned to a legacy version will negotiate with the TS surface but not the native surface.

## GATEWAY DISCOVERY
When gateway mode is on (`bEnableNativeGateway`, default `true`), the native surface exposes the single `unreal` tool and mirrors the TypeScript gateway's progressive discovery. `describe` drills down in three levels and never dumps a full `inputSchema`:
1. `describe { tool }` -> tool summary + paginated/filterable action list.
2. `describe { tool, action }` -> paginated/filterable parameter catalog (the **tool-union**, not action-specific).
3. `describe { tool, action, param }` -> exactly one parameter's full schema.

`perActionSchemas` is **always `false`**: parameters are the union catalog across all actions of the parent tool, and a parameter is passed only when relevant to the selected action. Invalid tool/action/param calls return closest-match `suggestions` and an executable `nextCall` payload (guided errors). The generated manifest `McpNativeGatewayManifest.h` is the single source of truth shared with the TS gateway; `McpNativeGatewayDescribe.cpp` is only a registry-fallback when that manifest fails to load.

## VALIDATION
```bash
npm run test:native-parity
npm run test:params
```

- Parity verifies canonical TS/native parent tools; the strict parameter audit catches schema and action mismatches.
