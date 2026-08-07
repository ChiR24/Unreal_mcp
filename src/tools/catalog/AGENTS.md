# CAPABILITY RECORDS — CONTRACT SOURCE OF TRUTH + GENERATION PIPELINE

Contract records are hand-authored here. Everything downstream is generated. Hand-editing a generated file is silently overwritten on the next `registry:generate` and fails drift gates. This guide exists so you never make that mistake.

## STRUCTURE
```
capabilities/
|-- records/                      # (217 files) HAND-EDIT ZONE
|   |-- aggregate.ts              # composes ALL_CAPABILITY_RECORDS, asserts 1,335
|   |-- parent-metadata.ts        # parent tool metadata
|   |-- core/builder.ts           # CoreRecordSpec + buildCoreRecord() helper
|   `-- <parent>/                 # per-parent record dirs
|-- retrieval/aggregate.ts        # core source records
|-- model.ts  parser.ts  identifiers.ts  constants.ts  hashing.ts
|-- generated/                    # (3 files) NEVER HAND-EDIT
|   |-- canonical-registry.generated.ts   # ~243k lines
|   |-- canonical-registry.generated.json
|   `-- parent-tool-definitions.generated.ts
|-- normalization/  (20)          # BUILD/AUDIT time; sources committed, its inventory artifact is not
|-- semantic/       (17)          # RUNTIME
`-- migration/      (7)           # alias + migration-map generation
consolidated-tool-definitions.ts  # HAND facade: imports generated defs, gateway input
```
Generator scripts: `scripts/generate-canonical-registry.ts`, `scripts/generate-gateway-manifest.ts`, `scripts/canonical-registry/targets.ts`.

## SOURCE OF TRUTH (hand-edit these)
- `capabilities/records/**` (per-parent dirs)
- `capabilities/records/aggregate.ts` (asserts count = 1,335)
- `capabilities/records/parent-metadata.ts`
- `capabilities/retrieval/aggregate.ts`
- `capabilities/{model,parser,identifiers,constants,hashing}.ts`
- `consolidated-tool-definitions.ts` (hand facade; gateway generator reads it)

## GENERATED — NEVER HAND-EDIT (committed to git)
- `capabilities/generated/canonical-registry.generated.{ts,json}` (~243k lines), `parent-tool-definitions.generated.ts`
- `../orchestration/generated-routing-index.generated.ts`
- `../../gateway/gateway-manifest.generated.{ts,json}`
- plugin `Private/MCP/Tools/McpGeneratedParentRegistry.{h,cpp}` (aggregator) + 15 `McpGeneratedParentRegistry_<Group>.cpp` group shards
- plugin `Private/MCP/Generated/McpGeneratedCapabilityShards.h` + 23 `_MCP_CAP_SHARD_<PARENT>.cpp` (24 files total — one .cpp per canonical parent)
- plugin `Private/MCP/Gateway/McpNativeGatewayManifest.h`

## WHERE TO LOOK
| Task | Location |
|------|----------|
| Add/change a contract | `capabilities/records/<parent>/` + `records/aggregate.ts` |
| Boilerplate-heavy record | `capabilities/records/core/builder.ts` (`CoreRecordSpec` + `buildCoreRecord`) |
| Record type shape | `capabilities/model.ts` (`CapabilityRecord = CapabilityRecordSource & { hashes }`) |
| Recompile artifacts | `npm run registry:generate` then `npm run registry:check` |
| Regenerate gateway manifest | `node --loader ts-node/esm scripts/generate-gateway-manifest.ts` (`--check` = `npm run manifest:check`) |
| Audit route normalization | `capabilities/normalization/` (`generateInventory`, `assertRouteDispositionsComplete`); `normalization:check` / `normalization:audit` |

## ADDING / CHANGING A CONTRACT
1. Edit only files in the SOURCE OF TRUTH list above.
2. Author records via `buildCoreRecord(spec)` in `records/core/builder.ts`; declare deltas only.
3. Export the new record from its `<parent>/` index and into `records/aggregate.ts`.
4. Bump every record-count constant the new record lands in — there are THREE, and each throws at module load: `ALL_CAPABILITY_RECORD_COUNT` (`records/aggregate.ts`, all records), plus `PILOT_CAPABILITY_RECORD_COUNT` and `CORE_CAPABILITY_RECORD_COUNT` (`retrieval/aggregate.ts`) when the parent belongs to those catalogs. `scripts/canonical-registry/targets.ts` reads `ALL_CAPABILITY_RECORD_COUNT`, so that count has one source. Do NOT confuse any of them with `REVIEWED_METRICS.occurrenceCount`, which counts audited LEGACY occurrences and is permanently 1,335 — record counts and the audit total agree today only because the migration was 1:1. Expect ~28 unit files pinning a count or a freeze hash to need updating with the record.
5. A record authored AFTER the migration must declare `normalization.provenance: 'post-migration'`. Keep its `legacyIds` pair: the action enum, `describe`, and `execute {tool,action}` are all derived from that field and nothing else. `extractOccurrences()` skips a marked record, so `occurrenceCount` stays 1,335 and the audit keeps describing only what shipped pre-gateway. Omitting the marker is fail-closed — the record is counted, the reviewed total stops reproducing, and the normalization build throws. Every per-parent `buildRecord`/`buildCoreRecord` stamps `normalization` itself, so declare the whole object beside the spread rather than adding a builder parameter — a post-migration record wants its own rationale anyway:
   ```ts
   { ...buildRecord({ /* ... */ }),
     normalization: { class: 'C_SAME_VERB_DIFFERENT_TARGET', disposition: 'retain',
       rationale: 'Authored after the gateway migration; no pre-gateway occurrence to audit.',
       provenance: 'post-migration' } }
   ```
6. Run `npm run registry:generate` to rebuild all generated artifacts.
7. Run `npm run registry:check` (the `--check` drift gate). **It is NOT in CI** see CRITICAL DRIFT GAP.

## CONVENTIONS
- `CapabilityRecord` (`model.ts`): `CapabilityRecordSource & { hashes }`. Required source fields: id, aliases, legacyIds, discovery (domain/family/topics/summary/whenToUse/whenNotToUse), schemas (input+output Draft-2020-12), examples, availability (unreal min/max, requiredPlugins, editorStates), behavior (effect, idempotency, longRunning, safeToRetry, supportsPreview, supportsUndo), policy (requiredScope, consent, dataAccess), cost (latency, resources), routing (parentTool, dispatchAction, dispatchMode), normalization (class, disposition, rationale; optional aliasOf, provenance), deprecation, parent. `hashes` {algorithm, schema, content} added by `createCapabilityRecord`.
- `normalization/` is build/audit time: `generate.ts#generateInventory()` builds+validates an **in-memory** inventory from the hand-authored, committed `routedispositions*.data.ts` ledgers; `assertRouteDispositionsComplete` fails on an unreviewed route. The ledgers are committed; the derived inventory is not.
- `semantic/` is runtime: envelope.ts (stable key-sorted serialization for cross-transport hashing), ids.ts (CatalogRevision/CorrelationId/IdempotencyKey), handles/paths/pagination/errors/execution-options/save-policy/frame-time/geometry/property-assignment.
- Native shards are MSVC-chunked at 4,000-char string literals.

## FLOW (3 hops)
- (a) records -> `records/aggregate.ts` -> `generate-canonical-registry.ts` -> `generated/parent-tool-definitions.generated.ts` -> imported by `consolidated-tool-definitions.ts`.
- (b) `consolidated-tool-definitions.ts` -> `generate-gateway-manifest.ts` -> `gateway-manifest.generated.{ts,json}` + native `McpNativeGatewayManifest.h`.
- (c) canonical-registry generator (`scripts/canonical-registry/targets.ts`) -> native parent registry + capability shards.

## CRITICAL DRIFT GAP
`npm run manifest:check` runs in CI. `npm run registry:check` does NOT. A stale canonical registry or native shard can pass CI green. Always run `registry:check` locally after touching any record.

## GUARD TESTS
- `tests/unit/plugin/gateway/generated_shard_source_contracts.test.ts` (shards MSVC-safe, ≤4,000-char literals, 23 .cpp, no orphan .h)
- `tests/unit/canonical-registry-parent-derivation.test.ts`
- `tests/unit/normalization-post-migration-provenance.test.ts` (the provenance skip, both fail-closed directions, and the non-empty `legacyIds` invariant)
- The record-count assertion in `records/aggregate.ts`

## ANTI-PATTERNS
- Editing any `*.generated.*`, `capabilities/generated/`, `gateway-manifest.generated.*`, or plugin `McpGenerated*` / `McpNativeGatewayManifest.h` by hand. Regenerate instead.
- Trusting a green CI as proof the registry is fresh. Run `registry:check`.
- Authoring a record without `buildCoreRecord` boilerplate filling (declares only deltas).
- Skipping `assertRouteDispositionsComplete` when adding a route (unreviewed routes fail the audit).
- Bumping `ALL_CAPABILITY_RECORD_COUNT` to silence a mismatch instead of fixing the records.
- Touching `REVIEWED_METRICS.occurrenceCount` or `normalization-inventory.json` to make room for a new record. That artifact records what shipped pre-gateway; raising it would make it assert something untrue. Mark the record `post-migration` instead.
- Emptying `legacyIds` to dodge the audit. The schema refuses it, because a record with no pair is absent from the parent action enum and from the gateway's legacy-pair index — it would ship searchable and callable by canonical id while `describe` never names its action.
