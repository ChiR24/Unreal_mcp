# MCP Protocol & Gateway Transport Contract

This document describes the protocol behavior of the native MCP transport, the
stdio proxy in front of it, and how the single `unreal` gateway tool is exposed.
It is the source of truth for the contract tests in `tests/unit/plugin/` and
`tests/unit/`.

## One gateway, two ways in

| Aspect | stdio proxy | Native MCP (`/mcp`) |
|--------|-------------|---------------------|
| Path | `node dist/cli.js` → HTTP → the plugin's `/mcp` | Plugin Streamable HTTP/SSE, no Node.js |
| Public surface | Whatever `/mcp` lists: the single `unreal` gateway tool | Permanent single `unreal` gateway tool (no opt-out) |
| Capability token | Read from `UE_PROJECT_PATH`, sent as `X-MCP-Capability-Token` | `X-MCP-Capability-Token` header |
| Requires | Node.js 20.19.0+, `unreal-engine-mcp-server` | UE 5.0–5.8, no Node.js |

The proxy (`src/proxy/native-proxy.ts`) answers `initialize` and `ping` itself
and relays every other message. While the editor is down a `tools/call` answers
`NOT_CONNECTED` (listings answer from its cache, or empty), and the first request
after the editor is back opens a new upstream session; an HTTP 404 (the editor
restarted and forgot the session) opens one and resends once. SSE notifications
reach the client as they stream.

The plugin binds loopback-first by default. Its `bAllowNonLoopback` project
setting governs **both** server-side listeners it owns — the WebSocket bridge
listen socket and the native MCP HTTP/SSE transport; when enabled, also enable
`bRequireCapabilityToken`.

## Permanent single-tool surface

The single `unreal` gateway tool is **permanent**. There is no opt-out: the
native `bEnableNativeGateway` project setting has been removed, and there is no
legacy 23-tool direct listing to restore. The 23 canonical parent tools stay
registered privately and are reachable only through `unreal.execute`.

`tools/list` always returns only `{ "unreal" }`. The proxy announces
`notifications/tools/list_changed` once, when the editor's real listing replaces
the placeholder or cached one it started with.

A `tools/call` whose name is not `unreal` is **not routed**. It returns a
bounded, copy-paste-executable `DIRECT_TOOL_CALL_REMOVED` receipt instead of
executing. The receipt carries a `nextCall` that drills exactly one level, never
a full schema dump:

- unknown tool name -> `{ "operation": "search" }`
- known tool, no action supplied -> `{ "operation": "describe", "tool": "<tool>" }`
- known tool with an action -> `{ "operation": "execute", "tool": "<tool>",
  "action": "<action>", "params": { ... } }`

Re-running the `nextCall` through the `unreal` tool completes the migration. The
receipt is built in `McpNativeGatewayDirectCallMigration.cpp` and sent from
`McpNativeTransportGateway.cpp`.

`initialize` returns the `instructions` text the native server reads from
`Resources/MCP/server-info.json` (plus the project's **Server Instructions**);
the proxy hands it on and adds one line about itself. It states the search ->
describe -> execute procedure a client should follow and how to recover from
each guided error.

## Progressive gateway discovery

Discovery is **never a full schema dump**; `describe` drills down one level per
call, and `perActionSchemas` is `false` only on the `{ tool }` summary:

1. `describe {}` -> the domains; `{ domain }` -> its families;
   `{ domain?, family }` -> its capabilities.
2. `describe { tool }` -> tool summary + its action names (all of them unless
   `limit` is sent, no `inputSchema` body), plus a `browse` call: `search`
   filtered to that tool, whose rows carry per-action summaries.
3. `describe { capability }` or `{ tool, action }` -> the exact contract of that
   capability: only the parameters the action declares
   (`perActionSchemas: true`), never a parent-tool union.
4. `describe { ..., param }` -> exactly one parameter's full schema.

`search` returns capability rows (`capability`, `parentTool`, `action`,
`domain`, `family`, `summary`, `effect`, `availability`, `policy`, `outputs`,
`runnable` and a `describe` `nextCall`) without `inputSchema` bodies. Every
invalid `search` or `describe` call returns a structured, **guided error**:
closest-match `suggestions` plus an executable `nextCall` payload. Discovery is
served from the generated capability store
(`Private/MCP/Gateway/McpNativeGatewayCapabilityStore.cpp` over the
`Private/MCP/Generated/` shards, generated from the capability records).

## Protocol version negotiation

The native transport implements full negotiation. Supported versions, in
`Private/MCP/Transport/McpNativeTransportPrivate.h` (`McpSupportedProtocolVersions`):

- `2025-11-25` (latest)
- `2025-06-18`
- `2025-03-26`

Behavior:

- At `initialize`, the server picks the highest version mutually supported, or
  the latest for an unknown well-formed request. The proxy answers `initialize`
  itself with the same three versions and the same rule, so a client pinned to
  a legacy version (`2024-11-05`, `2024-10-07`) is answered with `2025-11-25`.
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

### `server/discover` (2026-07-28 discovery)

The native transport answers `server/discover` without a session, handshake or
`MCP-Protocol-Version` header (the capability-token check still applies):
`resultType: "complete"`, `supportedVersions: ["2026-07-28"]` (the version a request
names in its `_meta`; `initialize` negotiates the older ones), the same `capabilities`
and `instructions` as `initialize`, the server identity under
`_meta["io.modelcontextprotocol/serverInfo"]`, and the cache hints `ttlMs: 3600000`
and `cacheScope: "public"`. `capabilities.extensions` also lists
`io.modelcontextprotocol/tasks`. Through the proxy, a `server/discover` sent while the
editor is down fails, and a client that probes with it falls back to `initialize`.

### 2026-07-28 requests

The native transport serves a request whose
`params._meta["io.modelcontextprotocol/protocolVersion"]` is `2026-07-28` with no
`initialize` and no session; its client capabilities ride in the same `_meta`. The
proxy sends such a request on its own, outside its session, with the headers below
filled from the body. The native transport:

- requires `MCP-Protocol-Version`, `Mcp-Method` and (for `tools/call`,
  `resources/read`, `prompts/get`, `tasks/*`) `Mcp-Name` headers that match the body
  (HTTP 400, `-32020`; a `=?base64?…?=` name is decoded first), and answers an
  unsupported version with HTTP 400, `-32022` and the supported list;
- stamps `resultType` on every result and `ttlMs`/`cacheScope` on listings and reads;
- answers HTTP 404 `-32601` for `ping`, `logging/setLevel`, `initialize` and any
  unknown method;
- asks for consent through the client: a call that needs a grant and has none answers
  `resultType: "input_required"` with a yes/no form elicitation and a signed
  `requestState` when the client declared `elicitation`; the retry with the user's yes
  runs once;
- with the `io.modelcontextprotocol/tasks` extension declared, a call still open near
  the client's time limit answers a task handle; `tasks/get`, `tasks/cancel` and
  `tasks/update` follow the extension.

Older clients keep `initialize`, sessions and describe's `consentGrant` unchanged.
`2026-07-28` is not an `initialize` version, so `McpSupportedProtocolVersions` still
ends the negotiable set at `2025-11-25`.

## Cancellation

`notifications/cancelled` is implemented on the native `/mcp` transport, and the
proxy relays it on its session. It maps to the queued operation and, once
cancelled, a late response for that operation is suppressed (the SSE socket
closes without a result). This is **advisory for already-dispatched work**: a
queued request is dropped before it runs, but an in-flight (already-executing)
editor operation cannot be interrupted — it runs to completion, and only its
late response is suppressed. The behavior is **session-scoped and bounded**:

- Cancellation correlates only to the caller's in-flight request, keyed by the
  client JSON-RPC id and the owning session id, so one session cannot cancel
  another.
- The cancel-marker maps are capped (`MaxCancelledMarkers`) with oldest-first
  eviction, so they cannot grow without limit.
- A client-supplied `_meta.progressToken` is captured and echoed verbatim
  (type-preserving) in `notifications/progress`.

2026-07-28 has no client notifications over HTTP (closing a call's stream is what
cancels it), so the proxy does not relay a cancel for a modern call; that call runs
to the end.

Not covered by an automated test; verified against a live editor.

## Progress tokens

A client-supplied `_meta.progressToken` is preserved and echoed verbatim
(type-preserving) in the corresponding notification. The server does not
invent or rewrite progress tokens.

## Task support

`execution.taskSupport` is not advertised as required or optional until task
support is implemented. Clients must not assume task support is present.

## Gateway manifest generation

The neutral gateway manifest is generated from
`src/tools/catalog/capabilities/generated/parent-tool-definitions.generated.ts` into:

- `src/gateway/gateway-manifest.generated.json` (read by the parameter audit, `tests/parameter-audit-context.mjs`)

Run:

```bash
node --loader ts-node/esm scripts/generate-gateway-manifest.ts          # regenerate
node --loader ts-node/esm scripts/generate-gateway-manifest.ts --check  # CI gate: fail on drift
```

The `manifest:check` npm script wraps the `--check` form. Never hand-edit the
generated artifacts.

## Version sources

The canonical version is `package.json` (`version`); `npm version` rewrites it
together with `package-lock.json`. `tests/unit/version-consistency.test.ts`
asserts these sources agree:

- `package.json` (`version`) and `package-lock.json` (`version`)
- `server.json` (`version` and the npm package `version`)
- `plugins/EngineWire/EngineWire.uplugin` (`VersionName`)

The proxy reports its version from `package.json` at runtime and the native
server from the `.uplugin` `VersionName`, so neither carries a copy.

> **Removed:** the experimental `UnrealAgent` in-editor OpenCode ACP panel has been removed. External consumers that previously drove OpenCode over ACP through the editor panel must now target the native `/mcp` `unreal` gateway endpoint (or the stdio proxy in front of it) instead.
