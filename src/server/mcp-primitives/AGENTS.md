# `src/server/mcp-primitives/` — MCP Protocol Primitives

Everything MCP exposes besides tools and resources: workflow prompts, argument completion and progress, plus the revision stamped on resource payloads.

## FILES
- `primitive-wiring.ts` — ENTRY. `SERVER_CAPABILITIES` (`tools`, `resources`, `prompts`, `completions`) is the exact advertised surface; `wirePrimitives(server)` registers `prompts/list`, `prompts/get`, `completion/complete`. Tools and resources register in `src/server/tool-registry.ts` and `src/handlers/resource-handlers.ts`.
- `prompts.ts` — `WORKFLOW_PROMPTS`, `listPrompts()`, `getPrompt()`. Returns text messages only; getting a prompt never executes anything.
- `completions.ts` — `complete(ref, argument, prefix)`: prefix > substring > subsequence > one-typo ranking, capped by item count and bytes; host paths and traversal prefixes return nothing.
- `resource-revision.ts` — `ResourceRevision`, `INITIAL_REVISION`. Only `ue://capability/catalog` moves (it follows `configure` visibility changes); every other revisioned URI reports `INITIAL_REVISION`.
- `progress/` — `readProgressToken()` reads the client's `_meta.progressToken` (never allocated), `createProgressReporter()` (bounded by `MAX_PROGRESS_NOTIFICATIONS`, `MAX_PROGRESS_MESSAGE_LENGTH`), `ProgressSinkRegistry` routes Unreal progress to the owning MCP request.

## ANTI-PATTERNS
- Advertising a capability in `SERVER_CAPABILITIES` without registering its handlers.
- Executing a tool from a prompt or completion path.
- Unbounded completion or progress output.
- Mutating state from a resource read.
