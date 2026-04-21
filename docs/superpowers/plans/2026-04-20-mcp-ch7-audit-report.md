# Ch7 StateTree Audit Report

Generated: 2026-04-20
Parent plan: `2026-04-20-mcp-ch7-state-tree.md`
UE verified: 5.7 (headers at `D:\Unreal\UE_5.7\Engine\Plugins\Runtime\StateTree`)

---

## 0. Module wiring (Build.cs)

`McpAutomationBridge.Build.cs:190-191`:
```
AddOptionalDynamicModule(Target, EngineDir, "StateTreeModule", "StateTreeModule");
AddOptionalDynamicModule(Target, EngineDir, "StateTreeEditorModule", "StateTreeEditorModule");
```

Also needed for 5.7 default schema (`UStateTreeComponentSchema`): the schema moved to `GameplayStateTreeModule`. The header is at `D:\Unreal\UE_5.7\Engine\Plugins\Runtime\GameplayStateTree\Source\GameplayStateTreeModule\Public\Components\StateTreeComponentSchema.h`.

**Gap**: `GameplayStateTreeModule` is NOT currently listed. The existing cpp guards schema creation behind `MCP_STATE_TREE_COMPONENT_SCHEMA_AVAILABLE` which appears to be unset for 5.7, so no schema is assigned today. To support `contextClass=UStateTreeComponentSchema`, add:
```csharp
AddOptionalDynamicModule(Target, EngineDir, "GameplayStateTreeModule", "GameplayStateTree");
```

Task 2 will handle this.

---

## 1. Existing handlers (C++) — `McpAutomationBridge_AIHandlers.cpp`

### 1a. `create_state_tree` — lines 1610-1697

**Params consumed**: `name`, `path` (default `/Game/AI/StateTrees`), `schemaType` (accepted but NOT used; only default branch).

**Flow**:
- `CreatePackage` + `NewObject<UStateTree>` + `NewObject<UStateTreeEditorData>` attached via `StateTree->EditorData = EditorData;`
- **Schema**: assigns `UStateTreeComponentSchema` only if `MCP_STATE_TREE_COMPONENT_SCHEMA_AVAILABLE` macro set. In 5.7, schema moved to GameplayStateTreeModule and this macro is 0 in cpp line 164 guard. So today 5.7 builds create StateTrees with no schema.
- `EditorData->AddRootState()` → "Root" added automatically.
- `McpSafeAssetSave(StateTree)`.

**Gaps**:
- `contextClass` param missing — cannot pick a different schema.
- Schema guard needs to be updated OR the module added. Simpler: load schema class by path at runtime (`LoadObject<UClass>(nullptr, *ContextClassPath)`) — no direct include required.

**Task 2 required**: YES.

### 1b. `add_state_tree_state` — lines 1700-1806

**Params consumed**: `stateTreePath`, `stateName`, `parentStateName` (default `Root`), `stateType` (default `State`).

**Flow**:
- Finds parent only at depth 1 (root subtrees + their immediate children). **Does not recurse deeper.**
- `EStateTreeStateType`: `State | Group | Linked | LinkedAsset`. Plan originally asked for `State | Subtree | Linked` — verified enum is: `State, Group, Linked, LinkedAsset, Subtree` (5 values per `StateTreeTypes.h:132`). Existing code recognizes `Group`, `Linked`, `LinkedAsset` but NOT `Subtree`.
- Uses `ParentState->AddChildState(FName, EStateTreeStateType)` — matches 5.7 header `StateTreeState.h:256`.

**Gaps**:
- Parent search only depth-1; Task 3 must deepen to full recursion.
- `Subtree` enum value not mapped (minor — subtrees are typically top-level via `AddSubTree`, but the current handler can't create subtrees at all; Task 3 should add this mapping, potentially calling `EditorData->AddSubTree(Name)` when `parentState` is empty/root and `stateType=Subtree`).
- Param name is `parentStateName`, plan specifies `parentState`. **Decision: accept both to avoid a breaking rename** (read both fields, preferring `parentState` if set).

**Task 3 required**: YES (recursion fix + `Subtree` + alias `parentState`).

### 1c. `add_state_tree_transition` — lines 1808-1922

**Params consumed**: `stateTreePath`, `fromState`, `toState`, `triggerType` (default `OnStateCompleted`).

**Flow**:
- Recursive `FindState` via nested TFunction — already correct shape. Walks `EditorData->SubTrees` then `Children` recursively.
- Maps trigger to `EStateTreeTransitionTrigger` (OnStateCompleted | OnStateFailed | OnTick | OnEvent). Full coverage.
- `SourceState->AddTransition(Trigger, EStateTreeTransitionType::GotoState, TargetState)` — matches 5.7 header `StateTreeState.h:321`.
- Saves via `McpSafeAssetSave`.

**Gaps**: Minor — no `eventTag` (for `OnEvent` triggers), no `delay`/`priority`. But since plan marks Task 4 as conditional and these are incremental rather than missing, this is safe-to-skip for Ch7's acceptance criteria.

**Task 4 decision**: **SKIP code changes**. Document verification only; add an optional commit for `eventTag`/`delay` parameters if desired, but not required for acceptance.

### 1d. `configure_state_tree_task` — lines 1924-2047

**Params consumed**: `stateTreePath`, `stateName`, `taskType` (accepted but unused), `selectionBehavior` (only in 5.6 and earlier — guarded out for 5.7 `ENGINE_MINOR_VERSION >= 7`).

**Flow**:
- Recursive `FindState`.
- For 5.6-: maps `selectionBehavior` string to `EStateTreeStateSelectionBehavior` enum.
- For 5.7+: suppresses the field (`(void)Behavior;`).
- Reports `FoundState->Tasks.Num()` in response.

**Reality check for 5.7**: `EStateTreeStateSelectionBehavior` STILL exists in 5.7 (`StateTreeTypes.h:152`) with values `None | TryEnterState | TrySelectChildrenInOrder | TrySelectChildrenAtRandom | TrySelectChildrenWithHighestUtility | ...`. The 5.7 guard was over-eager. However, fixing this is not strictly part of Ch7; it's a regression from an earlier migration. Task 5 will restore it while adding `taskProps`.

**Gaps**:
- Does NOT actually configure tasks — it only sets selection behavior. The name "configure_state_tree_task" is misleading.
- Does NOT add tasks.
- No `taskProps` reflection path.

**Task 5 required**: YES. Extend to:
- Re-enable `selectionBehavior` for 5.7 (the enum exists).
- Accept `taskProps: object` — writes into the task instance FInstancedStruct.
- Accept `taskIndex: number` (default 0) — which task in `State->Tasks` to configure.

**Task 6 decision**: YES. `configure_state_tree_task` does NOT add tasks today. A new `add_state_tree_task` action is needed that:
1. Loads `UStateTree`, finds state.
2. Accepts `stateTaskClass` — treated as a `UScriptStruct` path (C++ task) OR `UClass` path (Blueprint task via `UStateTreeTaskBlueprintBase`).
3. Appends `FStateTreeEditorNode` to `State->Tasks` with `Node.InitializeAs(TaskStruct)` (if `UScriptStruct`) or `InstanceObject = NewObject<UClass>` (if Blueprint).

---

## 2. TS handlers — `src/tools/handlers/ai-handlers.ts`

Lines 200-222:

```typescript
case 'create_state_tree':        // forwards: name (required)
case 'add_state_tree_state':     // forwards: stateTreePath, stateName (required)
case 'add_state_tree_transition':// forwards: stateTreePath, fromState, toState (required)
case 'configure_state_tree_task':// forwards: stateTreePath, stateName (required)
```

All cases forward the full `argsRecord` to C++ via `sendRequest()` (see line 39-49). So **any new field** added to the schema automatically reaches C++ without TS-side routing changes — only schema and unit tests need TS updates. C++ reads via `GetStringFieldAI`/`TryGetObjectField`.

**Existing forwarded fields that C++ today ignores**:
- `stateTaskClass` — in schema but unused by `configure_state_tree_task`. Task 6 will use it.
- `stateEvaluatorClass` — in schema but unused. Out of scope for Ch7.
- `transitionCondition` — in schema but unused by `add_state_tree_transition`. Skip per Task 4 decision.

---

## 3. 5.7 API signatures (verified from headers)

### `UStateTreeEditorData` (`StateTreeEditorData.h`)
- `TArray<TObjectPtr<UStateTreeState>> SubTrees` (line 418) — top-level states (Instanced).
- `UStateTreeState& AddSubTree(FName Name)` (line 229) — new top-level state.
- `UStateTreeState& AddRootState()` (line 242) — `AddSubTree("Root")`.
- `TObjectPtr<UStateTreeSchema> Schema` (line 375) — Instanced UPROPERTY. Assign via `NewObject<UStateTreeSchema>(EditorData, SchemaClass)`.
- `UStateTreeState* GetMutableStateByID(FGuid)` (line 145) — useful for GUID lookups, not needed here.
- `EStateTreeVisitor VisitHierarchy(TFunctionRef<...>)` (line 157) — for Tasks 7/8 walking, could simplify but hand-rolled recursion is clearer.

### `UStateTreeState` (`StateTreeState.h`)
- `FName Name`, `EStateTreeStateType Type`, `FGuid ID`, `TObjectPtr<UStateTreeState> Parent`.
- `TArray<TObjectPtr<UStateTreeState>> Children` (line 436, Instanced).
- `TArray<FStateTreeEditorNode> Tasks` (line 422).
- `FStateTreeEditorNode SingleTask` (line 430) — used when schema restricts single task; skip for now.
- `TArray<FStateTreeTransition> Transitions` (line 433).
- `UStateTreeState& AddChildState(FName, EStateTreeStateType = State)` (line 256).
- `template<T> TStateTreeEditorNode<T>& AddTask(TArgs&&...)` (line 294) — template-only; runtime equivalent is to construct `FStateTreeEditorNode` manually and `Tasks.Add(...)`.
- `FStateTreeTransition& AddTransition(Trigger, Type, InState)` (line 321).

### `FStateTreeEditorNode` (`StateTreeEditorNode.h`)
- `FInstancedStruct Node` (line 85) — THE task/condition/evaluator struct instance.
- `FInstancedStruct Instance` (line 88) — instance data (typed per `Node->GetInstanceDataType()`).
- `TObjectPtr<UObject> InstanceObject` (line 91) — for Blueprint tasks (UStateTreeTaskBlueprintBase).
- `FGuid ID` (line 101).

**How to add a task at runtime** (without templates):
```cpp
FStateTreeEditorNode& EditorNode = State->Tasks.AddDefaulted_GetRef();
EditorNode.ID = FGuid::NewGuid();
if (UScriptStruct* TaskStruct = LoadObject<UScriptStruct>(nullptr, *TaskPath))
{
    EditorNode.Node.InitializeAs(TaskStruct);  // FInstancedStruct::InitializeAs(UScriptStruct*)
    // Optionally init instance data:
    const FStateTreeNodeBase& Node = EditorNode.Node.GetMutable<FStateTreeNodeBase>();
    if (const UScriptStruct* InstanceType = Cast<const UScriptStruct>(Node.GetInstanceDataType()))
    {
        EditorNode.Instance.InitializeAs(InstanceType);
    }
}
else if (UClass* TaskClass = LoadObject<UClass>(nullptr, *TaskPath))
{
    // Blueprint task (UStateTreeTaskBlueprintBase subclass)
    EditorNode.InstanceObject = NewObject<UObject>(State, TaskClass);
}
```

**How to write task props via reflection**:
```cpp
// Use the Ch2 helper:
#include "MCP/Helpers/McpStructReflection.h"
// ...
FInstancedStruct& InstanceStruct = EditorNode.Instance; // or EditorNode.Node for node-level params
FString StructError;
McpStructReflection::SetStructFieldsFromJsonObject(
    InstanceStruct.GetScriptStruct(),
    InstanceStruct.GetMutableMemory(),
    *TaskPropsObj,
    StructError);
```

Note: `FInstancedStruct::GetMutableMemory()` returns `uint8*` in 5.7 (verified via `FInstancedStruct` header convention).

### `EStateTreeStateType` (`StateTreeTypes.h:133`)
Values: `State, Group, Linked, LinkedAsset, Subtree`.

**Important**: Plan proposed `'State' | 'Subtree' | 'Linked'`. Actual coverage should include all 5. Schema string enum for Task 3: `['State', 'Group', 'Linked', 'LinkedAsset', 'Subtree']`.

---

## 4. 补齐 task list (locked)

- [x] **Task 1** — this report.
- [ ] **Task 2** — `create_state_tree`: add `contextClass` (+ `schemaType` alias retained). Load `UClass` by path via `LoadObject<UClass>`; default to `/Script/GameplayStateTreeModule.StateTreeComponentSchema` if absent AND that class loads successfully; otherwise skip schema.
- [ ] **Task 3** — `add_state_tree_state`: (a) deepen parent search to full recursion; (b) map all 5 `EStateTreeStateType` values including `Subtree`; (c) when `stateType=Subtree` and `parentState` is empty/"Root" and current behavior would fall back to Root, instead call `EditorData->AddSubTree(Name)`; (d) accept both `parentState` and legacy `parentStateName`.
- [x] **Task 4** — **SKIPPED code**: `add_state_tree_transition` verified complete for core use case (all 4 triggers + recursive FindState + AddTransition(Trigger,Type,State)). Optional triggerTag/delay/priority not added — none were blocking acceptance per plan §Task 4.
- [ ] **Task 5** — `configure_state_tree_task`: (a) re-enable `selectionBehavior` for 5.7 (enum exists); (b) accept `taskProps: object` + `taskIndex: number` default 0; writes via `McpStructReflection` into `State->Tasks[taskIndex].Instance` (or `.Node` if no `Instance` script struct) — prefer `Instance` when valid.
- [ ] **Task 6** — NEW action `add_state_tree_task`: loads state, constructs `FStateTreeEditorNode` from `stateTaskClass` (UScriptStruct OR UClass), appends to `State->Tasks`, saves. Returns `taskIndex` + `taskId`.
- [ ] **Task 7** — NEW action `list_state_tree_states(stateTreePath)`: returns `{ stateTreeTree: { <subtreeName>: { type, children: {...} } } }` by walking `EditorData->SubTrees` and each state's `Children`.
- [ ] **Task 8** — NEW action `remove_state_tree_state(stateTreePath, stateName)`: recursive delete from parent's `Children`; top-level removal from `SubTrees`. Saves.

---

## 5. Risk notes

1. **`FInstancedStruct::GetMutableMemory()`** — API exists in 5.5+. If compile fails, fall back to `(uint8*)InstanceStruct.GetMutableMemory()` cast or use `InstanceStruct.GetMutable<T>()` when T is known. For reflection path we don't know T — so `GetMutableMemory()` is the correct API.
2. **`UStateTreeComponentSchema` load by path** — class path is `/Script/GameplayStateTreeModule.StateTreeComponentSchema` in 5.7. Must NOT add a hard `#include` since the plugin might be disabled; use `LoadObject<UClass>(nullptr, *Path)` which returns nullptr gracefully.
3. **Save during Blueprint task attach** — if `stateTaskClass` is a Blueprint (BP_Task_X), `NewObject<UObject>(State, TaskClass)` is correct (State as outer so it serializes with the StateTree).
4. **Subtree removal orphans** — Task 8 should NOT null-check `Parent` pointers on children; `TObjectPtr<UStateTreeState> Parent` is a soft reference. Just remove from parent's `Children` / `SubTrees`.

---

## 6. Deviations from plan

| Plan | Reality | Action |
|------|---------|--------|
| `stateType: 'State' \| 'Subtree' \| 'Linked'` | Enum has 5 values | Schema accepts all 5 |
| `parentState` param | Existing handler uses `parentStateName` | Accept both |
| `FStructView TaskView = EditorNode.GetMutableTaskInstance()` | No such method | Use `EditorNode.Instance.GetMutableMemory()` + `GetScriptStruct()` |
| `EditorData->SubTrees` is `TArray<UStateTreeState*>` | Actually `TArray<TObjectPtr<UStateTreeState>>` | Iterate directly; `TObjectPtr` converts implicitly to raw ptr |
| Plan assumes `UStateTreeSchema` is `Schema` UPROPERTY | ✓ Confirmed | Plan correct |
| `AddTransition(Trigger, Type, State)` | ✓ Signature matches | Plan correct |
| `configure_state_tree_task` adds task | It does NOT | Task 6 adds `add_state_tree_task` as separate action |
| Task 4 extension needed | Handler complete for core use case | **Skip Task 4 code changes** |
