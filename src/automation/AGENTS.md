# src/automation

Client-side TypeScript WebSocket transport between the MCP server and Unreal's bridge. This is independent of the plugin's native HTTP/SSE `/mcp` transport.

## STRUCTURE
```
automation/
|-- bridge.ts                    # thin public facade and dependency wiring
|-- bridge-config.ts             # options/env resolution, host policy, URL formatting
|-- bridge-client.ts             # WebSocket lifecycle, inbound frames, send/broadcast
|-- bridge-request-dispatcher.ts # lazy connect, backpressure queue, outbound requests
|-- bridge-state.ts              # mutable diagnostic timestamps and errors
|-- bridge-status.ts             # public status snapshot assembly
|-- bridge-frame.ts              # byte length and UTF-8 conversion
|-- connection-manager.ts        # active sockets, primary socket, heartbeat, rate limits
|-- connection-lifecycle.ts      # connect/disconnect transitions, reconnect policy, stop()
|-- handshake.ts                 # bridge_hello/bridge_ack negotiation
|-- message-handler.ts           # responses, events, progress, action correlation
|-- message-schema.ts            # Zod wire-message validation
|-- request-tracker.ts           # IDs, timeouts, progress extensions, coalescing
|-- request-correlation.ts       # MCP-request <-> automation-id correlation for progress fan-out
|-- request-context.ts           # async-local MCP request id + AbortSignal (cancellation)
|-- request-cancellation-error.ts # typed error for cancelled/dropped requests
|-- gateway-consent-context.ts   # async-local gateway consent envelope (never a handler param)
|-- gateway-correlation-context.ts # async-local gateway correlation id envelope
|-- gateway-expected-revisions-context.ts # async-local expected-revisions envelope
|-- gateway-timeout-context.ts   # async-local gateway timeout control
|-- capability-token-provider.ts # reads/verifies the plugin capability token, never logs it
|-- log-redaction.ts             # redacts tokens/secrets from logs and diagnostics
|-- diagnostics-snapshot-reader.ts # read-only TS reader for plugin diagnostics snapshots (current/previous)
|-- natural-timeout-cancellation.ts # best-effort advisory cancel_request frame delivery for natural timeouts
|-- types.ts                     # protocol, event, status, and queue contracts
`-- index.ts                     # public export surface
```
26 implementation files plus 16 colocated `*.test.ts` files.

## WHERE TO LOOK
| Task | File | Notes |
|------|------|-------|
| Add public bridge behavior | `bridge.ts` | Delegate to a focused component; keep facade small |
| Change connection options | `bridge-config.ts` | Normalize hosts, ports, protocols, limits, TLS |
| Change socket lifecycle | `bridge-client.ts` | Open/close/error handlers and validated inbound flow |
| Change lazy connect or queueing | `bridge-request-dispatcher.ts` | Owns connection promise and queued request draining |
| Change socket bookkeeping | `connection-manager.ts` | Primary selection, heartbeat, rate counters |
| Change handshake | `handshake.ts` | Connection is usable only after valid `bridge_ack` |
| Change response correlation | `message-handler.ts`, `request-tracker.ts` | Keep action checks and timer cleanup paired |
| Change MCP-request correlation | `request-correlation.ts`, `request-context.ts` | Async-local id + AbortSignal; canonicalize ids (`num:`/`str:` namespacing) |
| Change consent/correlation/timeout envelopes | `gateway-*-context.ts` | Async-local; ride the automation_request envelope, never handler params |
| Change token auth / redaction | `capability-token-provider.ts`, `log-redaction.ts` | Never log tokens; redact before diagnostics |
| Change diagnostics | `bridge-state.ts`, `bridge-status.ts` | Status should read state, not drive lifecycle |
| Change raw frame support | `bridge-frame.ts`, `message-schema.ts` | Enforce byte limits before protocol handling |

## DATA FLOW
1. `AutomationRequestDispatcher` lazily starts the client and waits for the `connected` event.
2. `HandshakeHandler` sends `bridge_hello`; only a validated `bridge_ack` registers the socket.
3. `RequestTracker` allocates the request ID before `AutomationBridgeClient.send()`.
4. Inbound data is byte-checked, parsed, rate-checked, schema-validated, then correlated by `MessageHandler`.
5. Completion, timeout, disconnect, or `stop()` must clear pending and queued work.

## CONVENTIONS
- `bridge.ts` wires components and exposes typed events; lifecycle details belong in the owning split file.
- `bridge-client.ts` owns application frames; `handshake.ts` owns hello/ack; `connection-manager.ts` owns heartbeat frames.
- Preserve byte-based payload limits for strings, buffers, buffer arrays, and array-buffer views.
- Read-only requests may coalesce; mutations must retain independent request IDs.
- Progress extensions remain bounded by stale-progress, extension-count, and absolute-timeout guards.
- Keep capability tokens out of logs and diagnostic metadata; status exposes only whether a token is required.
- On failed send, timeout, disconnect, or shutdown, reject work exactly once and clear all timers/listeners.

## VALIDATION
```bash
npm run type-check
npx vitest run src/automation tests/unit/automation/bridge_host_validation.test.ts
```

## ANTI-PATTERNS
- Adding transport logic back into the facade.
- Treating WebSocket `open` as connected before handshake completion.
- Sending untracked requests or bypassing dispatcher backpressure.
- Moving config normalization, auth headers, or raw frame parsing into callers.
