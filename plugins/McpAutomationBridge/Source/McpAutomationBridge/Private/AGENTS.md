# Private/ (C++ Handler Implementations)

90+ C++ handler files implementing UE automation actions.

## OVERVIEW
Each `*Handlers.cpp` file implements actions for one tool domain. Handlers are registered in `McpAutomationBridgeSubsystem::InitializeHandlers()`.

## STRUCTURE
```
Private/
├── McpAutomationBridgeSubsystem.cpp      # Subsystem lifecycle + handler registration
├── McpAutomationBridge_ProcessRequest.cpp # Tool → handler routing
├── McpAutomationBridgeHelpers.h           # CRITICAL safety helpers
├── McpConnectionManager.cpp               # WebSocket server
├── McpBridgeWebSocket.cpp                 # WS frame handling
│
├── *Handlers.cpp files by domain:
│   ├── _AIHandlers.cpp                    # AI controllers, EQS, perception
│   ├── _AnimationHandlers.cpp             # Animation BPs, montages, blendspaces
│   ├── _AssetWorkflowHandlers.cpp         # Asset CRUD operations
│   ├── _BlueprintHandlers.cpp             # BP creation + graph editing
│   ├── _CombatHandlers.cpp                # Weapons, projectiles, damage
│   ├── _EnvironmentHandlers.cpp           # Landscape, foliage
│   ├── _GASHandlers.cpp                   # Gameplay Ability System
│   ├── _LevelHandlers.cpp                 # Level load/save, streaming
│   ├── _LightingHandlers.cpp              # Light spawning, GI, shadows
│   ├── _NiagaraHandlers.cpp               # Niagara VFX systems
│   ├── _PCGHandlers.cpp                   # Procedural Content Generation
│   ├── _SequencerHandlers.cpp             # Sequencer + MRQ
│   ├── _VirtualProductionHandlers.cpp     # nDisplay, ICVFX
│   ├── _WaterHandlers.cpp                 # Water bodies (ocean, lake, river)
│   ├── _WeatherHandlers.cpp               # Sky, fog, volumetric clouds
│   ├── _XRPluginsHandlers.cpp             # OpenXR, Quest, SteamVR, ARKit
│   └── ... (70+ more)
```

## HANDLER PATTERN
```cpp
// In *Handlers.cpp
TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleMyAction(
    const TSharedPtr<FJsonObject>& Params)
{
    // 1. Extract params
    FString Path = Params->GetStringField(TEXT("path"));
    
    // 2. Validate
    if (Path.IsEmpty()) {
        return MakeErrorResponse(TEXT("path is required"));
    }
    
    // 3. Execute UE API
    UObject* Asset = LoadObject<UObject>(nullptr, *Path);
    
    // 4. Return result
    return MakeSuccessResponse(TEXT("Asset loaded"));
}

// In InitializeHandlers():
Handlers.Add(TEXT("my_action"), 
    FMcpHandler::CreateUObject(this, &ThisClass::HandleMyAction));
```

## CONVENTIONS
- **Helper Usage**: Always use `McpSafeAssetSave()`, `GetActiveWorld()`, `FindActorByLabelOrName()`
- **Error Responses**: Use `MakeErrorResponse()` with descriptive messages
- **Null Safety**: Check all pointers before use, especially after `SpawnActor()`
- **Conditional Compilation**: Use `#if WITH_EDITOR` for editor-only code

## ANTI-PATTERNS
- **UPackage::SavePackage()**: Crashes in UE 5.7. Use `McpSafeAssetSave()`
- **ANY_PACKAGE**: Deprecated. Use `nullptr`
- **GEditor Direct Access**: Check null, use `GetActiveWorld()` helper
- **Raw Pointers**: Use `TWeakObjectPtr` for stored actor references
- **Stubs**: No "not implemented" returns. Full implementation required
