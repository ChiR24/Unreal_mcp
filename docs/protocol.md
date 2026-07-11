# MCP Protocol & Gateway Transport Contract

This document describes the protocol behavior shared by the TypeScript stdio
transport and the native MCP transport, and how the single `unreal` gateway
tool is exposed on each. It is the source of truth for the contract tests in
`tests/unit/plugin/` and `tests/unit/`.

## Two transports, one gateway contract

| Aspect | TypeScript stdio | Native MCP (`/mcp`) |
|--------|-----------------|---------------------|
| Path | `node dist/cli.js` → WebSocket bridge → C++ subsystem | Plugin Streamable HTTP/SSE, no Node.js |
| Gateway control | `MCP_GATEWAY_MODE` env (set at server start) | `bEnableNativeGateway` project setting (needs editor restart) |
| Capability token | `bridge_hello.capabilityToken` | `X-MCP-Capability-Token` header |
| Requires | Node.js 18+, `unreal-engine-mcp-server` | UE 5.0–5.8, no Node.js |

Both surfaces bind loopback-first by default. The plugin's `bAllowNonLoopback`
project setting governs **both** server-side listeners it owns — the WebSocket
bridge listen socket and the native MCP HTTP/SSE transport; when enabled, also
enable `bRequireCapabilityToken`. The TypeScript stdio bridge is a WebSocket
*client* to that plugin socket, not a second server, so its analogous opt-in is
the `MCP_AUTOMATION_ALLOW_NON_LOOPBACK` environment variable (paired with
`MCP_AUTOMATION_HOST=0.0.0.0`) on the Node.js process. The two flags are
independent: flipping one does not change the other surface's binding posture.

## Gateway mode (default on)

Gateway mode is **on by default** on both transports:

- TypeScript: `isGatewayMode()` returns `true` when `MCP_GATEWAY_MODE` is
  undefined or empty. Set it to `false`, `0`, or `no` to restore the 23-tool
  direct-listing (legacy) mode.
- Native: `bEnableNativeGateway` defaults to `true`. Disabling it restores the
  23-tool listing on the native surface.

When the gateway is on, `tools/list` returns only `{ "unreal" }`. A
`tools/call` whose name is not `unreal` returns an `UNKNOWN_TOOL` error that
directs the client to `search` / `describe` / `execute`.

## Protocol version negotiation (2025-11-25)

The native transport implements full negotiation. Supported versions, in
`Private/MCP/Transport/McpNativeTransportPrivate.h`:

- `2025-11-25` (latest)
- `2025-06-18`
- `2025-03-26`

Behavior:

- At `initialize`, the server picks the highest version mutually supported.
- `McpLatestProtocolVersion()` returns `2025-11-25`.
- `McpDefaultProtocolVersion()` returns `2025-03-26`, used when a
  post-initialize request omits `MCP-Protocol-Version` and no session version
  is known.
- `FMcpNativeTransport::GuardProtocolVersionHeader()` validates the
  `MCP-Protocol-Version` header on every post-initialize request and returns
  HTTP 400 on an unsupported or invalid value.

## Cancellation

`notifications/cancelled` maps to the queued operation; once cancelled, a late
response for that operation is suppressed. Covered by
`tests/unit/plugin/native_cancellation_contracts.test.ts`.

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

All version sources must agree (currently `0.5.30`):

- `package.json` (`version`)
- `server.json` (`version` and the npm package `version`)
- `plugins/McpAutomationBridge/McpAutomationBridge.uplugin` (`VersionName`)
- `src/server/server-factory.ts` (`SERVER_VERSION` fallback when
  `package.json` cannot be read)

`npm run version:check` asserts agreement via
`tests/unit/version-consistency.test.ts`.
