# src/automation

TypeScript WebSocket automation bridge client. This subtree connects the stdio MCP server to the Unreal plugin WebSocket bridge; it is not the native plugin HTTP/SSE MCP transport.

## STRUCTURE
```
automation/
|-- bridge.ts                 # AutomationBridge facade and lifecycle
|-- connection-manager.ts     # socket connect/reconnect, host policy, TLS/token plumbing
|-- handshake.ts              # protocol/version/capability negotiation
|-- message-handler.ts        # inbound frame parsing and dispatch
|-- message-schema.ts         # message validation shapes
|-- request-tracker.ts        # pending requests, timeouts, response correlation
|-- types.ts                  # protocol interfaces
|-- *.test.ts                 # Vitest coverage for connection, handshake, tracker
`-- index.ts
```

## WHERE TO LOOK
| Task | File | Notes |
|------|------|-------|
| Send automation request | `bridge.ts` | Use the bridge queue/API, not raw socket sends |
| Change connection behavior | `connection-manager.ts` | Host/port/TLS/token/reconnect logic |
| Change protocol negotiation | `handshake.ts` | Keep capabilities in sync with plugin handshake |
| Change frame handling | `message-handler.ts`, `message-schema.ts` | Validate before resolving requests |
| Change timeout behavior | `request-tracker.ts` | Every request needs tracked timeout cleanup |

## CONVENTIONS
- Every outbound request has a unique ID tracked by `RequestTracker` until resolved, rejected, or timed out.
- Handshake must complete before automation commands are treated as connected.
- WebSocket payload limits are byte limits; do not replace them with string-length checks.
- Reconnect uses exponential backoff and emits connection/handshake events consumed by `src/unreal-bridge.ts`.
- Loopback is default. Non-loopback hosts require explicit `MCP_AUTOMATION_ALLOW_NON_LOOPBACK=true`.
- Capability tokens are secrets. Send them only through the configured handshake/header path and never log them.

## ENVIRONMENT
- `MCP_AUTOMATION_HOST`, `MCP_AUTOMATION_PORT`, and `MCP_AUTOMATION_WS_PORTS` steer connection targets.
- `MCP_AUTOMATION_CLIENT_MODE` flips client/server bridge behavior.
- `MCP_AUTOMATION_USE_TLS` enables `wss://` behavior.
- `MCP_CONNECTION_TIMEOUT_MS` and `MCP_REQUEST_TIMEOUT_MS` control wait bounds.
- `MOCK_UNREAL_CONNECTION=true` is a test/development escape hatch, not production behavior.

## ANTI-PATTERNS
- Calling `ws.send()` or mutating sockets directly outside connection/bridge internals.
- Leaving pending requests after disconnect or timeout.
- Logging capability tokens, full auth headers, or unbounded payloads.
- Treating a socket open as usable before handshake/capability negotiation succeeds.
