# MCP Protocol & Gateway Transport Contract

This document describes the protocol behavior shared by the TypeScript stdio
transport and the native MCP transport, and how the single `unreal` gateway
tool is exposed on each. It is the source of truth for the contract tests in
`tests/unit/plugin/` and `tests/unit/`.

## Two transports, one gateway contract

| Aspect | TypeScript stdio | Native MCP (`/mcp`) |
|--------|-----------------|---------------------|
| Path | `node dist/cli.js` → WebSocket bridge → C++ subsystem | Plugin Streamable HTTP/SSE, no Node.js |
| Public surface | Permanent single `unreal` gateway tool (no opt-out) | Permanent single `unreal` gateway tool (no opt-out) |
| Capability token | `bridge_hello.capabilityToken` | `X-MCP-Capability-Token` header |
| Requires | Node.js 20.19.0+, `unreal-engine-mcp-server` | UE 5.0–5.8, no Node.js |

Both surfaces bind loopback-first by default. The plugin's `bAllowNonLoopback`
project setting governs **both** server-side listeners it owns — the WebSocket
bridge listen socket and the native MCP HTTP/SSE transport; when enabled, also
enable `bRequireCapabilityToken`. The TypeScript stdio bridge is a WebSocket
*client* to that plugin socket, not a second server, so its analogous opt-in is
the `MCP_AUTOMATION_ALLOW_NON_LOOPBACK` environment variable (paired with
`MCP_AUTOMATION_HOST=0.0.0.0`) on the Node.js process. The two flags are
independent: flipping one does not change the other surface's binding posture.

## Permanent single-tool surface

The single `unreal` gateway tool is **permanent on both transports**. There is
no opt-out: the TypeScript `MCP_GATEWAY_MODE` env var and the native
`bEnableNativeGateway` project setting have both been removed, and there is no
legacy 23-tool direct listing to restore. The 23 canonical parent tools stay
registered privately and are reachable only through `unreal.execute`.
(`MCP_AUTOMATION_CLIENT_MODE` is unrelated: it selects WebSocket client vs
server topology for the TypeScript bridge, not the public tool surface.)

`tools/list` always returns only `{ "unreal" }`, and the listing never changes
shape, so `notifications/tools/list_changed` is suppressed on both surfaces.

A `tools/call` whose name is not `unreal` is **not routed**. It returns a
bounded, copy-paste-executable `DIRECT_TOOL_CALL_REMOVED` receipt instead of
executing. The receipt carries a `nextCall` that drills exactly one level, never
a full schema dump:

- unknown tool name -> `{ "operation": "search" }`
- known tool, no action supplied -> `{ "operation": "describe", "tool": "<tool>" }`
- known tool with an action -> `{ "operation": "execute", "tool": "<tool>",
  "action": "<action>", "params": { ... } }`

Re-running the `nextCall` through the `unreal` tool completes the migration. The
TypeScript receipt is built in `src/server/gateway/direct-call-migration.ts`; the
native transport emits the same `DIRECT_TOOL_CALL_REMOVED` shape from
`McpNativeTransportGateway.cpp`.

## Progressive gateway discovery

Both transports expose the same progressive `unreal` gateway. Discovery is
**never a full schema dump**; `describe` drills down in three levels, and
`perActionSchemas` is **always `false`**:

1. `describe { tool }` -> tool summary + a paginated/filterable action list
   (no `inputSchema` body).
2. `describe { tool, action }` -> a paginated/filterable parameter catalog for
   the action. The catalog is the **tool-union**: parameters are shared across
   all actions of the parent tool, not action-specific, so per-action schema
   mappings do not exist.
3. `describe { tool, action, param }` -> exactly one parameter's full schema.

`search` returns compact matches (name, category, description, actions) without
`inputSchema` or `parameterNames` bodies. Every invalid `describe` call
(tool/action/param) returns a structured, **guided error**: closest-match
`suggestions` plus an executable `nextCall` payload that drills one level
deeper. The generated native manifest (`McpNativeGatewayManifest.h`) is the
single source of truth shared with the TS gateway; the native
`McpNativeGatewayDescribe.cpp` is only a registry fallback when that manifest
fails to load.

## Protocol version negotiation

The native transport implements full negotiation. Supported versions, in
`Private/MCP/Transport/McpNativeTransportPrivate.h` (`McpSupportedProtocolVersions`):

- `2025-11-25` (latest)
- `2025-06-18`
- `2025-03-26`

Behavior:

- At `initialize`, the server picks the highest version mutually supported, or
  the latest for an unknown well-formed request.
- `McpLatestProtocolVersion()` returns `2025-11-25`.
- `McpDefaultProtocolVersion()` returns `2025-03-26`, used when a
  post-initialize request omits `MCP-Protocol-Version` and no session version
  is known.
- `FMcpNativeTransport::GuardProtocolVersionHeader()` validates the
  `MCP-Protocol-Version` header on every post-initialize request. A request
  that **omits** the header is **NOT rejected** — it resolves to the negotiated
  session version if known, otherwise `McpDefaultProtocolVersion()`
  (`2025-03-26`); **no HTTP 400 is returned for an absent header**. Only a
  **present but unsupported** (or malformed) header returns HTTP 400; the
  response is sent through `SendAndClose(ClientSocket, 400, ...)`.

### Intentional native/TS asymmetry

The native `/mcp` transport is **intentionally stricter than the TypeScript
surface**. It supports only the three modern versions listed above. The
TypeScript stdio server negotiates through the MCP SDK's
`SUPPORTED_PROTOCOL_VERSIONS`, which also accepts two older legacy versions:

- `2024-11-05`
- `2024-10-07`

A client pinned to a legacy version therefore negotiates with the TS surface
but not the native surface. The native transport **deliberately does not
implement the later `2026-07-28` release-candidate version**; that RC is not
part of this codebase and is excluded from `McpSupportedProtocolVersions`.
State "latest" as `2025-11-25` only; do not claim support for any later RC.

## Cancellation

### Native MCP transport (implemented)

`notifications/cancelled` is implemented on the native `/mcp` transport. It
maps to the queued operation and, once cancelled, a late response for that
operation is suppressed (the SSE socket closes without a result). This is
**advisory for already-dispatched work**: a queued request is dropped before it
runs, but an in-flight (already-executing) editor operation cannot be
interrupted — it runs to completion, and only its late response is suppressed.
The behavior is **session-scoped and bounded**:

- Cancellation correlates only to the caller's in-flight request, keyed by the
  client JSON-RPC id and the owning session id, so one session cannot cancel
  another.
- The cancel-marker maps are capped (`MaxCancelledMarkers`) with oldest-first
  eviction, so they cannot grow without limit.
- A client-supplied `_meta.progressToken` is captured and echoed verbatim
  (type-preserving) in `notifications/progress`.

Covered by `tests/unit/plugin/native_cancellation_contracts.test.ts`.

### TypeScript stdio transport (implemented)

Forwarding of inbound `notifications/cancelled` from the TS stdio server to the
automation bridge **is implemented**. `server-factory.ts` registers
`CancelledNotificationSchema` and calls `AutomationBridge.cancelMcpRequest`,
which rejects the matching queued or inflight automation request (keyed by the
canonicalized JSON-RPC id) and sends a targeted `cancel_request` frame to
Unreal. Both the explicit notification and the SDK `AbortSignal` converge on the
same idempotent primitive, and a late `automation_response` for a cancelled
request is harmless because the tracker entry was already removed. Gateway and
legacy tool modes both capture `extra.requestId` / `extra.signal` via an
async-local request context so handlers can be cancelled without changing their
signatures.

Covered by `tests/unit/plugin/bridge_cancellation_contracts.test.ts` and the
dispatcher/request-context unit tests.

## Progress tokens

A client-supplied `_meta.progressToken` is preserved and echoed verbatim
(type-preserving) in the corresponding notification. The server does not
invent or rewrite progress tokens.

## Task support

`execution.taskSupport` is not advertised as required or optional until task
support is implemented. Clients must not assume task support is present.

## Gateway manifest generation

The neutral gateway manifest is generated from
`src/tools/catalog/consolidated-tool-definitions.ts` into:

- `src/gateway/gateway-manifest.generated.ts` (compiled into `dist/`)
- `src/gateway/gateway-manifest.generated.json` (neutral asset, parity source)
- `plugins/.../MCP/Gateway/McpNativeGatewayManifest.h` (embedded JSON)

Run:

```bash
node --loader ts-node/esm scripts/generate-gateway-manifest.ts          # regenerate
node --loader ts-node/esm scripts/generate-gateway-manifest.ts --check  # CI gate: fail on drift
```

The `manifest:check` npm script wraps the `--check` form. Never hand-edit the
generated artifacts.

## Version sources

All version sources must agree (currently `0.5.30`). The canonical source is
`package.json` (`version`); `npm version` rewrites it together with
`package-lock.json`, so both stay in lockstep. Every other coordinated
source is compared against `package.json` by `npm run version:check`
(`tests/unit/version-consistency.test.ts`):

- `package.json` (`version`) and `package-lock.json` (`version`, rewritten by `npm version`)
- `server.json` (`version` and the npm package `version`)
- `plugins/McpAutomationBridge/McpAutomationBridge.uplugin` (`VersionName`)
- `plugins/McpAutomationBridge/Resources/MCP/server-info.json` (`version`)
- `src/server/server-factory.ts` (`SERVER_VERSION` fallback when
  `package.json` cannot be read)
- `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Transport/McpNativeTransport.h`
  (`ServerVersion` `TEXT` fallback)

`npm run version:check` asserts agreement across all seven sources.

> **Removed:** the experimental `UnrealAgent` in-editor OpenCode ACP panel has been removed. External consumers that previously drove OpenCode over ACP through the editor panel must now target the native `/mcp` `unreal` gateway endpoint (or the TypeScript stdio `unreal` gateway tool) instead.
