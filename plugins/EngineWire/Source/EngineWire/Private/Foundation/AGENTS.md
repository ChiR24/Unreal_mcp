# Foundation AGENTS.md

Shared primitives for the EngineWire plugin. If you are writing a
handler in `Private/Domains/`, reuse what lives here. Do not grow a
domain-local copy of reflection, path, JSON, or response helpers.

## STRUCTURE (subdir -> what it gives you)

| Subdir | Count | Gives you |
|--------|-------|-----------|
| `Reflection/` | 12 files | UE property <-> JSON serialization boundary (`McpPropertyReflection*`), calls by reflection (`McpReflectedInvoke`), `McpInstancedStructView.h` |
| `HandlerUtils/` | 6 .h, 5 .cpp | action normalization, JSON/path/response/transform helpers, actor lookup by name, project config reads |
| `Blueprint/` | 4 .cpp, 1 .h | BP pin/type introspection and struct nodes, declared in `McpBlueprintUtils.h` |
| `Diagnostics/` | 8 files | diagnostics snapshot capture/load/rotation + file-name/schema helpers, `McpWorldTickTimer` |
| `GraphLayout/` | 6 files | graph node extent geometry, overlap tests, the layered layout (`McpLayered`) with its known-answer test |
| `Solvers/` | 20 files | general algorithms, one per file (a long one split), each with a known-answer test: `McpPratt` (the assertion language of execute's `expect`/`require`), `McpBoxSat` (boxes turned about Z: separation, depth, containment), `McpKMeans` (weighted k-means, fixed seed), `McpWatershed` (basins of a height over a graph, merged across high saddles), `McpBetweenness` (Brandes route shares of weighted nodes and edges), `McpCassowary` (linear constraints, required or preferred by strength, solved by the incremental simplex Kiwi builds) |
| `Optimize/` | 12 files | minimizers of a caller's cost, each with a known-answer test: `McpNelderMead` (the downhill simplex, for a cost of a few variables with no gradient: a camera judged by traces), `McpMinSnap` (the minimum-snap path through points, Richter et al.'s closed form), `McpAnneal` (Metropolis annealing over a caller's state), `McpChoiceKnapsack` (at most one option per group: the cheapest pick whose gains reach what is needed) |
| `Render/` | 5 files | post-process volume resolution, console variable writes, stat overlay state |
| `BridgeHelpers/` | umbrella + 7 groups | asset/actor/BP/property/response/security facade |
| root | 25 files (at cap) | `McpSecureTokenCompare.h` (constant-time token equality), capability authorization predicates and the path scan, the request principal and its quota, the idempotency ledger, compensation receipts, live-state revisions, readiness, the scoped editor transaction, screenshot resampling, the telemetry registry |

`Foundation` is split finely (<=25 files/folder, 250 pure-line ceiling) so
`tests/unit/plugin/source_structure*.test.ts` passes. Keep it that way.

## WHERE TO LOOK (Need -> use this header)

| Need | Use this header |
|------|-----------------|
| Serialize a UObject/UProperty to JSON | `Reflection/McpPropertyReflection.h` (`McpPropertyReflection`) |
| Apply JSON onto a property/object | `Reflection/McpPropertyReflection.h` `ApplyJsonValueToProperty` / `ApplyJsonObjectToObject` |
| Property type name / support check | `Reflection/McpPropertyReflection.h` `GetPropertyTypeName` / `IsPropertyTypeSupported` |
| Dispatch an action in a handler | Resolve with `HandlerUtils/McpHandlerUtilsActionsPaths.h` `NormalizeAction`, then an explicit compare chain (see `../Domains/AGENTS.md`) |
| Build an error response | `SendAutomationError(Socket, RequestId, Message, Code)` on the subsystem |
| Normalize/sanitize action + path strings | `HandlerUtils/McpHandlerUtilsActionsPaths.h` |
| Match a name with case, spaces and underscores ignored | `HandlerUtils/McpHandlerUtilsActionsPaths.h` `McpPlainKey` |
| Extract/coerce JSON fields | `HandlerUtils/McpHandlerUtilsJson.h` |
| Standard response builders | `HandlerUtils/McpHandlerUtilsResponses.h` |
| Transform / math helpers; a reply number to the nearest 1 / Scale | `HandlerUtils/McpHandlerUtilsTransforms.h` (`McpRoundTo`) |
| BP graph node helpers | `Blueprint/McpBlueprintUtils.h` |
| Resolve / validate `/Game` asset path | `BridgeHelpers/Assets/McpAssetResolve.h` |
| Load a material the caller named, no fallback | `BridgeHelpers/Assets/McpAssetResolve.h` `LoadMaterialForMcp` |
| Create assets / dirs / save registry | `BridgeHelpers/Assets/{McpAssetCreation,McpAssetDirs,McpAssetSaveRegistry}.h` |
| Make the package of a new asset | `BridgeHelpers/Assets/McpAssetCreation.h` `McpCreatePackageForNewAsset` (refuses an occupied path and a name the engine does not take) or `McpCreatePackageOrNull` (null where the engine's `CreatePackage` is a Fatal log: a name holding `//`); never the engine call (contract test) |
| Spawn an actor | `BridgeHelpers/Actors/McpActorSpawnHelpers.h` |
| Find an actor by name or label | `HandlerUtils/McpHandlerUtilsActionsPaths.h` `McpHandlerUtils::FindActorByName` |
| Load / compile BP, SCS lookup, BP paths | `BridgeHelpers/Blueprints/{McpBPAssetLoad,McpBPCompileHelpers,McpScsLookup,McpBPPaths}.h` |
| The engine's StandardMacros library (ForLoop, DoOnce, Gate...): load it, find a macro, tell one apart | `BridgeHelpers/Blueprints/McpStandardMacros.h` |
| Read/apply a property (scalar/object/array) | `BridgeHelpers/Properties/{McpNestedPropPath,McpComponentLookup,McpPropApply*}.h` (export: `Reflection/McpPropertyReflection.h`) |
| Refuse a number outside a property's ClampMin/ClampMax | `Reflection/McpPropertyReflection.h` `CheckRange` |
| Resolve a UClass by name | `BridgeHelpers/Reflection/McpClassResolve.h` |
| An enumerator's short name for a reply | `BridgeHelpers/Reflection/McpEnumName.h` `McpEnumName(Value)` |
| Build / verify response, capture output, JSON fields | `BridgeHelpers/Responses/{McpResponses,McpResponseVerify,McpOutputCapture,McpJsonFields}.h` |
| Refuse a missing field, a missing editor or a bad asset path in one line | `BridgeHelpers/Responses/McpResponses.h` `McpReadRequiredField` ("<field> required"), `McpRequireEditor`, `McpReadAssetPathOrReply` / `McpSanitizeAssetPathOrReply` (refusal wording, ASSET_NOT_FOUND): `if (!McpReadRequiredField(...)) return true;` |
| Set a console variable and know it took | `Render/McpConsoleVariableWrite.h` `McpSetConsoleVariable` (console priority by default, read-only refused, read back; `IConsoleVariable::Set` alone loses to a console set silently) |
| Show or hide a stat overlay without toggling it twice | `Render/McpViewportStats.h` `McpIsStatShown` (the viewport a Stat command lands on: the running game, else the last level viewport; Stat commands toggle) |
| Give a spawned volume real bounds | `../Domains/Volume/McpVolumeGeometry.h` `VolumeHelpers::CreateBoxBrushForVolume` (a volume from `SpawnActor` has no brush; scaling it leaves zero bounds) |
| Everything at once | `BridgeHelpers/McpHelpers.h` (umbrella) |

`Reflection/McpPropertyReflection.h` is THE UE-property <-> JSON boundary.
Every domain that touches properties funnels through it.

## THE SECURITY PRIMITIVES

- `McpSecureTokenCompare.h` -> `McpConstantTimeTokenEquals(A, B)`: XOR-accumulates
  the full UTF-8 span, folds length, no early exit. Called by BOTH the WebSocket
  bridge and native `/mcp`. Never replace with `==` / `Equals` (contract test fails).
- `BridgeHelpers/Security/McpProjectPaths.h`: confines paths
  to project roots (`/Game/...`, mount points), rejects `..`, `:`, `//`.
  `McpResolveProjectFilePath` is the one resolver for a file the plugin writes (containment, device names, no
  link on the way); `McpFileNameCase` is how this platform compares names (case-sensitive on Linux).
- `BridgeHelpers/Security/McpProjectFileWrite.h`: `McpWriteProjectFile` / `McpWriteProjectText` write a file the
  caller placed, refusing it when the file as opened is not the resolved path (a link made after the check). Use
  them, not `FFileHelper::Save*ToFile`, for a caller-chosen path.
- `McpCapabilityPrincipal.h`: the principal of the request whose handler runs now (`ActiveRequestPrincipal`,
  set by `FActiveRequestScope` around each dispatch). A handler that reaches a package the request did not name
  asks `ActiveRequestPermitsPath` / `ActiveRequestPermitsObject` (an actor counts as its level's map) or
  `ActiveRequestHoldsScope`; the save, delete and reflection sinks already do.
- `BridgeHelpers/Security/McpSecretMask.h`: `McpMaskSecrets` (free text) and `McpMaskSecretsDeep` (a JSON tree,
  a secret-named key's whole value) mask credentials before a reply, receipt or log line leaves; the key-name
  classifiers live in `McpSecretKeys.cpp`. Mask through these, never a local keyword list.
- `BridgeHelpers/Security/McpCommandValidate.h`: blocklist
  of `&&`, `||`, `;`, `|`, backtick, `<`, `>`, newline, CR for console/UBT args.
- `BridgeHelpers/Security/McpSafeReflectionTarget.h`: `IsAddressable` refuses an object in a `/Script` package
  (a native class or CDO, the plugin's own settings among them) and one outside a path-restricted caller's
  prefixes; the generic property handlers ask it before they read or write and refuse with `DenyCode()`.
- Hazardous editor ops (save/load/delete) live in `../Safety/`; include
  `Safety/McpSafeOperations.h`, which also declares the unqualified spellings
  (`McpSafeAssetSave`, `ScanPathSynchronous`, ...). See `../Safety/AGENTS.md`.

## CONVENTIONS

- SCS rule: Blueprint component templates must be owned by SCS nodes via
  `SCS->CreateNode()` / `SCS->AddNode()`. Relevant to `Blueprint/` and
  `BridgeHelpers/Blueprints/McpScsLookup.h`. Do not attach templates ad hoc.
- Adding a handler is a Core/Domains concern; the registration flow lives in
  `../Core/AGENTS.md` and `../Domains/AGENTS.md`.
- Every local `Mcp*` include must resolve (contract test). Include the umbrella
  `McpHelpers.h` unless you need a single shard.

## ANTI-PATTERNS

- Top one: writing a domain-local copy of a reflection/path/JSON/response helper
  that already lives here. Reuse it.
- Reimplementing `McpConstantTimeTokenEquals` with `==` / `FString::Equals`.
- Hand-rolling `/Game` path validation instead of `McpProjectPaths.h`.
- Skipping `McpCommandValidate.h` on console-command strings.
- Building responses by hand when `McpHandlerUtilsResponses.h` exists.
