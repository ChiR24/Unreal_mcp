# src/server/gateway/ — GATEWAY ROUTING ENGINE

The 16 files here own search/describe/execute/configure routing for the `unreal` gateway tool. They decide WHAT to call and HOW to shape the request, then hand off to the canonical 23-tool boundary. They do not implement domain logic.

NOTE: `src/gateway/` (sibling, 3 of 4 files generated) is only the manifest DATA + loader. Never edit it from here; never route through it at runtime.

## STRUCTURE

| File | Owns |
|------|------|
| `tool-registry-gateway.ts` | Entry: `handleUnrealGatewayCall()` -> `dispatchGatewayOperation()` switching search/describe/execute/configure. `configure` wraps `handleManageToolsCall()`. |
| `gateway-shared.ts` | `getString` `getBoundedInteger` `gatewayError` `isGatewayFailure` `findTool` `allToolNames` `nextGatewayCorrelationId` |
| `gateway-search.ts` | `searchGatewayCapabilities()` |
| `gateway-search-filters.ts` | `readFilters` `validateFilters` `selectCandidates`; cursor encode/decode |
| `gateway-describe.ts` | `describeGatewayCapability()` level router |
| `gateway-describe-browse.ts` | `describeCatalog` `describeDomain` `describeFamily` + pagination |
| `gateway-describe-capability.ts` | `describeCapabilityRecord` `describeCapabilityParameter` `describeCapabilityReference` |
| `gateway-capability-view.ts` | `capabilityContract()`, declared param names, `parameterSchema` |
| `gateway-capability-index.ts` | `capabilityIndex` `resolveCapability` `resolveLegacyPair` `catalogRevision` `allCapabilityIds` |
| `gateway-availability.ts` | `capabilityAvailability()` |
| `gateway-execute.ts` | `executeGatewayCall()` |
| `gateway-execute-resolve.ts` | `executeTargetIndex` `resolveExecuteTarget` (capability id, tool+action, alias, migration) |
| `gateway-execute-validate.ts` | `validateAgainstCapabilitySchema` `applyDeclaredDefaults` `validateExecutionOptions` `findControlKeyInParams` |
| `gateway-execute-envelope.ts` | `refuseWithTarget` `executeErrorEnvelope` `executeSuccessEnvelope` `toSemanticError` |
| `gateway-execute-dispatch.ts` | `dispatchAndValidate()` -> `maybeElicitMissingArgs()` -> `handleConsolidatedToolCall()` |
| `gateway-guidance.ts` | `closestMatches()` (Levenshtein+prefix, `MAX_SUGGESTIONS=3`) `buildNextCall()` |
| `gateway-schema-normalize.ts` | `normalizeSchemaTypes()` |

## REQUEST FLOW

1. `handleUnrealGatewayCall` -> `dispatchGatewayOperation` picks the op.
2. **search**: `searchGatewayCapabilities` -> `readFilters` -> `validateFilters` -> `selectCandidates` (cursor paged).
3. **describe**: `describeGatewayCapability` level router -> browse / capability / catalog shards.
4. **execute** stage order (preserve exactly):
   a. `resolveExecuteTarget` (id, tool+action, alias, migration).
   b. `checkStaticRequest`: enabled -> params -> options -> `applyDeclaredDefaults` -> `validateAgainstCapabilitySchema`.
   c. `context.ensureConnected()`.
   d. `dispatchAndValidate` -> `maybeElicitMissingArgs` -> `handleConsolidatedToolCall(record.routing.parentTool, targetArgs, context.tools)`.
   e. Result held to declared output schema.
5. **configure**: wraps `handleManageToolsCall`.

Execute error codes: `UNREAL_EXECUTION_ERROR`, `OUTPUT_SCHEMA_VIOLATION`, `RESULT_TOO_LARGE`.

## PROGRESSIVE DISCLOSURE

`describe` never dumps a full inputSchema. Levels:
- `{}` -> domains
- `{domain}` -> families
- `{domain?,family}` -> capabilities
- `{capability}` -> one exact contract
- `{capability,param}` -> one parameter schema
- legacy `{tool}` -> parent summary (`perActionSchemas` forced `false`; the parameter catalog is the tool-UNION across all actions)
- `{tool,action}` -> the capability behind the pair

Unknown tool/action/param returns `suggestions` (`closestMatches`) + executable `nextCall`.

## CONVENTIONS

- The gateway never calls a domain handler directly. `handleConsolidatedToolCall(record.routing.parentTool, targetArgs, context.tools)` is the canonical 23-tool boundary.
- No mode toggle: the public surface is permanently the single `unreal` gateway tool. The `config.MCP_GATEWAY_MODE` flag and the `tool-registry-legacy.ts` direct-listing path were removed.
- Native mirror: `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Gateway/`. Keep behaviorally in sync; it is likewise permanent (the `bEnableNativeGateway` setting was removed).

## ANTI-PATTERNS

- Routing around `handleConsolidatedToolCall` to a domain handler.
- Editing `src/gateway/` manifest data from this engine.
- Dumping full `inputSchema` at the describe summary level (breaks progressive disclosure).
- Drifting the native `MCP/Gateway/` mirror out of sync with these 16 files.
