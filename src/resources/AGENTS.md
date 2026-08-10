# `src/resources/` — Resource Providers

17 `.ts` files. `src/handlers/resource-handlers.ts` (the 2-file MCP request handlers) IMPORTS and CALLS these providers. Dependency runs: `handlers -> resources`.

## What each file owns

`resource-read-router.ts` — **ENTRY POINT** for non-legacy URIs. Exports `ResourceReadRouter` class with `read(uri: string): Promise<ResourceReadResult>`. Switches on exact URIs first (`ue://project`, `ue://editor`, `ue://selection`, `ue://capability/catalog`), then prefix-matches `ue://capability/`, `ue://knowledge/`, `ue://object/`, `ue://asset/`. Enforces byte budget and returns MCP `contents`.

`resource-catalog.ts` — owns `NEW_RESOURCE_DEFINITIONS` (4 static) and `RESOURCE_TEMPLATES` (4 templates). NOT an entry point; just the catalog data consumed by the router.

`resource-errors.ts` — exports `RESOURCE_ERROR_CODES` = `{ INVALID_URI, NOT_FOUND, UNAVAILABLE, TOO_LARGE, TRAVERSAL }` and typed `ResourceError`. Also `enforceByteBudget(uri, text)` (64 KiB max) and `normalizeContentPath(uri, rawPath)` (rejects host paths, traversal, non-UE-mount roots). `redactProjectName(raw)` strips host paths to project name only.

`editor-state-resources.ts` — exports `EditorStateResources` (`readProject`, `readEditor`, `readSelection`) and `BridgeEditorStateSource`. Live reads cross the game-thread boundary through an injected `EditorStateSource`; default binds to the automation bridge; unit tests inject a fake. When editor unavailable throws `ResourceError` and never mutates. `MAX_SELECTION = 200`; payload includes `count`, `totalCount`, `truncated`.

`capability-resources.ts` — exports `CapabilityResources` (`readCatalog`, `readRecord`). Default data source is `GatewayManifestCapabilitySource` reading the neutral gateway manifest. URI: `ue://capability/catalog`; template: `ue://capability/{capabilityId}`. Cap: 64 catalog entries, 200 actions per record.

`knowledge-resources.ts` — exports `KnowledgeResources` (`readKnowledge`, `readObject`, `readAsset`). Static topics: `paths`, `safety`, `gateway`, `transports`, `resources`. URI: `ue://knowledge/{engineVersion}/{topic}`. Object/asset refs: `ue://object/{objectPath}`, `ue://asset/{assetPath}` — normalize the path then check existence via injected `AssetLookupSource` (default: `BridgeAssetLookupSource` over `asset_exists`). When editor unavailable throws `ResourceError` and never mutates.

`asset-pagination.ts` — exports `getAssetListTtlMs()` (default 10s from `ASSET_LIST_TTL_MS` env), `normalizePage(value)`, `normalizePageSize(value)` (default 30, max 50). Pure pagination helpers; no resource URIs.

`assets.ts` — exports `AssetResources` class (extends `BaseTool`, implements `IAssetResources`). Methods: `list(dir, recursive, limit)`, `listPaged(dir, page, pageSize, recursive)`, `find(assetPath)`, `clearCache(dir?)`, `invalidateAssetPaths(paths[])`. Uses `asset-pagination` helpers. 10s TTL cache. Normalizes `/Content` to `/Game`. Forces non-recursive listing. Bridges to `list` action with `limit` (max 1,000 internally for paged full scan).

`actors.ts` — exports `ActorResources` class. Methods: `listActors()`, `getActorByName(name)`, `getActorTransform(actorPath)`, `listActorComponents(actorPath)`. 5s TTL cache.

`levels.ts` — exports `LevelResources` class. Methods: `getCurrentLevel()`, `getLevelName()`, `saveCurrentLevel()`. Bridges to `list_levels` and `save_level` actions.

## Revision URIs

`ue://project`, `ue://editor` (tracks `ue://pie`), `ue://selection`, `ue://capability/catalog`. Injected via `RevisionProvider` from `../server/mcp-primitives/resource-revision.js`. `ue://editor` is NOT independently subscribable — its revision follows `ue://pie`.

## WHERE TO LOOK

| Task | Location | Notes |
|------|----------|-------|
| Add/change resource URI | `resource-catalog.ts` + `resource-read-router.ts` | Catalog entry + router switch arm |
| Resource handler wiring | `src/handlers/resource-handlers.ts` | Instantiates providers, passes `ExtendedResourceReader` |
| Error codes | `resource-errors.ts` | `RESOURCE_ERROR_CODES` enum |
| Editor state (live) | `editor-state-resources.ts` | `BridgeEditorStateSource` |
| Capability catalog source | `capability-resources.ts` | `GatewayManifestCapabilitySource` |
| Asset pagination tuning | `asset-pagination.ts` | TTL env, page size bounds |

## ANTI-PATTERNS

- Never call `EditorStateResources` methods without going through the injected `EditorStateSource` — the default `BridgeEditorStateSource` requires a live editor connection.
- Resource reads are READ-ONLY; never mutate editor state from a resource read path.
- Never emit a host filesystem path or secret — `redactProjectName` and `normalizeContentPath` must guard every path input.
- Do not bypass `resource-read-router.ts` for new URIs — add a switch arm there, not a parallel routing path.
- Respect the `MAX_SELECTION = 200` cap and the 64 KiB byte budget enforced by `enforceByteBudget`.
- Do not inject a live `AutomationRequestBridge` into resource providers in unit tests — use a fake `EditorStateSource` / `AssetLookupSource` instead.
- Do not expose `ue://editor` as its own subscription; it tracks `ue://pie`.