# plugins/McpAutomationBridge

Native C++ Automation Bridge for Unreal Engine 5.0-5.7.

## OVERVIEW
Editor-only UE subsystem executing automation requests received via WebSocket.

## STRUCTURE
```
Source/McpAutomationBridge/
├── Public/
│   ├── McpAutomationBridgeSubsystem.h  # Main subsystem, handler declarations
│   └── McpAutomationBridgeSettings.h   # Host/Port/Token config
├── Private/
│   ├── McpAutomationBridgeSubsystem.cpp    # Initialize, tick, dispatch
│   ├── McpAutomationBridge_ProcessRequest.cpp  # Request routing
│   ├── *Handlers.cpp                       # Action implementations (35+)
│   └── McpAutomationBridgeHelpers.h        # Critical UE 5.7 safety helpers
└── McpAutomationBridge.Build.cs
```

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Add handler | `*Handlers.cpp` | Declare in `Subsystem.h`, register in `Subsystem.cpp` |
| Save Asset | `McpSafeAssetSave(Asset)` | Use helper in `McpAutomationBridgeHelpers.h` |
| Component creation | `SCS->CreateNode()` | Use proper SCS ownership for UE 5.7 |
| JSON Parsing | `FJsonObjectConverter` | UE standard for Struct ↔ JSON |

## CONVENTIONS
- **Game Thread Safety**: Handlers are dispatched to game thread automatically by subsystem.
- **UE 5.7+ SCS**: Component templates must be owned by `SCS_Node`, not the Blueprint.
- **Safe Saving**: NEVER use `UPackage::SavePackage()`. Use `McpSafeAssetSave`.
- **ANY_PACKAGE**: Deprecated. Use `nullptr` for path-based object lookups.

## ANTI-PATTERNS
- **Modal Dialogs**: Avoid `UEditorAssetLibrary::SaveAsset()` on new assets (crashes D3D12).
- **Hardcoded Paths**: Do not use absolute Windows paths in handlers.
- **Blocking Thread**: WebSocket frame processing must not block the game thread.
