# plugins/McpAutomationBridge

Native C++ Automation Bridge for Unreal Engine 5.0-5.7.

## OVERVIEW
Editor-only UE subsystem executing automation requests via WebSocket. Receives JSON from TS MCP server, dispatches to game thread handlers.

## STRUCTURE
```
McpAutomationBridge/
├── Source/McpAutomationBridge/
│   ├── Public/
│   │   ├── McpAutomationBridgeSubsystem.h   # Subsystem + handler declarations
│   │   ├── McpAutomationBridgeSettings.h    # Host/Port/Token config
│   │   └── McpAutomationBridgeHelpers.h     # CRITICAL: UE 5.7 safety helpers
│   └── Private/
│       ├── McpAutomationBridgeSubsystem.cpp     # Initialize, tick, dispatch
│       ├── McpAutomationBridge_ProcessRequest.cpp # Request routing
│       ├── *Handlers.cpp                         # 90+ action implementations
│       └── McpConnectionManager.cpp              # WebSocket server
├── Config/
│   └── DefaultMcpAutomationBridge.ini
└── McpAutomationBridge.Build.cs
```

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Add handler | `*Handlers.cpp` | Declare in `Subsystem.h`, register in `InitializeHandlers()` |
| Save asset | `McpAutomationBridgeHelpers.h` | Use `McpSafeAssetSave(Asset)` |
| Component creation | `*SCSHandlers.cpp` | Use `SCS->CreateNode()` for UE 5.7 |
| JSON parsing | Any handler | Use `FJsonObjectConverter` |
| Find actor | Any handler | Use `FindActorByLabelOrName(World, Name)` helper |
| Get world | Any handler | Use `GetActiveWorld()` helper |
| Iterate classes | Any handler | Use `GetDerivedClasses()` helper |

## CONVENTIONS
- **Game Thread Safety**: Handlers auto-dispatched to game thread by subsystem
- **UE 5.7+ SCS**: Component templates owned by `SCS_Node`, not Blueprint
- **Safe Saving**: NEVER use `UPackage::SavePackage()`. Use `McpSafeAssetSave`
- **ANY_PACKAGE**: Deprecated. Use `nullptr` for path-based lookups
- **Null Checks**: Always check `GEditor` and `SpawnActor` results

## ANTI-PATTERNS
- **Modal Dialogs**: Avoid `UEditorAssetLibrary::SaveAsset()` on new assets (D3D12 crash)
- **Hardcoded Paths**: No absolute Windows paths in handlers
- **Blocking Thread**: WebSocket processing must not block game thread
- **Raw UObject***: Use `TWeakObjectPtr` for stored references
- **CDO Iteration**: Filter CDOs when using `TObjectIterator`
- **TObjectIterator**: Unsafe in UE 5.7. Use `GetDerivedClasses()` helper.
- **FindActorByName**: Unsafe lookup. Use `FindActorByLabelOrName()`.
