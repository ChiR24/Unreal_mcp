# `src/server/mcp-primitives/` — MCP Protocol Primitives

Everything MCP exposes besides tools and resources: workflow prompts, argument completion and progress, plus the revision stamped on resource payloads.

## FILES
- `primitive-wiring.ts` — ENTRY. `SERVER_CAPABILITIES` (`tools`, `resources` with `subscribe`, `prompts`, `completions`) is the exact advertised surface; `wirePrimitives(server)` registers `prompts/list`, `prompts/get`, `completion/complete`. Tools and resources register in `src/server/tool-registry.ts` and `src/handlers/resource-handlers.ts`.
- `resource-subscriptions.ts` — `wireResourceSubscriptions(server, automationBridge)`, one per Server instance (unbound in `onclose`): `resources/subscribe` / `unsubscribe` for 2025-era clients, and `sendResourceUpdated({ uri: 'ue://changes' })` when the bridge emits `changesMoved` (the plugin's `changes_moved`, at most one a second). A 2026-07-28 instance always sends it, because the SDK's stdio listen router hands it only to the `subscriptions/listen` streams that named the URI; a 2025-era instance passes everything through, so it sends only when the client subscribed. Native mirror: `Private/MCP/Protocol/McpNativeTransportListen.cpp`.
- `prompts.ts` — `WORKFLOW_PROMPTS`, `listPrompts()`, `getPrompt()`. Returns text messages only; getting a prompt never executes anything.
- `completions.ts` — `complete(ref, argument, prefix)`: prefix > substring > subsequence > one-typo ranking, capped by item count and bytes; host paths and traversal prefixes return nothing.
- `resource-revision.ts` — `ResourceRevision`, `INITIAL_REVISION`. `ue://capability/catalog` moves with `configure` visibility changes; `ue://changes` reports its cursor's number plus one (computed in `src/resources/editor-state-resources.ts`); every other revisioned URI reports `INITIAL_REVISION`.
- `progress/` — `readProgressToken()` reads the client's `_meta.progressToken` (never allocated), `createProgressReporter()` (bounded by `MAX_PROGRESS_NOTIFICATIONS`, `MAX_PROGRESS_MESSAGE_LENGTH`), `ProgressSinkRegistry` routes Unreal progress to the owning MCP request.

## ANTI-PATTERNS
- Advertising a capability in `SERVER_CAPABILITIES` without registering its handlers.
- Executing a tool from a prompt or completion path.
- Unbounded completion or progress output.
- Mutating state from a resource read.
- Reading `ue://changes` or polling `changes_since` through the bridge from the server itself: every request the plugin answers takes the user-change warning the agent's next call should carry. The cursor comes from `bridge_ack` and `changes_moved`.
