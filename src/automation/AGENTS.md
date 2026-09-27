# src/automation

Client-side TypeScript WebSocket transport between the MCP server and Unreal's bridge. Independent of the plugin's native HTTP/SSE `/mcp` transport.

## STRUCTURE
```
automation/
|-- bridge.ts                      # thin public facade and dependency wiring
|-- bridge-config.ts               # options/env resolution: one host + one port, TLS, limits
|-- bridge-client.ts               # WebSocket lifecycle, inbound frames (size + rate check before parse), send
|-- bridge-request-dispatcher.ts   # lazy connect, backpressure queue, outbound requests, cancelMcpRequest
|-- bridge-state.ts, bridge-status.ts  # diagnostic state and the status snapshot
|-- connection-manager.ts          # the one socket, WS-ping heartbeat, inbound rate limit
|-- connection-lifecycle.ts        # connect/disconnect transitions, reconnect policy, stop()
|-- handshake.ts                   # bridge_hello / bridge_ack (sends the capability token, redacts it in logs)
|-- message-handler.ts             # responses, events, progress correlation
|-- message-schema.ts              # Zod wire-message validation
|-- request-tracker.ts             # ids, timeouts, progress extensions
|-- request-correlation.ts         # MCP request <-> automation id (1:1)
|-- request-context.ts             # async-local MCP request id + AbortSignal
|-- request-cancellation-error.ts, natural-timeout-cancellation.ts
|-- capability-token-provider.ts   # resolves the token; never logs it
|-- log-redaction.ts               # redacts secrets from logs and diagnostics
|-- diagnostics-snapshot-reader.ts # read-only reader for plugin diagnostics snapshots
|-- types.ts, index.ts
```
21 implementation files plus 17 colocated `*.test.ts` files.

## DATA FLOW
1. `AutomationRequestDispatcher` lazily starts the client and waits for `connected`.
2. `HandshakeHandler` sends `bridge_hello`; only a validated `bridge_ack` registers the socket.
3. `RequestTracker` allocates the request id before `send()`. Gateway controls (`correlationId`, `consent`, `expectedRevisions`, `timeoutMs`) arrive as explicit send options and ride the envelope, never handler params.
4. Inbound frames are size-checked, rate-checked, parsed, schema-validated, then correlated by `MessageHandler`.
5. Completion, timeout, disconnect, cancel or `stop()` rejects work exactly once and clears timers.

## CONVENTIONS
- `bridge.ts` wires components; lifecycle details belong in the owning file.
- Every request keeps its own id; there is no read coalescing.
- Progress extensions stay bounded by stale-progress, extension-count and absolute-timeout guards.
- Capability tokens never reach logs or diagnostics; status exposes only whether one is required.

## ANTI-PATTERNS
- Treating WebSocket `open` as connected before the handshake completes.
- Sending untracked requests or bypassing dispatcher backpressure.
- Moving config normalization or frame parsing into callers.
