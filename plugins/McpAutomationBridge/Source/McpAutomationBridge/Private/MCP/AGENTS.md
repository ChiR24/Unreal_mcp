# NATIVE MCP

Direct plugin MCP implementation for Streamable HTTP/SSE at `/mcp`. This subtree owns protocol metadata, sessions, dynamic tool visibility, and translation into the bridge subsystem; it does not own editor action implementations.

## STRUCTURE
| Area | Responsibility |
|------|----------------|
| `DynamicTools/` (5) | Enabled state, categories, protected tools |
| `Execute/` (24) | Native execute pipeline: request parse, folded pins, schema validation, receipts |
| `Gateway/` (25, at cap) | Native gateway mirror of the TS engine: catalog, capability store, describe, search, guidance, folding (`McpNativeGatewayFolding`: legacy pairs, pins, `dispatchBy`) |
| `Generated/` (24) | **ALL GENERATED** capability shards (`npm run registry:generate`). Never hand-edit |
| `Primitives/` (7) | Prompts, completions, resource revision |
| `Protocol/` (4) | JSON-RPC parse/build helpers and MCP tool-result envelopes |
| `Registry/` (5) | Canonical-name gate, `FMcpToolDefinition` (name/description/category), `McpSchemaBuilder`, cached schemas |
| `Resources/` (8) | `ue://` resource catalog and readers |
| `Routing/` (7) | Consolidated parent-tool action routing helpers |
| `Tools/` (1) | `McpGeneratedParentRegistry.cpp`: **GENERATED** from the capability records; registers one `FMcpToolDefinition` per parent |
| `Transport/` (25, at cap) | Bind/listen, HTTP parsing, sessions, SSE, pending requests, the keepalive thread (`McpNativeTransportKeepalive.cpp`), shutdown |

## CANONICAL SURFACE
`FMcpToolRegistry::Register()` accepts exactly these 23 names:

```text
manage_tools, manage_asset, manage_blueprint, control_actor, control_editor, manage_level
build_environment, animation_physics, system_control, manage_sequence, inspect
manage_audio, manage_geometry, manage_effect, manage_gas, manage_character, manage_combat
manage_ai, manage_inventory, manage_interaction, manage_networking, manage_level_structure, manage_pcg
```

- Registration is generated (`Tools/McpGeneratedParentRegistry.cpp`); `Registry/McpToolRegistry.cpp` accepts only canonical names and ignores duplicates.
- Adding a canonical registrar entry alone cannot expose a new parent tool. Update the canonical gate deliberately, keep TS/native parity, and justify context growth.
- `tools/list` filters accepted registry entries by dynamic enabled state; `tools/call` enforces the same state before dispatch.

## TOOL DEFINITIONS
- `FMcpToolDefinition` is pure data (name, description, category); every tool dispatches on its own name and the handler reads the concrete action from `action`.
- Execute mirrors `action` into `subAction` for handlers that still read the older field. Do not spread additional alias normalization.
- `manage_tools` is intercepted locally and returns a one-shot response; other tool calls queue through `UMcpAutomationBridgeSubsystem` and complete over SSE.

## DYNAMIC TOOLS
- Startup enables all accepted tools when `bLoadAllToolsOnStart` is true; otherwise it enables the `core` category.
- `manage_tools` and `inspect` are protected tools. The `core` category cannot be disabled.
- `DynamicTools` owns the internal tool visibility state (enabled tools/categories) consumed by `unreal.execute`. The public `tools/list` is permanently a single static `unreal` tool, so visibility never changes its shape: `OnToolsListChanged()` returns early and `notifications/tools/list_changed` is suppressed. Preserve locking around tool/category state and cached registry schemas. The `bEnableNativeGateway` setting and the legacy 23-tool direct listing were removed in the Task 30 cutover.

## TRANSPORT LIFECYCLE
- `POST /mcp` handles JSON-RPC; `GET /mcp` opens the persistent notification SSE stream; `DELETE /mcp` terminates a session and its streams.
- `initialize` must carry an id and returns `Mcp-Session-Id`. All later requests and notification streams require a valid session header.
- Client notifications receive HTTP 202 after validation. `tools/call` owns its socket until the streamed result completes.
- Return JSON-RPC errors through `McpJsonRpc` and tool outcomes through MCP `content[]` plus `isError`; never leak raw handler JSON as the top-level response.
- Do not block socket threads on Unreal work. Shutdown intentionally pumps game-thread tasks while draining active connections and async writes.
- **Long calls answer before the client gives up** (native only; the TS stdio server has no twin yet). The keepalive thread (`McpNativeTransportKeepalive.cpp`, never the game thread) pings every open `tools/call` after 8 s of silence with `McpAutomationBridge::DescribeEditorWork` (the handler in flight, its running time and last progress, the shader queue) and at 27 s writes a success receipt whose `task.state` is `running` or `queued`; the work goes on, and `bAnsweredRunning` keeps the entry until its completion settles the idempotency slot. Nothing on that thread may take `AutomationRequestExecutionMutex` (a running handler holds it): no `CancelAutomationRequest` there. A handler that reports progress (`SendProgressUpdate`) is what makes these replies say how far it has got.

## SECURITY
- Empty/`localhost` listen hosts normalize to loopback. A disallowed non-loopback host falls back to `127.0.0.1`.
- **Fail-closed LAN coupling**: the native transport refuses to bind non-loopback unless `bRequireCapabilityToken` is also enabled (`SECURITY: refusing to bind native MCP to non-loopback` in `Transport/McpNativeTransportLifecycle.cpp`). A LAN-exposed surface can never start without auth.
- When capability auth is enabled, require `X-MCP-Capability-Token` before method dispatch.
- **Constant-time token checks**: `McpConstantTimeTokenEquals` (`Private/Foundation/McpSecureTokenCompare.h`) compares the token with no data-dependent early exit, so timing never leaks how much of a token matched.
- **Session-scoped bounded cancellation**: `notifications/cancelled` correlates only to the caller's in-flight request, keyed by the client JSON-RPC id and the owning session id, so one session cannot cancel another. The cancel-marker maps (`CancelledInternalRequestIds` + `CancelledMarkerOrder`) are capped by `MaxCancelledMarkers` with oldest-first eviction, and a late response for a cancelled request is suppressed (the SSE socket closes without a result). See `Transport/McpNativeTransportCancellation.cpp`.
- Browser Origin/CORS access is allowed only under capability-token protection; preserve origin rejection and preflight behavior.
- Keep request-size limits, session expiry, method/path checks, write serialization, and socket ownership accounting intact.

## PROTOCOL VERSION NEGOTIATION (intentional legacy asymmetry)
The native transport supports **exactly the three modern MCP versions**:
`2025-11-25` (latest), `2025-06-18`, and `2025-03-26` (see `McpSupportedProtocolVersions` in `Transport/McpNativeTransportPrivate.h`). At `initialize` it echoes the highest mutually supported version, or the latest for an unknown well-formed request. A post-initialize request that omits the `MCP-Protocol-Version` header is accepted; only a present-but-unsupported value is refused (HTTP 400).

- **The native surface deliberately does NOT implement the later `2026-07-28` release-candidate version.** That RC is fictional for this codebase and is explicitly excluded from `McpSupportedProtocolVersions`; the contract test asserts it never appears as a listed/implemented version.
- **Asymmetry with the TS SDK**: the TypeScript stdio server negotiates through the MCP SDK's `SUPPORTED_PROTOCOL_VERSIONS`, which also accepts two older legacy versions (`2024-11-05` and `2024-10-07`). The native `/mcp` transport is intentionally stricter (modern versions only), so a client pinned to a legacy version will negotiate with the TS surface but not the native surface.

## GATEWAY DISCOVERY
The native surface permanently exposes the single `unreal` tool and mirrors the TypeScript gateway's progressive discovery. `describe` drills down in three levels and never dumps a full `inputSchema`:
1. `describe { tool }` -> tool summary + paginated/filterable action list.
2. `describe { tool, action }` -> paginated/filterable parameter catalog (the **tool-union**, not action-specific).
3. `describe { tool, action, param }` -> exactly one parameter's full schema.

`perActionSchemas` is **always `false`**: parameters are the union catalog across all actions of the parent tool, and a parameter is passed only when relevant to the selected action. Invalid tool/action/param calls return closest-match `suggestions` and an executable `nextCall` payload (guided errors). Native describe is served from the generated parent registry (`McpGeneratedParentRegistry*`), the same capability records the TS gateway reads.

## BARE DESCRIBE (intentional shape asymmetry)
A bare `describe {}` returns a DIFFERENT drill-down shape per transport, sharing only the `scope: "catalog"` label:
- **Native** (`McpNativeGatewayDescribeOverview.cpp`): enumerates the 23 canonical parent tools (`tool`, `actionCount`, `nextCall{operation,tool}`), because the native surface has no domain layer to drill through.
- **TypeScript** (`gateway-describe-browse.ts`): enumerates capability discovery domains (`domain`, `capabilityCount`, `familyCount`, `nextCall{operation,domain}`), then families, then capabilities.
A client trained on one surface's `nextCall` chain will not find the same levels on the other. Do not "fix" one side to match the other without a deliberate cross-transport decision; the drill-down depth is a per-transport property.

## VALIDATION
```bash
npm run registry:check   # generated shards and parent registry match the records
npm run test:params
```

- There is no automated TS/native gateway parity gate: keep `Gateway/` and `Execute/` in step with `src/server/gateway/` by hand.
- Folded families mirror the TS door exactly: `McpNativeGatewayValidation.cpp` applies `McpApplyFoldedPins` before defaults and schema validation and `McpResolveDispatchAction` after them; `FindByParentAction` falls back to any legacy pair, so every former name still resolves. A consent grant may name the capability by its canonical id, an alias, or a folded `tool.action` pair (`FMcpCapabilityDemand::ConsentNames`).
