# Extending the MCP Automation Bridge plugin

How to add editor behaviour to the plugin in `plugins/McpAutomationBridge/`. Every capability a client sees is generated from the TypeScript capability records; the plugin implements them. The [Development](https://github.com/ChiR24/Unreal_mcp/wiki/Development) wiki page covers the TypeScript side, and the `AGENTS.md` file in each plugin folder holds that area's detailed rules.

## How a request reaches a handler

1. **Transport.** The WebSocket listener (`Private/Transport/`, used by the stdio server) or the native MCP server (`Private/MCP/`, whose gateway validates calls against the generated records in `Private/MCP/Generated/`).
2. **Pre-queue gate.** `Private/Core/Security/McpPrequeueGate` checks scope, consent, paths and quota on both transports before anything is queued. The plugin is the security authority; the TypeScript side only fails fast.
3. **Queue.** `QueueAutomationRequest()` appends the request, and the game thread drains up to 16 per tick in `ProcessPendingAutomationRequests()`. There is exactly one dequeuer.
4. **Handler table.** `InitializeHandlers()` in `Private/Core/Subsystem/McpAutomationBridgeSubsystemHandlerRegistration.cpp` maps each of the 23 parent tools, plus `console_command`, to a domain handler. `Route(Parent, Fallback, { FSubRoute... })` sends the sub-actions a sibling domain claims to that domain.
5. **Domain dispatcher.** `Handle<Domain>Action` in `Private/Domains/<Domain>/…Dispatch.cpp` reads `subAction` first, then `action`, and calls the leaf handler through a compare chain.
6. **Leaf handler.** It does the work and replies with `SendAutomationResponse` or `SendAutomationError`. The gateway projects the reply onto the record's declared output schema and returns it with a receipt.

## Adding an action

### 1. Write the capability record

In `src/tools/catalog/capabilities/records/<tool>/`, usually through `buildCoreRecord()`. Declare exactly the parameters the handler reads and the fields it returns (the gateway refuses anything undeclared), plus effect, scope, consent, cost, and 3 to 6 search topics phrased the way people ask. A variant of an existing action belongs in `records/folds/<tool>.folds.ts` as a new selector value.

Then regenerate:

```bash
npm run registry:generate
npm run registry:check
npm run manifest:check
```

This rewrites the TypeScript definitions, the gateway manifest, the native registry and schema shards under `Private/MCP/Generated/`, and the action reference. Never hand-edit a generated file.

### 2. Declare the handler

Add it to the matching `MCP_SUBSYSTEM_*_DECLARATIONS` list in `Public/McpAutomationBridgeSubsystem<Area>Declarations.h`:

```cpp
MCP_DECLARE_PAYLOAD_HANDLER(HandleControlActorAddTag); \
```

The macro declares `bool Name(const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)` on `UMcpAutomationBridgeSubsystem`.

### 3. Implement it

Put the body in a responsibility `.cpp` in `Private/Domains/<Domain>/`, next to related handlers. Adapted from `ControlActor/McpAutomationBridge_ControlActorTags.cpp`:

```cpp
bool UMcpAutomationBridgeSubsystem::HandleControlActorAddTag(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString TargetName;
  Payload->TryGetStringField(TEXT("actorName"), TargetName);
  FString TagValue;
  Payload->TryGetStringField(TEXT("tag"), TagValue);
  // ... resolve the actors into Targets ...

  FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Add Actor Tag")),
                                          EMcpMutationDurability::EditorStateOnly, Targets);
  for (UObject *Target : Targets) {
    AActor *Actor = CastChecked<AActor>(Target);
    Actor->Tags.AddUnique(FName(*TagValue));
    Actor->MarkPackageDirty();
  }

  TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
  Data->SetNumberField(TEXT("taggedCount"), Targets.Num());
  Transaction.DescribeInto(Data);
  SendAutomationResponse(Socket, RequestId, true, TEXT("Tag applied"), Data);
  return true;
}
```

Report failures with `SendAutomationError(Socket, RequestId, Message, Code)` and a SCREAMING_SNAKE code such as `INVALID_PAYLOAD`, `MISSING_PARAMETER` or `NOT_FOUND`.

### 4. Route it

Add the sub-action to the compare chain in the domain's `…Dispatch.cpp`:

```cpp
if (LowerSub == TEXT("add_tag"))
  return HandleControlActorAddTag(RequestId, Payload, RequestingSocket);
```

If the action belongs to a domain other than its parent tool's default one, add an `FSubRoute` for it in `InitializeHandlers()`.

### 5. Test it

- An integration case under `tests/mcp-tools/<category>/`; `npm run test:params` fails until every action and parameter is covered. See the [Testing Guide](testing-guide.md).
- `npm run test:unit`: the source-contract tests in `tests/unit/plugin/` read the C++ and enforce the rules below.
- A search phrasing in `tests/eval/corpus.data.ts` if the capability is hard to find, checked by `npm run eval:check`.
- A live run in the editor, through the `unreal` tool.

## Rules for plugin code

| Rule | Detail |
|------|--------|
| **Game thread only** | Editor work runs through the queue. Never call editor APIs from a socket thread, and never add a second drain path (`Public/McpQueueFairness.h`). |
| **Safe save, load and delete** | `McpSafeAssetSave`, `McpSafeLevelSave`, `McpSafeLoadMap` and `McpSafeDeleteFolder` instead of `UPackage::SavePackage()`, `FEditorFileUtils::SaveMap` or `UEditorAssetLibrary::DeleteDirectory` (`Private/Safety/`). |
| **Honest undo** | Wrap editor-state changes in `FMcpScopedEditorTransaction` (`Private/Foundation/`) with the right `EMcpMutationDurability`, and call `DescribeInto()` so the reply says whether undo reverts the change. Only `EditorStateOnly` opens a transaction; disk writes, builds and renders are not undoable. |
| **Blueprint components** | Owned by SCS nodes created with `SCS->CreateNode()` and `SCS->AddNode()` |
| **Promised behaviour is authored** | An action that promises gameplay logic writes it into the Blueprint with `McpBlueprintBehaviour::Author` and a recipe under `Resources/Recipes/<Domain>/`. See `Private/Domains/BlueprintGraph/Behaviour/AGENTS.md`. |
| **Thin dispatchers** | The dispatcher owns the unknown-action error. A leaf handler returns `false` instead of reporting one itself. |
| **Every engine from 5.0 to 5.8** | Guard version-specific APIs with `ENGINE_MAJOR_VERSION` / `ENGINE_MINOR_VERSION` checks or the macros in `Private/Core/Compatibility/McpVersionCompatibility.h`. No `ANY_PACKAGE`. |
| **Optional engine plugins** | Code that needs one compiles away through the `MCP_HAS_*` defines from `McpAutomationBridge.Build.cs` (`MCP_HAS_PCG`, `MCP_HAS_MOVIE_RENDER_PIPELINE`, `MCP_HAS_TAKE_RECORDER`, ...). A missing module must never break the build. |
| **Include what you use** | Non-unity and no-PCH builds compile each file alone, so each file includes what it uses, and every local `Mcp*` include must resolve |
| **Size ceilings** | At most 250 lines of code per file and 25 files per folder, with no `Common*` or `Part<N>` split files. Split by responsibility into a subfolder instead. |
| **Reuse Foundation** | Reflection, path, Blueprint, JSON, response and object-resolution helpers live in `Private/Foundation/`. Don't grow domain-local copies. |

## Where things live

All paths are under `plugins/McpAutomationBridge/Source/McpAutomationBridge/`.

| Folder | Holds |
|--------|-------|
| `Public/` | The subsystem, settings, connection manager and handler declarations |
| `Private/Core/` | Request queue, game-thread drain, handler table, pre-queue gate |
| `Private/Domains/` | The editor work, one folder per domain |
| `Private/Foundation/` | Shared helpers: reflection, paths, auth predicates, responses, transactions |
| `Private/MCP/` | Native MCP server and gateway; `Generated/` is generated from the records |
| `Private/Safety/` | Wrappers for hazardous editor operations |
| `Private/Transport/` | WebSocket listener, TLS, token auth, rate limits |
| `Private/Tests/` | Native automation tests |

The optional Fab adapter is a separate, delay-loaded module in `Source/McpAutomationBridgeFab/`.

## Related documents

- [Development](https://github.com/ChiR24/Unreal_mcp/wiki/Development) (wiki): the whole codebase, build and checks
- [Testing Guide](testing-guide.md): suites, cases and expectations
- [Security and receipts](security-and-receipts.md): scopes, consent, path gating, refusal codes
- [Protocol](protocol.md): transports, version negotiation, cancellation
