# TRANSPORT: WEBSOCKET BRIDGE

WebSocket automation bridge only. The native `/mcp` HTTP/SSE transport is a SEPARATE lifecycle under `../MCP/Transport/` (see `../MCP/AGENTS.md`). Do not document or edit it here; do not route around either boundary.

Two subdirs, 21 files. `WebSocket/` owns sockets, framing, TLS, handshakes. `Connection/` owns the connection manager: auth, per-socket rate limits, request/socket correlation, cancellation, telemetry.

## STRUCTURE

WebSocket/
- `McpBridgeWebSocket.cpp` / `.h` / `Private.h` — socket core, recv/send loop, listen socket lifecycle.
- `McpBridgeWebSocketServer.cpp` — listen bind + **loopback gate** (fail-closed).
- `McpBridgeWebSocketServerHandshake.cpp` — HTTP upgrade, **origin rejection (close 4403)**, subprotocol, frame-size limits.
- `McpBridgeWebSocketServerClients.cpp` — connected client registry.
- `McpBridgeWebSocketClient.cpp` / `McpBridgeWebSocketClientHandshake.cpp` — client mode (TS bridge acts as client).
- `McpBridgeWebSocketFrameReceive.cpp` / `FrameSend.cpp` / `RawIO.cpp` / `Utilities.cpp` — frame encode/decode, raw socket IO, helpers.
- `McpBridgeWebSocketTls.cpp` — TLS establish; preserve cert/key validation.

Connection/
- `McpConnectionManager.cpp` / `.h` / `Private.h` — manager core, rate-limit map teardown.
- `McpConnectionManagerConnection.cpp` — connect/disconnect, socket auth state.
- `McpConnectionManagerMessages.cpp` — message routing, `bridge_hello` token, handshake gate, rate-limit calls.
- `McpConnectionManagerSocketEvents.cpp` — socket events; clears per-socket rate state on teardown.
- `McpConnectionManagerCancellation.cpp` — request cancellation (scoped, advisory).
- `McpConnectionManagerResponses.cpp` — response correlation back to the originating socket.
- `McpConnectionManagerTelemetry.cpp` — connection telemetry.

## CONNECTION LIFECYCLE

1. **Bind gate** — `McpBridgeWebSocketServer.cpp` computes `bIsLoopback` (127.0.0.1 / ::1; `localhost` normalizes to 127.0.0.1). Loopback binds. Non-loopback binds ONLY if `bAllowNonLoopback` AND `bRequireCapabilityToken` both set; else destroys the listen socket and returns 0. No implicit `0.0.0.0` fallback.
2. **Upgrade / origin** — `ServerHandshake` validates the HTTP upgrade. Any non-empty `Origin` header is rejected with close 4403 BEFORE `101 Switching Protocols`.
3. **bridge_hello** — client MUST send `bridge_hello` first. Missing `capabilityToken` when required => `INVALID_CAPABILITY_TOKEN`, close 4005. Socket joins `AuthenticatedSockets`.
4. **Token** — compared with `McpConstantTimeTokenEquals` (full UTF-8 span, XOR-accumulate, length folded in, no early exit).
5. **Request** — `automation_request` before auth => `HANDSHAKE_REQUIRED`, close 4004. RequestId/Action mapped to socket for routing.
6. **Response / heartbeat** — responses routed ONLY to the originating socket (never broadcast). Heartbeats cleaned up on disconnect.
7. **Teardown** — delegates unbound, per-socket rate state removed, pending requests cleared.

## SECURITY INVARIANTS (non-negotiable)

- **Loopback fail-closed.** Non-loopback bind requires BOTH `bAllowNonLoopback` and `bRequireCapabilityToken`. Single-flag opt-in is rejected. [machine-enforced: `tests/unit/plugin/security_contracts.test.ts`]
- **Constant-time token compare.** Only `McpConstantTimeTokenEquals` (`../Foundation/McpSecureTokenCompare.h`). Replacing with `==`/`Equals` fails the contract test. [machine-enforced: `tests/unit/plugin/security_contracts.test.ts`]
- **Origin rejection.** Non-empty `Origin` => close 4403 before upgrade. Never downgrade to allow browsers. [machine-enforced: `tests/unit/plugin/websocket_origin_contracts.test.ts`]
- **Per-socket response correlation.** No automation response may reach an unrelated socket. [machine-enforced: `tests/unit/plugin/security_contracts.test.ts`]
- **Rate limiting.** `UpdateRateLimit` under `RateLimitMutex`; per-socket state in `SocketRateLimits`, cleared on teardown. Exceeding returns `RATE_LIMIT_EXCEEDED` (close 4008).
- **Handshake before automation.** No request before `bridge_hello` auth.
- **Frame/handshake byte limits.** Enforce header (8192) and frame size caps.

## CONVENTIONS

- Defaults: host `127.0.0.1`, ports `8090,8091`, multi-listen on, non-loopback off.
- Keep editor API work OFF socket threads. Hand it to the Core game-thread queue (`../Core/AGENTS.md`).
- 250 pure-line ceiling + ≤25 files/folder apply.

## ANTI-PATTERNS

- Don't add an implicit `0.0.0.0` / non-loopback fallback when the token flag is off.
- Don't swap `McpConstantTimeTokenEquals` for `==`/`Equals` (timing oracle).
- Don't weaken or remove origin rejection.
- Don't broadcast responses; correlation is per-socket.
- Don't run editor work on socket threads; don't bypass the Core queue.
- Don't document native `/mcp` sessions/SSE here; that is `../MCP/AGENTS.md`.
