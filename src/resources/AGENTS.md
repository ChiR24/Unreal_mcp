# `src/resources/` — Resource Providers

Data behind MCP resources. `src/handlers/resource-handlers.ts` owns registration (`new ResourceHandler(...).registerHandlers()`) and calls these providers; dependency runs `handlers -> resources`.

## URIs
- Served by `resource-handlers.ts` directly: `ue://assets` (`AssetResources.list('/Game')` → `manage_asset list`), `ue://actors` (`listActors()` → `control_actor list`), `ue://health`, `ue://automation-bridge`.
- Served by `resource-read-router.ts` (`ResourceReadRouter.read(uri)`): exact `ue://capability/catalog`, `ue://project`, `ue://editor`, `ue://selection`, `ue://state/revisions`; templates `ue://capability/{capabilityId}`, `ue://knowledge/{topic}`, `ue://asset/{assetPath}`.

## FILES
- `resource-read-router.ts` — ENTRY for the router URIs; enforces the byte budget and returns MCP `contents`.
- `resource-catalog.ts` — the static definitions and templates the router serves.
- `resource-errors.ts` — `RESOURCE_ERROR_CODES` (`INVALID_URI`, `NOT_FOUND`, `UNAVAILABLE`, `TOO_LARGE`, `TRAVERSAL`), `ResourceError`, `enforceByteBudget()` (64 KiB), `normalizeContentPath()` (rejects host paths, traversal, non-mount roots), `redactProjectName()`.
- `editor-state-resources.ts` — `EditorStateResources` over an injected `EditorStateSource` (default `BridgeEditorStateSource`). Throws `ResourceError` when the editor is unavailable; never mutates. `MAX_SELECTION = 200`.
- `capability-resources.ts` — `CapabilityResources` over `GatewayManifestCapabilitySource` (64 catalog entries, 200 actions per record).
- `knowledge-resources.ts` — static topics; `ue://asset/{assetPath}` normalizes the path and asks `manage_asset exists`.
- `assets.ts`, `actors.ts` — the two list resources.

## REVISIONS
From `../server/mcp-primitives/resource-revision.ts`. Only `ue://capability/catalog` moves; `ue://editor` reports the `ue://pie` revision.

## ANTI-PATTERNS
- Mutating editor state from a read path.
- Emitting a host filesystem path or secret: `normalizeContentPath` and `redactProjectName` guard every path.
- A parallel routing path for a new URI instead of a router arm.
- Sending a bare action name to the bridge: always `{parent tool, { action, ... }}`.
