# CAPABILITY RECORDS — CONTRACT SOURCE OF TRUTH + GENERATION PIPELINE

Contract records are hand-authored here. Everything downstream is generated. A hand edit to a generated file is overwritten on the next `registry:generate` and fails the drift gates.

## STRUCTURE
```
capabilities/
|-- records/                      # HAND-EDIT ZONE
|   |-- aggregate.ts              # ALL_CAPABILITY_RECORDS (folded); asserts ALL_CAPABILITY_RECORD_COUNT = 385
|   |-- unfolded.ts               # every authored source before folding (tests read it)
|   |-- parent-metadata.ts        # parent tool metadata
|   |-- compensation.ts           # behavior.compensation (inverse capability or cleanup note)
|   |-- core/builder.ts           # buildCoreRecord(): declare deltas only
|   |-- shared/fold*.ts           # applyFolds(): folds sibling records into one family
|   |-- folds/<parent>.folds.ts   # fold specs (data): primary, selector, members
|   `-- <parent>/                 # per-parent record dirs
|-- generated/                    # NEVER HAND-EDIT: canonical-registry.generated.{ts,json},
|                                 #   capability-cost-index.generated.ts, parent-tool-definitions.generated.ts
|-- retrieval/                    # scoring.ts, tokenize.ts, alias-fold.ts, constants.ts
|-- semantic/                     # RUNTIME: execution options, errors, paths, handles, receipts
`-- model.ts  parser.ts  identifiers.ts  hashing.ts  record-*.ts
```
Generators: `scripts/generate-canonical-registry.ts` (+ `scripts/canonical-registry/`), `scripts/generate-gateway-manifest.ts`.

## GENERATED — NEVER HAND-EDIT (committed)
- `capabilities/generated/*` (4 files)
- `src/gateway/gateway-manifest.generated.{ts,json}`
- `docs/action-reference.generated.md`
- plugin `Private/MCP/Tools/McpGeneratedParentRegistry.cpp`
- plugin `Private/MCP/Generated/McpGeneratedCapabilityShards.h` + one `_MCP_CAP_SHARD_<PARENT>.cpp` per parent (24 files; MSVC-chunked at 4,000-char literals)

## THE RECORD
`CapabilityRecord = CapabilityRecordSource & { hashes }`. Source fields: `id`, `aliases`, `legacyIds`, `discovery` (domain/family/topics/summary/whenToUse/whenNotToUse), `schemas` (input + output, Draft 2020-12), `examples`, `availability` (requiredPlugins, editorStates), `behavior` (effect, idempotency, longRunning, safeToRetry, compensation?), `policy` (requiredScope, consent, dataAccess), `cost` (latency, resources), `routing` (parentTool, dispatchAction, dispatchBy?), `parent`.

Declare exactly what the C++ handler reads and emits. The gateway validates strictly (undeclared param → `UNDECLARED_PARAMETER`), and `npm run test:params` (strict) fails on a case param no schema declares or an optional param no case covers.

## FOLDED FAMILIES (read before adding an action)
385 records fold 1,274 authored sources; 236 are families, and 1,453 `{tool, action}` pairs stay callable. `records/folds/<parent>.folds.ts` lists, per family, the primary action, a selector parameter (`kind`, `edit`, `setting`, ...) and the member each selector value dispatches to; `routing.dispatchBy` maps selector → bridge action, and each former name is a `legacyIds[]` entry carrying `folded: { <selector>: <value> }` pins that both gateways inject before validation. A family whose primary is one of its members keeps the selector optional with that member as default. Members must share effect, policy, availability, family and id namespace (`applyFolds` throws otherwise).

To add an action to an existing family, author its record in `<parent>/` and add it to the family spec (a new selector value, or an `aliasMembers` entry). A brand-new operation is a new record, left unfolded.

## ADDING / CHANGING A CONTRACT
1. Author via `buildCoreRecord(spec)` (or the parent's builder); export it into `records/aggregate.ts`.
2. A new folded record changes `ALL_CAPABILITY_RECORD_COUNT`; fix the records or folds on a mismatch, never the constant to silence it.
3. `npm run registry:generate`, then `registry:check` and `manifest:check` (both CI gates).
4. Add or update the integration case under `tests/mcp-tools/` so `test:params` covers every optional param.

## RETRIEVAL
- `discovery.topics` is the search vocabulary (field weight 8 in `retrieval/constants.ts`). Declare 3-6 phrases a caller types, not copies of the action name; BM25 length normalization means extra phrases dilute each match.
- When the caller's verb is not in the action name (`move actor` vs `set_transform`), declare an alias id (`aliases: ['control_actor.move_actor']`). Aliases must be unique across ids and aliases (`GATEWAY_INDEX_CONFLICT`).
- `tests/unit/gateway-search-vocabulary.test.ts` pins the phrasings that must rank first; `npm run eval:check` measures the corpus.

## ANTI-PATTERNS
- Editing any `*.generated.*` or plugin `McpGenerated*` by hand.
- Bumping `ALL_CAPABILITY_RECORD_COUNT` to silence a mismatch.
- Emptying `legacyIds`: the record would drop out of the action enum and the legacy-pair index.
- Declaring params the handler never reads, or leaving out ones it does.
