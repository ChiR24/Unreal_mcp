# Editor-Side Extension Concept

To lift several Remote Control limitations (SimpleConstructionScript authoring, typed property assignment, asset migrations, modal override hooks), we will eventually ship an optional Unreal Editor plugin that exposes a trusted message bus for the MCP server. This document tracks the scope and next steps for that plugin.

## Goals
- **Direct SCS Access** – expose Blueprint SimpleConstructionScript mutations through a curated C++ API that the MCP server can call.
- **Typed Property Marshaling** – relay incoming JSON payloads through Unreal’s `FProperty` system so class/enum/soft object references resolve without manual string coercion.
- **Asset Lifecycle Helpers** – wrap save/move/delete flows with redirector fix-up, source control hooks, and safety prompts suppressed via automation policies.
- **Modal Dialog Mediation** – surface blocking dialogs to the MCP server with explicit continue/cancel channels instead of stalling automation.

## Architecture Sketch
- Editor plugin registers a `UMcpAutomationBridge` subsystem.
- Subsystem subscribes to a local WebSocket or named pipe opened by the Node MCP server when it needs elevated actions.
- Each elevated command includes a capability token so the plugin can enforce an allow-list (exposed through project settings) and fail gracefully if disabled.
- Results are serialized back to the MCP server with structured warnings so the client can still prompt the user when manual intervention is required.

## Scaffolded Plugin (0.1.0)
- Plugin root lives under `Public/McpAutomationBridge` with a standard editor-only module layout.
- `McpAutomationBridge.uplugin` marks the bridge experimental and disabled by default so teams opt-in per project.
- `UMcpAutomationBridgeSubsystem` boots on editor startup, publishes a `bridge_started` handshake, and exposes `SendRawMessage` for transport validation.
- The subsystem maintains a WebSocket session to the MCP server using the endpoint defined in the new **Project Settings ▸ Plugins ▸ MCP Automation Bridge** panel. Headers include the optional capability token provided there. The transport now ships inside the plugin itself, removing any dependency on Epic’s `WebSockets` engine plugin.
- Settings are read during subsystem initialization; after changing the endpoint or token, restart the editor (or disable/re-enable the plugin) to apply updates.
- An `FTSTicker` callback governs reconnect backoff (configurable or disable-able via the same settings) and keeps the bridge responsive even when the socket drops unexpectedly.
- Broadcast delegates give Blueprint automation a simple entry point while C++ callers use the subsystem for richer payloads.

## Server Integration (0.1.0)
- `src/automation-bridge.ts` spins up a lightweight WebSocket server (default `ws://127.0.0.1:8090`) guarded by an optional capability token.
- Handshake flow: editor sends `bridge_hello` → server validates capability token → server responds with `bridge_ack` and caches the socket for future elevated commands.
- Environment flags: `MCP_AUTOMATION_WS_HOST`, `MCP_AUTOMATION_WS_PORT`, `MCP_AUTOMATION_CAPABILITY_TOKEN`, and `MCP_AUTOMATION_BRIDGE_ENABLED` allow operators to relocate or disable the listener without code changes.
- Health endpoint (`ue://health`) now surfaces bridge connectivity status so MCP clients can confirm when the plugin is online.

## Implemented Actions (0.2.0)
- `execute_editor_python` &mdash; runs Python source directly inside the editor via `IPythonScriptPlugin`. The MCP server routes to this action when callers set `execute_python.transport` to `automation_bridge`, enabling projects that disable Remote Control Python execution to continue automating scripts safely through the plugin channel.
- `set_object_property` &mdash; coerces JSON payloads into `FProperty` values using `FJsonObjectConverter`, marks the owning package dirty (unless `markDirty` is false), and emits a normalized response. When coupled with `inspect.set_property` + `transport = automation_bridge`, this unlocks typed property edits without relying on Remote Control.
- `get_object_property` &mdash; serializes the requested property into JSON and returns it to the server. `inspect.get_property` can target this path by setting `transport = automation_bridge`, providing a safe read channel when Remote Control is disabled.

## Incremental Plan
1. **Prototype Messaging** – implement a minimal editor command (e.g., resolve Blueprint CDO path) to validate transport reliability.
2. **SCS Writer** – add functions to create/remove components on Blueprints and commit nodes to disk.
3. **Typed Property Adapter** – leverage `FPropertyExporter` utilities to coerce enums, soft object paths, arrays, and structs.
4. **Asset Workflow Hooks** – wrap the Asset Registry and Asset Tools modules to execute save/move/delete with redirector cleanup and SCC integration.
5. **Modal Handling** – intercept common editor dialogs with latent actions exposed to the MCP server (with timeouts for safety).

## Open Questions
- Should the plugin expose additional authentication (e.g., session tokens) beyond local transport controls?
- How do we package the plugin so non-technical users can drop it into their project (marketplace vs. source plugin)?
- Do we need to support hot reload, or can we require a full editor restart when the plugin updates?

Contributions are welcome—open a discussion before tackling any of the stages above so we can keep the scope aligned with the main MCP server roadmap.
