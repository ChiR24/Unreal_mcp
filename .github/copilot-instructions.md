# Unreal MCP — AI Agent Quick Guide (for AI coding agents)

This repo runs **two cooperating processes**: Node.js MCP server (`src/`) + native C++ UE Editor plugin (`Plugins/McpAutomationBridge/Source/`).

**All ops**: JSON payloads → `sendAutomationRequest(action, params)` (TS) → WS → `UMcpAutomationBridgeSubsystem::ProcessAutomationRequest()` (C++) → domain `*Handlers.cpp` (native UE subsystems). Typed via `FJsonObjectConverter::JsonObjectToUStruct()` + `FProperty`; no Python.

## Quickstart Checklist
- Enable UE Plugins: **MCP Automation Bridge**, **Editor Scripting Utilities**.
- Sync: `npm run automation:sync -- --project "X:/MyProject/Plugins"`.
- Verify: `npm run automation:verify -- --project "X:/MyProject/Plugins"`.
- Editor: Build/start (plugin WS client → Node 8090/8091).
- Server: `npm run dev` (on-demand `tryConnect()`).
- Offline: `UNREAL_MCP_MOCK_MODE=1`.
- WASM: `npm run build:wasm` (5-8x JSON perf).

## High-Value Files
**Node**:
- `src/index.ts`: MCP setup, `ensureConnectedOnDemand()`, consolidated tools.
- `src/unreal-bridge.ts`: Throttling/safety (`executeThrottledCommand()`).
- `src/automation-bridge.ts`: WS client, `sendAutomationRequest()`, handshake.
- `src/tools/consolidated-*.ts`: 12 tools dispatch (`handleConsolidatedToolCall()`).
- `src/tools/*.ts`: Domain impls (e.g., `blueprint.ts`).

**Plugin** (`Plugins/McpAutomationBridge/Source/McpAutomationBridge/`):
- `Private/McpAutomationBridgeSubsystem.cpp`: WS loop, `ProcessAutomationRequest()` dispatcher.
- `Private/McpBridgeWebSocket.cpp/h`: Custom WS client (reconnect/handshake).
- `Private/McpAutomationBridgeHelpers.h`: `ResolveClassByName()`, JSON→UStruct.
- `Private/*Handlers.cpp` (~18): `AssetWorkflowHandlers::CreateMaterial()`, `SCSHandlers::AddComponent()`.
- `Public/McpAutomationBridgeSubsystem.h`: UEditorSubsystem decls.

## Add New Tool Workflow
1. **C++**: `*Handlers.cpp` impl → register in `Subsystem.cpp::ProcessAutomationRequest()` switch → `FReply {Success=true, Data=Json}`.
2. **TS**: `src/tools/<domain>.ts` → `automationBridge.sendAutomationRequest(action, params)`.
3. **Consolidated**: Route in `consolidated-tool-handlers.ts`.
4. **Validate/Wrap**: `responseValidator.wrapResponse(tool, resp)`.
5. **Test**: `tests/test-<domain>.mjs` → `npm run test:<domain>` (Markdown cases).

**Example** (C++ handler):
```cpp
case "create_material":
    return AssetWorkflowHandlers::CreateMaterial(Payload, Reply);
```

**Example** (TS call):
```ts
const resp = await automationBridge.sendAutomationRequest('create_material', {name, path});
return responseValidator.wrapResponse('manage_asset', resp);
```

## Conventions
- **12 Consolidated Tools Only**: `manage_asset`, `control_actor`, etc. (`consolidated-tool-definitions.ts`).
- **On-Demand**: `bridge.tryConnect(3)` before ops; no polling.
- **Paths**: `/Content` → `/Game` (`normalize.ts`).
- **Vec/Rot**: `{x,y,z}`/`[x,y,z]` → `toVec3Tuple()`.
- **Throttling**: Queue/prioritize (100ms min; stats 300ms; `unreal-bridge.ts`).
- **Safety**: Block quit/crash/Python/`&&` (`unreal-bridge.ts`); C++ structured errors.
- **Typed C++**: JSON → `FProperty`/`UStruct` (`Helpers.h`); `ResolveClassByName(name/path)`.
- **Responses**: `{success, message?, error?, warnings?, data?}`.
- **WASM**: Auto (`initializeWASM()`); JSON/math perf.

## Commands (PowerShell)
- Plugin: `netstat -ano | findstr :8090`; clean `rm Plugins/McpAutomationBridge/{Binaries,Intermediate}`.
- Lint: `npm run lint`; C++: `npm run lint:cpp`.
- Build: `npm run build`; WASM: `npm run build:wasm`.
- Dev/Test: `npm run dev`; `npm run test:manage_asset` (or `test`).
- Sync/Verify: `npm run automation:sync/verify`.

## Debug/Health
- MCP Resources: `ue://health` (metrics), `ue://automation-bridge` (pending/handshake).
- Logs: `LOG_LEVEL=debug`; UE Output Log (`bridge_ack`/`automation_request`).
- Verify Plugin: `npm run automation:verify`.

Specify op (e.g., `scs_add_component`) for minimal PR/tests.

---

## Detailed Guide (Preserved)
Node: `src/index.ts/unreal-bridge/automation-bridge`. Tools: `src/tools/consolidated-*/tools/*`. Plugin: `Plugins/.../Private/*`.

**Tests**: `tests/run-unreal-tool-tests.mjs` (Markdown); `UNREAL_MCP_MOCK_MODE=1`.
