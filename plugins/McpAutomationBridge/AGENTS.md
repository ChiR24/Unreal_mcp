# MCP Automation Bridge Plugin

Editor-only UE subsystem receiving WebSocket requests from TS MCP server, executing automation on game thread.

## STRUCTURE

```
Source/McpAutomationBridge/
├── Public/
│   ├── McpAutomationBridgeSubsystem.h  # Main subsystem, handler declarations
│   ├── McpAutomationBridgeSettings.h   # Project settings (host/port/token)
│   └── McpConnectionManager.h          # WebSocket connection state
├── Private/
│   ├── McpAutomationBridgeSubsystem.cpp    # Initialize, tick, connection
│   ├── McpAutomationBridge_ProcessRequest.cpp  # Request dispatcher
│   ├── McpBridgeWebSocket.cpp              # Custom WebSocket implementation
│   ├── McpConnectionManager.cpp            # Reconnect logic, telemetry
│   ├── *Handlers.cpp                       # Domain-specific handlers (35+)
│   └── McpAutomationBridgeGlobals.cpp      # Shared helpers
└── McpAutomationBridge.Build.cs
```

## WHERE TO LOOK

| Task | File(s) | Notes |
|------|---------|-------|
| Add new handler | Create `*Handlers.cpp` + declare in `Subsystem.h` | Follow existing pattern |
| Register handler | `InitializeHandlers()` in `Subsystem.cpp` | Map action string to handler |
| Request routing | `_ProcessRequest.cpp` | Main dispatcher, uses `AutomationHandlers` TMap |
| Response format | `SendAutomationResponse()` / `SendAutomationError()` | JSON with `request_id`, `success`, `result` |
| WebSocket impl | `McpBridgeWebSocket.cpp` | No external deps, binary frame support |
| Settings | `McpAutomationBridgeSettings.h/cpp` | Host, port, capability token, reconnect |

## HANDLER FILES

- `_AssetWorkflowHandlers.cpp` - import, duplicate, rename, delete, materials
- `_AssetQueryHandlers.cpp` - list, search, dependencies
- `_BlueprintHandlers.cpp` - create, compile, defaults
- `_BlueprintGraphHandlers.cpp` - node creation, pin connections
- `_SCSHandlers.cpp` - SimpleConstructionScript, add_component
- `_ControlHandlers.cpp` - spawn, delete, transform actors
- `_EditorFunctionHandlers.cpp` - PIE, camera, console commands
- `_SequenceHandlers.cpp` / `_SequencerHandlers.cpp` - Level Sequence ops
- `_LightingHandlers.cpp` / `_EffectHandlers.cpp` - lights, Niagara, debug shapes
- `_LandscapeHandlers.cpp` / `_FoliageHandlers.cpp` - terrain, vegetation
- `_PropertyHandlers.cpp` - get/set FProperty, array/map/set ops
- `_AnimationHandlers.cpp` - AnimBP, montages, ragdoll
- `_WorldPartitionHandlers.cpp` - load cells, data layers

## CONVENTIONS

### Handler Signature
```cpp
bool HandleXxxAction(const FString& RequestId, const FString& Action,
                     const TSharedPtr<FJsonObject>& Payload,
                     TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
```

### Response Pattern
```cpp
// Success
SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Done"), ResultJson);
// Error  
SendAutomationError(RequestingSocket, RequestId, TEXT("Asset not found"), TEXT("NOT_FOUND"));
```

### JSON Payload
- Read fields: `Payload->GetStringField(TEXT("asset_path"))`
- Build result: `TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());`
- Nested: Use `Payload->GetObjectField()`, check `HasField()` first

### Game Thread Safety
- All handlers run on game thread (dispatcher ensures this)
- Long ops: consider `AsyncTask()` + callback
- Avoid `StaticFindObject` during GC/async loading (already guarded in dispatcher)

## ANTI-PATTERNS

- **Breaking payload contract** - Changing field names breaks TS side
- **Missing error codes** - Always provide structured error codes for TS
- **Forgetting UE version guards** - Use `#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 7`
- **Ignoring SubobjectData** - UE 5.7+ requires SubobjectDataInterface for SCS ops
- **Unguarded property access** - Always `HasField()` before `Get*Field()`
- **Blocking WebSocket thread** - Dispatch to game thread, never block

## NOTES

- **No Python** - All automation is native C++, `execute_editor_python` returns error
- **Capability token** - Optional auth via `MCP_AUTOMATION_CAPABILITY_TOKEN`
- **Handler mapping** - See `docs/handler-mapping.md` for TS tool -> C++ handler routes
- **UE 5.0-5.7** - Version guards needed for SCS, ControlRig, SubobjectData
