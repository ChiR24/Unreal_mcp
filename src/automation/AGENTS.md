# src/automation

WebSocket bridge client and protocol handling for UE communication.

## OVERVIEW
Manages bidirectional WebSocket connection to Unreal Engine's MCP Automation Bridge plugin. Promise-based API with automatic reconnection. Default ports: 8090, 8091.

## STRUCTURE
```
automation/
├── bridge.ts             # Main AutomationBridge class
├── connection-manager.ts # Reconnect logic, heartbeat, port scanning
├── handshake.ts          # Protocol version negotiation
├── message-handler.ts    # JSON-RPC frame processing
├── request-tracker.ts    # Request ID tracking + timeout management
├── types.ts              # Protocol types
└── index.ts              # Public exports
```

## WHERE TO LOOK
| Task | File | Notes |
|------|------|-------|
| Bridge API | `bridge.ts` | `sendAutomationRequest()`, `tryConnect()` |
| Connection | `connection-manager.ts` | Port scanning, reconnect logic |
| Handshake | `handshake.ts` | Capability negotiation |
| Protocol | `message-handler.ts` | JSON-RPC parsing |

## CONVENTIONS
- **Request Tracking**: Every request gets unique ID via `request-tracker.ts`
- **Handshake First**: Must negotiate capabilities before sending commands
- **Byte Checks**: WebSocket size limits in BYTES, not string length
- **Exponential Backoff**: Connection retries with increasing delays

## ANTI-PATTERNS
- **Raw WS Send**: Never use `ws.send()` directly; use `sendAutomationRequest()`
- **Unhandled Timeouts**: Every request must have configurable timeout
- **Logging Tokens**: Never log `MCP_AUTOMATION_CAPABILITY_TOKEN`
- **Blocking Connection**: Connection is async; don't block on connect
