# Foundation AGENTS.md

Shared primitives for the McpAutomationBridge plugin. If you are writing a
handler in `Private/Domains/`, reuse what lives here. Do not grow a
domain-local copy of reflection, path, JSON, or response helpers.

## STRUCTURE (subdir -> what it gives you)

| Subdir | Count | Gives you |
|--------|-------|-----------|
| `Reflection/` | 2 .h (10 files w/ .cpp) | UE property <-> JSON serialization boundary |
| `HandlerUtils/` | 6 .h | dispatch macros, JSON/path/response/transform helpers |
| `Blueprint/` | 5 .cpp | BP pin/type introspection (no public .h; pull via subsystem) |
| `BridgeHelpers/` | umbrella + 7 groups | asset/actor/BP/property/response/security facade |
| root | `McpSecureTokenCompare.h` | constant-time token equality |

`Foundation` is split finely (<=25 files/folder, 250 pure-line ceiling) so
`tests/unit/plugin/source_structure*.test.ts` passes. Keep it that way.

## WHERE TO LOOK (Need -> use this header)

| Need | Use this header |
|------|-----------------|
| Serialize a UObject/UProperty to JSON | `Reflection/McpPropertyReflection.h` (`McpPropertyReflection`) |
| Apply JSON onto a property/object | `Reflection/McpPropertyReflection.h` `ApplyJsonValueToProperty` / `ApplyJsonObjectToObject` |
| Property type name / support check | `Reflection/McpPropertyReflection.h` `GetPropertyTypeName` / `IsPropertyTypeSupported` |
| Dispatch an action in a handler | `HandlerUtils/McpHandlerUtils.h` `MCP_DISPATCH_ACTION` / `MCP_DISPATCH_SUBACTION` |
| Build an error response | `HandlerUtils/McpHandlerUtils.h` `MCP_ERROR_INVALID_PAYLOAD` / `MCP_ERROR_MISSING_PARAM` / `MCP_ERROR_NOT_FOUND` |
| Normalize/sanitize action + path strings | `HandlerUtils/McpHandlerUtilsActionsPaths.h` |
| Extract/coerce JSON fields | `HandlerUtils/McpHandlerUtilsJson.h` |
| Standard response builders | `HandlerUtils/McpHandlerUtilsResponses.h` |
| Transform / math helpers | `HandlerUtils/McpHandlerUtilsTransforms.h` |
| BP graph node helpers | `HandlerUtils/McpHandlerUtilsBlueprintGraph.h` |
| Resolve / validate `/Game` asset path | `BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetResolution.h` |
| Create assets / dirs / save registry | `BridgeHelpers/Assets/{AssetCreation,AssetDirectories,AssetSaveRegistry}.h` |
| Spawn an actor | `BridgeHelpers/Actors/McpAutomationBridgeHelpersActorSpawn.h` |
| Load / compile BP, SCS lookup, BP paths | `BridgeHelpers/Blueprints/{BlueprintAssetLoad,BlueprintCompilation,ScsLookup,BlueprintPaths}.h` |
| Read/apply a property (scalar/object/array) | `BridgeHelpers/Properties/{PropertyLookup,PropertyExport,NestedPropertyPath,ComponentLookup,PropertyApply*}.h` |
| Resolve a UClass by name | `BridgeHelpers/Reflection/McpAutomationBridgeHelpersClassResolution.h` |
| Build / verify response, capture output, JSON fields | `BridgeHelpers/Responses/{Responses,ResponseVerification,OutputCapture,JsonFields}.h` |
| Everything at once | `BridgeHelpers/McpAutomationBridgeHelpers.h` (umbrella) |

`Reflection/McpPropertyReflection.h` is THE UE-property <-> JSON boundary.
Every domain that touches properties funnels through it.

## THE SECURITY PRIMITIVES

- `McpSecureTokenCompare.h` -> `McpConstantTimeTokenEquals(A, B)`: XOR-accumulates
  the full UTF-8 span, folds length, no early exit. Called by BOTH the WebSocket
  bridge and native `/mcp`. Never replace with `==` / `Equals` (contract test fails).
- `BridgeHelpers/Security/McpAutomationBridgeHelpersProjectPaths.h`: confines paths
  to project roots (`/Game/...`, mount points), rejects `..`, `:`, `//`.
- `BridgeHelpers/Security/McpAutomationBridgeHelpersCommandValidation.h`: blocklist
  of `&&`, `||`, `;`, `|`, backtick, `<`, `>`, newline, CR for console/UBT args.
- `BridgeHelpers/Security/McpAutomationBridgeHelpersSafeOperationsFacade.h`: wraps
  hazardous editor ops. For save/load/delete crashes see `../Safety/AGENTS.md`
  (different directory; link, not copy).

## CONVENTIONS

- SCS rule: Blueprint component templates must be owned by SCS nodes via
  `SCS->CreateNode()` / `SCS->AddNode()`. Relevant to `Blueprint/` and
  `BridgeHelpers/Blueprints/ScsLookup`. Do not attach templates ad hoc.
- Adding a handler is a Core/Domains concern (queue + `MCP_DISPATCH_*`). This file
  only supplies the macros; the registration flow lives in `../Core/AGENTS.md`
  and `../Domains/AGENTS.md`.
- Every local `Mcp*` include must resolve (contract test). Include the umbrella
  `McpAutomationBridgeHelpers.h` unless you need a single shard.

## ANTI-PATTERNS

- Top one: writing a domain-local copy of a reflection/path/JSON/response helper
  that already lives here. Reuse it.
- Reimplementing `McpConstantTimeTokenEquals` with `==` / `FString::Equals`.
- Hand-rolling `/Game` path validation instead of `...ProjectPaths.h`.
- Skipping `...CommandValidation.h` on console-command strings.
- Building responses by hand when `McpHandlerUtilsResponses.h` exists.
