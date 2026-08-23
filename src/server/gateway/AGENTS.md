# src/server/gateway/ — GATEWAY ROUTING ENGINE

The 23 source files here own search/describe/execute/configure routing for the `unreal` gateway tool. They decide WHAT to call and HOW to shape the request, then hand off to the canonical 23-tool boundary. They do not implement domain logic. (Plus 3 colocated unit-test files and this guide; 27 entries total.)

NOTE: `src/gateway/` (sibling, 3 of 4 files generated) is only the manifest DATA + loader. Never edit it from here; never route through it at runtime.

## STRUCTURE

The gateway ENTRY lives one level up: `src/server/tool-registry-gateway.ts` -> `handleUnrealGatewayCall()` -> `dispatchGatewayOperation()` switching search/describe/execute/configure (`configure` wraps `handleManageToolsCall()`).

| File | Owns |
|------|------|
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
| `gateway-execute-validate.ts` | Facade re-exporting the validation stages below (`applyDeclaredDefaults` `validateExecutionOptions` `findControlKeyInParams` `validateAgainstCapabilitySchema`) |
| `gateway-option-validate.ts` | Stage 2-3 execution-option rules: supported keys, timeout bounds, idempotency-key format, `expectedRevisions` shape, preview refusal, unimplemented-option refusal |
| `gateway-schema-validate.ts` | Stage 4 Draft-2020-12 subset validator: supported keywords, fail-closed unknown keyword, `hasOwn` prototype-safe lookup, declared defaults |
| `gateway-execute-envelope.ts` | `refuseWithTarget` `executeErrorEnvelope` `executeSuccessEnvelope` `toSemanticError` |
| `gateway-execute-policy.ts` | execute-stage policy gate (authorization preflight) |
| `gateway-execute-idempotency.ts` | execute-stage idempotency slot handling (delegates to `idempotency-ledger.ts`) |
| `idempotency-ledger.ts` | Principal-scoped idempotency ledger (cap 1024, SHA-256 slot; native mirror cap 4096) |
| `gateway-receipt-context.ts` | `buildReceiptContext()` — correlation, catalog revision, echoed options |
| `gateway-execute-dispatch.ts` | `dispatchAndValidate()` -> `maybeElicitMissingArgs()` -> `handleConsolidatedToolCall()` |
| `direct-call-migration.ts` | `DIRECT_TOOL_CALL_REMOVED` receipt for direct canonical-name calls |
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
- Drifting the native `MCP/Gateway/` mirror out of sync with these 23 files.
