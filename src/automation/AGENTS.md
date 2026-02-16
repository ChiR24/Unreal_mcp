# src/automation

WebSocket bridge client and protocol handling (9 files).

## OVERVIEW
Manages connection to Unreal Engine and provides a promise-based API for automation requests.

## STRUCTURE
```
automation/
├── bridge.ts              # Main AutomationBridge class (36KB)
├── connection-manager.ts  # Reconnect logic, heartbeat
├── handshake.ts           # Protocol version negotiation
├── message-handler.ts     # JSON-RPC frame processing
├── message-schema.ts      # Message type definitions
├── request-tracker.ts     # Pending request tracking
├── types.ts               # TypeScript interfaces
└── index.ts               # Module exports
```

## WHERE TO LOOK
| Task | File | Notes |
|------|------|-------|
| Bridge Client | `bridge.ts` | Main AutomationBridge class |
| Connection | `connection-manager.ts` | Reconnect logic, heartbeat |
| Handshake | `handshake.ts` | Protocol version negotiation |
| Protocol Logic | `message-handler.ts` | JSON-RPC frame processing |

## CONVENTIONS
- **Request Tracking**: Every request must have a unique ID tracked in `request-tracker.ts`.
- **Handshake Protocol**: Must negotiate capabilities before sending commands.
- **Byte Checks**: WebSocket size limits must be checked in **BYTES**, not string length.
- **Retry Strategy**: Exponential backoff for connection failures.

## ANTI-PATTERNS
- **Raw WS Send**: Never use `ws.send()` directly; use the bridge's send queue.
- **Unhandled Timeouts**: Every request must have a configurable timeout.
- **Logging Secret Tokens**: Never log the `MCP_AUTOMATION_CAPABILITY_TOKEN`.
