# CAPABILITY RECORDS — CONTRACT SOURCE OF TRUTH + GENERATION PIPELINE

Contract records are hand-authored here. Everything downstream is generated. A hand edit to a generated file is overwritten on the next `registry:generate` and fails the drift gates.

## STRUCTURE
```
capabilities/
|-- records/                      # HAND-EDIT ZONE
|   |-- aggregate.ts              # ALL_CAPABILITY_RECORDS (folded); asserts ALL_CAPABILITY_RECORD_COUNT (449 on 2026-10-27)
|   |-- unfolded.ts               # every authored source before folding (tests read it)
|   |-- parent-metadata.ts        # parent tool metadata
|   |-- compensation.ts           # behavior.compensation (inverse capability or cleanup note)
|   |-- core/builder.ts           # buildCoreRecord(): declare deltas only
|   |-- shared/fold*.ts           # applyFolds(): folds sibling records into one family
|   |-- folds/<parent>.folds.ts   # fold specs (data): primary, selector, members
|   `-- <parent>/                 # per-parent record dirs
|-- generated/                    # NEVER HAND-EDIT: canonical-registry.generated.{ts,json},
|                                 #   capability-cost-index.generated.ts, parent-tool-definitions.generated.ts
`-- model.ts  parser.ts  identifiers.ts  hashing.ts  record-*.ts
```
Generators: `scripts/generate-canonical-registry.ts` (+ `scripts/canonical-registry/`), `scripts/generate-gateway-manifest.ts`.

## GENERATED — NEVER HAND-EDIT (committed)
- `capabilities/generated/*` (4 files)
- `src/gateway/gateway-manifest.generated.json`
- `docs/action-reference.generated.md`
- plugin `Private/MCP/Tools/McpGeneratedParentRegistry.cpp`
- plugin `Private/MCP/Generated/McpGeneratedCapabilityShards.h` + one `McpCapShard<Parent>.cpp` per parent (24 files; MSVC-chunked at 4,000-char literals)

## THE RECORD
`CapabilityRecord = CapabilityRecordSource & { hashes }`. Source fields: `id`, `aliases`, `legacyIds`, `discovery` (domain/family/topics/summary/whenToUse/whenNotToUse), `schemas` (input + output, Draft 2020-12), `examples`, `availability` (requiredPlugins, editorStates, minEngineVersion? — the first 5.x whose build has the handler; the plugin reads the record unavailable below it and refuses it `ENGINE_VERSION_UNSUPPORTED`), `behavior` (effect, idempotency, longRunning, safeToRetry, compensation?), `policy` (requiredScope, consent, dataAccess, raisedBy? = input params that demand a stricter scope as well when sent; a fold merges them), `cost` (latency, resources), `routing` (parentTool, dispatchAction, dispatchBy?).

Declare exactly what the C++ handler reads and emits. The gateway validates strictly (undeclared param → `UNDECLARED_PARAMETER`), and `npm run test:params` (strict) fails on a case param no schema declares or an optional param no case covers.

## FOLDED FAMILIES (read before adding an action)
480 records fold 1,459 authored sources; 242 are families, and 1,642 `{tool, action}` pairs stay callable (2026-10-29). `records/folds/<parent>.folds.ts` lists, per family, the primary action, a selector parameter (`kind`, `edit`, `setting`, ...) and the member each selector value dispatches to; `routing.dispatchBy` maps selector → bridge action, and each former name is a `legacyIds[]` entry carrying `folded: { <selector>: <value> }` pins that the gateway injects before validation, plus the `aliases` that member declared (a call by one runs that member as its old name does; the primary's own aliases stay on the record alone). A family whose primary is one of its members keeps the selector optional with that member as default. Members must share effect, policy, availability, family and id namespace (`applyFolds` throws otherwise). A spec's `topics` come first and its members' own topics follow (search reads them all), and `routing.dispatchBy.declaredBy` names the variants that declare each parameter the members do not all share, which describe shows as `variants` on that parameter.

A family's discovery summary is the spec's `summary`; each member's own summary goes under the selector parameter's description, one line per variant (`value: summary`, a sibling action name rewritten to `selector=value`). That line is where a member's defaults, replies and refusals reach a caller, so write member summaries in full; search scores the spec summary, topics and whenToUse, never parameter descriptions.

To add an action to an existing family, author its record in `<parent>/` and add it to the family spec (a new selector value, or an `aliasMembers` entry). A brand-new operation is a new record, left unfolded.

## ADDING / CHANGING A CONTRACT
1. Author via `buildCoreRecord(spec)` (or the parent's builder); export it into `records/aggregate.ts`.
2. A new folded record changes `ALL_CAPABILITY_RECORD_COUNT`; fix the records or folds on a mismatch, never the constant to silence it.
3. `npm run registry:generate`, then `registry:check` and `manifest:check` (both CI gates).
4. Add or update the integration case under `tests/mcp-tools/` so `test:params` covers every optional param.

## RETRIEVAL
- `discovery.topics` is the search vocabulary (rule weight 12 in `McpNativeGatewaySearchMatch.cpp`). Declare 3-6 phrases a caller types, not copies of the action name.
- When the caller's verb is not in the action name (`move actor` vs `set_transform`), declare an alias id (`aliases: ['control_actor.move_actor']`). Aliases must be unique across ids and aliases (`GATEWAY_INDEX_CONFLICT`).
- Search adds a full-coverage bonus (100) when every word of a multi-word request is named by a record's names, family, domain or topics; summary and whenToUse prose never count. A topic phrase that holds all of a caller's words beats a sibling family whose member names match only some of them. A topic that holds the whole request as written adds 50 more (`McpSearchPhraseTopicBonus`): "tag an asset" reaches `set_metadata`, not `query_asset` through its `find_by_tag` name.
- `tests/eval/native-ranker.ts` is a TypeScript copy of the native ranker that reads its word lists from the C++. `native-gate.test.ts` pins it to the C++ weights and holds it to the corpus floors, and `plain-probe.test.ts` holds it to a floor on its plain requests (`npm run eval:check`). Raise a floor when a change gains cases, never lower one to pass.

## ANTI-PATTERNS
- Editing any `*.generated.*` or plugin `McpGenerated*` by hand.
- Bumping `ALL_CAPABILITY_RECORD_COUNT` to silence a mismatch.
- Emptying `legacyIds`: the record would drop out of the action enum and the legacy-pair index.
- Declaring params the handler never reads, or leaving out ones it does.
