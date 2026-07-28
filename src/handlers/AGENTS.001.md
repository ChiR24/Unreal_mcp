# `src/handlers/` — MCP Resource Request Handlers

2 files. Sits in front of `src/resources/` (the providers). Dependency runs: `MCP protocol -> handlers -> resources`.

`resource-handlers.ts` — the only handler. Exports `ResourceHandler` with `registerHandlers()`. Registers `ReadResourceRequestSchema`. Handles legacy URIs directly: `ue://assets` (AssetResources.list), `ue://actors` (ActorResources.listActors), `ue://level` (LevelResources.getCurrentLevel), `ue://health` (HealthMonitor), `ue://automation-bridge` (AutomationStatusBridge), `ue://version`. Delegates all other URIs to `extendedReader.read(uri)` if provided.