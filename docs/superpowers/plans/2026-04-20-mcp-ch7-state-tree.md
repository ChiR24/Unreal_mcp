# Ch7 — `manage_ai` StateTree Completion (audit + 补齐)

> **Parent plan:** `2026-04-20-mcp-tier123-expansion.md`
> **Spec:** §4 Ch7
> **Depends on:** None directly (existing AI subsystem)
> **Estimated:** 1 day, 7 commits (1 audit + ~6 补齐)

**Goal:** Audit existing 4 StateTree actions in `manage_ai`, then extend/add as needed to cover: contextClass on create, stateType on add_state, taskProps on configure, plus new `add_state_tree_task`, `list_state_tree_states`, `remove_state_tree_state`.

---

## Task 1: Audit existing StateTree handlers (gate-task)

**Read-only** — produces an audit report that determines Tasks 2-7 concrete shape.

**Files:**
- Read: `plugins/.../Private/McpAutomationBridge_AIHandlers.cpp` (4 StateTree handler functions)
- Read: `src/tools/handlers/ai-handlers.ts` (TS switch cases)
- Read: `src/tools/consolidated-tool-definitions.ts` (manage_ai schema for StateTree fields)
- Read: `X:\Unreal_Engine\UE_5.7\Engine\Plugins\Runtime\StateTree\Source\StateTreeEditorModule\Public\StateTreeEditorData.h`
- Read: `X:\Unreal_Engine\UE_5.7\Engine\Plugins\Runtime\StateTree\Source\StateTreeEditorModule\Public\StateTreeState.h`

- [ ] **Step 1: Grep handlers file for StateTree functions**

```bash
grep -n "StateTree\|state_tree" plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_AIHandlers.cpp
```

Record function names, parameters consumed, return shape.

- [ ] **Step 2: Grep TS handler switch cases**

```bash
grep -n "state_tree" src/tools/handlers/ai-handlers.ts
```

Cross-reference with C++ side: are all TS-routed params actually read by C++?

- [ ] **Step 3: Read 5.7 StateTree editor headers (API signature capture)**

```bash
grep -n "class UStateTreeEditorData" X:/Unreal_Engine/UE_5.7/Engine/Plugins/Runtime/StateTree/Source/StateTreeEditorModule/Public/StateTreeEditorData.h
grep -n "AddSubTree\|AddState\|RemoveState\|Schema" X:/Unreal_Engine/UE_5.7/Engine/Plugins/Runtime/StateTree/Source/StateTreeEditorModule/Public/StateTreeEditorData.h
grep -n "class UStateTreeState" X:/Unreal_Engine/UE_5.7/Engine/Plugins/Runtime/StateTree/Source/StateTreeEditorModule/Public/StateTreeState.h
```

Note the exact signatures of:
- How states are added (method name, parameters)
- How state types (State / Subtree / Linked) are distinguished (`EStateTreeStateType` or similar)
- How tasks are attached (array property name on `UStateTreeState`, element type)
- How transitions are added
- How schema/context-class is referenced

- [ ] **Step 4: Produce audit report**

Write to `docs/superpowers/plans/2026-04-20-mcp-ch7-audit-report.md`:

```markdown
# Ch7 StateTree Audit Report

## Existing handlers (C++)

### HandleStateTreeCreate (or equivalent name)
- Params consumed: <list>
- Missing: contextClass (Y/N) — schema has stateTreePath but no schema class
- Next action: Task 2 (extend create_state_tree) / skip if already done

### HandleStateTreeAddState
- Params consumed: <list>
- Missing: stateType (Y/N), parentState (Y/N)
- Next action: Task 3

### HandleStateTreeAddTransition
- Params consumed: <list>
- Gaps: transitionCondition semantics undefined — determine if this is a trigger name, an expression, or a Tag
- Next action: Task 4 if clarification needed

### HandleConfigureStateTreeTask
- Params consumed: <list>
- Missing: taskProps JSON → reflection into task instance
- Note: whether this ADDS a task or only CONFIGURES an existing one
- Next action: Task 5 (extend) + maybe Task 6 (add_state_tree_task as new action if configure cannot add)

## API signatures (from 5.7 headers)

- UStateTreeEditorData::<method>(...) — <exact signature>
- UStateTreeState::Tasks member → TArray<FStateTreeEditorNode>
- EStateTreeStateType enum values: <list>
- Schema pointer: UStateTreeEditorData::Schema (UPROPERTY pointing to UStateTreeSchema)

## 补齐 task list

- [ ] Task 2: Extend create_state_tree → add contextClass param (required)
- [ ] Task 3: Extend add_state_tree_state → add stateType + parentState
- [ ] Task 4: Verify/extend add_state_tree_transition
- [ ] Task 5: Extend configure_state_tree_task → add taskProps
- [ ] Task 6: Add add_state_tree_task (if configure can't add)
- [ ] Task 7: Add list_state_tree_states
- [ ] Task 8: Add remove_state_tree_state
```

- [ ] **Step 5: Commit audit**
```bash
git add docs/superpowers/plans/2026-04-20-mcp-ch7-audit-report.md
git commit -m "docs(ch7): StateTree audit report

Maps existing 4 handlers' gaps against 5.7 API. Locks Tasks 2-8 shape."
```

---

## Task 2: Extend `create_state_tree` with `contextClass`

**Prerequisite:** Audit Task 1 committed.

**Files:**
- Modify: `src/tools/consolidated-tool-definitions.ts` (manage_ai inputSchema.properties — add `contextClass`)
- Modify: `src/tools/handlers/ai-handlers.ts` (pass contextClass through)
- Modify: `plugins/.../Private/McpAutomationBridge_AIHandlers.cpp` (HandleStateTreeCreate reads contextClass, sets UStateTreeEditorData->Schema)

- [ ] **Step 1: Schema addition**

In `consolidated-tool-definitions.ts` `manage_ai.inputSchema.properties`, add:
```typescript
contextClass: { type: 'string', description: 'Schema/context class (e.g. /Script/StateTreeModule.StateTreeComponentSchema).' },
```

- [ ] **Step 2: Unit test (mock)**

```typescript
// Extend src/tools/handlers/ai-handlers.test.ts (create if absent)
describe('manage_ai: create_state_tree with contextClass', () => {
  it('passes contextClass through to automation', async () => {
    const mockTools = { executeAutomation: vi.fn().mockResolvedValue({ success: true, stateTreePath: '/Game/ST' }) };
    await handleAITools(
      'create_state_tree',
      { name: 'ST_Test', path: '/Game', contextClass: '/Script/StateTreeModule.StateTreeComponentSchema' } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(mockTools.executeAutomation).toHaveBeenCalledWith('create_state_tree', expect.objectContaining({
      contextClass: '/Script/StateTreeModule.StateTreeComponentSchema'
    }));
  });
});
```

- [ ] **Step 3: TS handler extension**

In `ai-handlers.ts`'s `create_state_tree` case, ensure `contextClass` from args is forwarded in the `sendRequest` call (exact pattern depends on how the file structures forwarding; consult Task 1 audit).

Run test: PASS.

- [ ] **Step 4: C++ handler extension**

In `McpAutomationBridge_AIHandlers.cpp`, in `HandleStateTreeCreate`:

```cpp
// After creating UStateTree asset:
FString ContextClassPath;
Params->TryGetStringField(TEXT("contextClass"), ContextClassPath);
UClass* SchemaClass = nullptr;
if (!ContextClassPath.IsEmpty())
{
    SchemaClass = LoadObject<UClass>(nullptr, *ContextClassPath);
}
if (!SchemaClass)
{
    SchemaClass = UStateTreeComponentSchema::StaticClass(); // sensible default
}

// Assuming EditorData property chain (verified per audit):
if (UStateTree* ST = Cast<UStateTree>(NewAsset))
{
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(ST->EditorData);
    if (!EditorData)
    {
        EditorData = NewObject<UStateTreeEditorData>(ST, UStateTreeEditorData::StaticClass(), NAME_None, RF_Transactional);
        ST->EditorData = EditorData;
    }
    EditorData->Schema = NewObject<UStateTreeSchema>(EditorData, SchemaClass);
}
```

(The exact property/method names must be confirmed via Task 1 audit — this code is the expected shape.)

- [ ] **Step 5: Integration**
```javascript
{ scenario: 'StateTree: create with context class', toolName: 'manage_ai',
  arguments: { action: 'create_state_tree', name: 'ST_Ch7Test', path: '/Game/DataTest', contextClass: '/Script/StateTreeModule.StateTreeComponentSchema' },
  expected: 'success|already exists' },
```

- [ ] **Step 6: Compile + run**. Expected PASS.

- [ ] **Step 7: Commit**
```bash
git add -u
git commit -m "feat(manage_ai): create_state_tree accepts contextClass

Sets UStateTreeEditorData::Schema to the provided class. Defaults to
UStateTreeComponentSchema if contextClass is omitted."
```

---

## Task 3: Extend `add_state_tree_state` with `stateType` + `parentState`

**Files:** Same as Task 2 (schema + ai-handlers.ts + AIHandlers.cpp)

- [ ] **Step 1: Schema additions**

```typescript
stateType: { type: 'string', enum: ['State', 'Subtree', 'Linked'], description: 'StateTree state type (defaults to State).' },
parentState: { type: 'string', description: 'Parent state name for nested states (defaults to root).' },
```

- [ ] **Step 2: Unit test**

```typescript
describe('manage_ai: add_state_tree_state with stateType', () => {
  it('forwards stateType and parentState', async () => {
    const mockTools = { executeAutomation: vi.fn().mockResolvedValue({ success: true }) };
    await handleAITools(
      'add_state_tree_state',
      { stateTreePath: '/Game/ST', stateName: 'Combat', stateType: 'Subtree', parentState: 'Root' } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(mockTools.executeAutomation).toHaveBeenCalledWith('add_state_tree_state', expect.objectContaining({
      stateType: 'Subtree', parentState: 'Root'
    }));
  });
});
```
Run: FAIL.

- [ ] **Step 3: TS handler patch**

Ensure `stateType` and `parentState` are forwarded from args.

Run test: PASS.

- [ ] **Step 4: C++ handler patch**

```cpp
FString StateTypeStr = TEXT("State");
Params->TryGetStringField(TEXT("stateType"), StateTypeStr);
FString ParentStateName;
Params->TryGetStringField(TEXT("parentState"), ParentStateName);

// Resolve parent
UStateTreeEditorData* EditorData = /* ... */;
UStateTreeState* ParentState = EditorData->GetRootState();
if (!ParentStateName.IsEmpty())
{
    // Recursively find by name (helper below)
    ParentState = FindStateTreeStateByName(EditorData, ParentStateName);
    if (!ParentState)
    {
        // return NotFound error
    }
}

// Add child state
UStateTreeState* NewState = NewObject<UStateTreeState>(EditorData, UStateTreeState::StaticClass(), NAME_None, RF_Transactional);
NewState->Name = FName(*StateName);

if (StateTypeStr.Equals(TEXT("Subtree"), ESearchCase::IgnoreCase))
    NewState->Type = EStateTreeStateType::Subtree;
else if (StateTypeStr.Equals(TEXT("Linked"), ESearchCase::IgnoreCase))
    NewState->Type = EStateTreeStateType::Linked;
else
    NewState->Type = EStateTreeStateType::State;

ParentState->Children.Add(NewState);
```

Helper (add to same file):
```cpp
static UStateTreeState* FindStateTreeStateByName(UStateTreeEditorData* EditorData, const FString& Name)
{
    if (!EditorData) return nullptr;
    // Walk SubTrees -> Children recursively
    // ... (implementation depends on 5.7 data shape, confirmed in audit)
    return nullptr;
}
```

(Per audit: if `EditorData->SubTrees` is `TArray<UStateTreeState*>`, iterate; if it's a tree root, walk `Children`. Use the audit-discovered shape.)

- [ ] **Step 5: Integration**
```javascript
{ scenario: 'StateTree: add Root state', toolName: 'manage_ai',
  arguments: { action: 'add_state_tree_state', stateTreePath: '/Game/DataTest/ST_Ch7Test', stateName: 'Root', stateType: 'State' },
  expected: 'success|already exists' },
{ scenario: 'StateTree: add Combat subtree under Root', toolName: 'manage_ai',
  arguments: { action: 'add_state_tree_state', stateTreePath: '/Game/DataTest/ST_Ch7Test', stateName: 'Combat', stateType: 'Subtree', parentState: 'Root' },
  expected: 'success|already exists' },
```

- [ ] **Step 6: Compile + run**. PASS.

- [ ] **Step 7: Commit**
```bash
git add -u
git commit -m "feat(manage_ai): add_state_tree_state accepts stateType + parentState

EStateTreeStateType enum mapped from schema string. parentState
resolved via recursive Name lookup; omit for root."
```

---

## Task 4: Verify/extend `add_state_tree_transition`

- [ ] **Step 1: Per audit, determine if transition handler is complete**

If complete: skip Task 4 entirely (commit a `docs/CHANGELOG.md` entry noting verification).
If schema lacks trigger specifics: add `triggerTag` and `delay` params per 5.7 transition struct (`FStateTreeTransition`).

Schema (if extension needed):
```typescript
triggerTag: { type: 'string', description: 'GameplayTag triggering the transition.' },
delay: { type: 'number', description: 'Delay in seconds before transition fires.' },
```

- [ ] **Step 2: Unit test + handler patch (if needed)** — follow same TDD pattern as Tasks 2/3.

- [ ] **Step 3: Commit**
```bash
git add -u
git commit -m "feat(manage_ai): extend add_state_tree_transition with triggerTag/delay

(or) docs(manage_ai): verify add_state_tree_transition already complete"
```

---

## Task 5: Extend `configure_state_tree_task` with `taskProps`

- [ ] **Step 1: Schema**

```typescript
taskProps: { type: 'object', additionalProperties: true, description: 'Property values for the task instance (written via reflection).' },
```

- [ ] **Step 2: Unit test**

```typescript
describe('manage_ai: configure_state_tree_task with taskProps', () => {
  it('forwards taskProps', async () => {
    const mockTools = { executeAutomation: vi.fn().mockResolvedValue({ success: true }) };
    await handleAITools(
      'configure_state_tree_task',
      { stateTreePath: '/Game/ST', stateName: 'Combat', stateTaskClass: '/Script/Game.BTTask_MoveTo', taskProps: { Distance: 500 } } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(mockTools.executeAutomation).toHaveBeenCalledWith('configure_state_tree_task', expect.objectContaining({
      taskProps: { Distance: 500 }
    }));
  });
});
```
Run: FAIL.

- [ ] **Step 3: TS handler patch** — forward `taskProps`.
Run test: PASS.

- [ ] **Step 4: C++ handler patch**

```cpp
#include "MCP/Helpers/McpStructReflection.h"

// After resolving the task instance (FStateTreeEditorNode or similar):
const TSharedPtr<FJsonObject>* TaskPropsObj = nullptr;
if (Params->TryGetObjectField(TEXT("taskProps"), TaskPropsObj) && TaskPropsObj)
{
    // FStateTreeEditorNode wraps an FInstancedStruct holding the task
    // Access: EditorNode.GetMutableTaskInstance() or similar
    FStructView TaskView = /* resolve via audit */;
    FString StructError;
    McpStructReflection::SetStructFieldsFromJsonObject(
        TaskView.GetScriptStruct(), TaskView.GetMemory(),
        *TaskPropsObj, StructError);
}
```

- [ ] **Step 5: Integration**
```javascript
{ scenario: 'StateTree: configure task with props', toolName: 'manage_ai',
  arguments: { action: 'configure_state_tree_task', stateTreePath: '/Game/DataTest/ST_Ch7Test', stateName: 'Combat', stateTaskClass: '/Script/StateTreeTestSuite.TestTask_Stand', taskProps: { Duration: 2.0 } },
  expected: 'success|not found' },
```

(Use any actual StateTree task class available in 5.7; adjust path to a real one found during audit.)

- [ ] **Step 6: Compile + run**. Expected PASS if task class exists.

- [ ] **Step 7: Commit**
```bash
git add -u
git commit -m "feat(manage_ai): configure_state_tree_task accepts taskProps

Uses McpStructReflection to write JSON fields into the FStructView
backing the task instance. Unknown fields cause InvalidParams error."
```

---

## Task 6 (conditional): Add `add_state_tree_task` action

**Only if audit finds that `configure_state_tree_task` cannot create a new task (only modifies existing).**

- [ ] **Step 1: Schema — add to action enum + no new params beyond existing**

```typescript
'add_state_tree_task'
```

- [ ] **Step 2: Unit test**

```typescript
describe('manage_ai: add_state_tree_task', () => {
  it('forwards task class to add on state', async () => {
    const mockTools = { executeAutomation: vi.fn().mockResolvedValue({ success: true }) };
    await handleAITools(
      'add_state_tree_task',
      { stateTreePath: '/Game/ST', stateName: 'Combat', stateTaskClass: '/Script/Game.BTTask_MoveTo' } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(mockTools.executeAutomation).toHaveBeenCalledWith('add_state_tree_task', expect.anything());
  });
});
```
Run: FAIL.

- [ ] **Step 3: TS handler case**

```typescript
case 'add_state_tree_task': {
  const stateTreePath = argsRecord.stateTreePath as string | undefined;
  const stateName = argsRecord.stateName as string | undefined;
  const stateTaskClass = argsRecord.stateTaskClass as string | undefined;
  if (!stateTreePath) throw new Error('Missing required parameter: stateTreePath');
  if (!stateName) throw new Error('Missing required parameter: stateName');
  if (!stateTaskClass) throw new Error('Missing required parameter: stateTaskClass');
  const res = await executeAutomationRequest(tools, 'add_state_tree_task', { stateTreePath, stateName, stateTaskClass });
  return cleanObject(res as Record<string, unknown>) as Record<string, unknown>;
}
```
Run: PASS.

- [ ] **Step 4: C++ handler**

```cpp
TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleStateTreeAddTask(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    FString STPath, StateName, TaskClassPath;
    if (!Params->TryGetStringField(TEXT("stateTreePath"), STPath) ||
        !Params->TryGetStringField(TEXT("stateName"), StateName) ||
        !Params->TryGetStringField(TEXT("stateTaskClass"), TaskClassPath))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Missing required param"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }
    UStateTree* ST = LoadObject<UStateTree>(nullptr, *STPath);
    UScriptStruct* TaskStruct = LoadObject<UScriptStruct>(nullptr, *TaskClassPath);
    if (!ST || !TaskStruct)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("StateTree or task class not found"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }
    UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(ST->EditorData);
    if (!EditorData)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("No EditorData"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("ConflictState"));
        return Response;
    }
    UStateTreeState* State = FindStateTreeStateByName(EditorData, StateName);
    if (!State)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("State not found: %s"), *StateName));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }

    // Create task node
    FStateTreeEditorNode NewTaskNode;
    NewTaskNode.Node.InitializeAs(TaskStruct); // FInstancedStruct::InitializeAs
    State->Tasks.Add(NewTaskNode);

    ST->MarkPackageDirty();
    McpSafeAssetSave(ST);

    Response->SetBoolField(TEXT("success"), true);
    return Response;
}
```

Register: `add_state_tree_task`.

- [ ] **Step 5: Integration**
```javascript
{ scenario: 'StateTree: add task to Combat', toolName: 'manage_ai',
  arguments: { action: 'add_state_tree_task', stateTreePath: '/Game/DataTest/ST_Ch7Test', stateName: 'Combat', stateTaskClass: '/Script/StateTreeTestSuite.TestTask_Stand' },
  expected: 'success|not found' },
```

- [ ] **Step 6: Compile + run**. PASS.

- [ ] **Step 7: Commit**
```bash
git add -u
git commit -m "feat(manage_ai): add add_state_tree_task action

Creates FStateTreeEditorNode wrapping FInstancedStruct of the task
and appends to UStateTreeState::Tasks. configure_state_tree_task
can then set props on it."
```

---

## Task 7: Add `list_state_tree_states` action

- [ ] **Step 1: Schema — add to action enum + output**

```typescript
'list_state_tree_states'
// Output:
stateTreeTree: { type: 'object', additionalProperties: true, description: 'Nested states hierarchy {name: {children: {...}}}.' }
```

- [ ] **Step 2: Unit test**

```typescript
describe('manage_ai: list_state_tree_states', () => {
  it('returns tree hierarchy', async () => {
    const mockTools = { executeAutomation: vi.fn().mockResolvedValue({
      success: true, stateTreeTree: { Root: { children: { Combat: {} } } }
    }) };
    const res = await handleAITools(
      'list_state_tree_states',
      { stateTreePath: '/Game/ST' } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(res.stateTreeTree).toBeDefined();
  });
});
```
Run: FAIL.

- [ ] **Step 3: TS handler case**

```typescript
case 'list_state_tree_states': {
  const stateTreePath = argsRecord.stateTreePath as string | undefined;
  if (!stateTreePath) throw new Error('Missing required parameter: stateTreePath');
  const res = await executeAutomationRequest(tools, 'list_state_tree_states', { stateTreePath });
  return cleanObject(res as Record<string, unknown>) as Record<string, unknown>;
}
```
Run: PASS.

- [ ] **Step 4: C++ handler**

```cpp
static TSharedPtr<FJsonObject> StateToJson(UStateTreeState* State)
{
    TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
    if (!State) return Obj;
    Obj->SetStringField(TEXT("type"),
        State->Type == EStateTreeStateType::Subtree ? TEXT("Subtree") :
        State->Type == EStateTreeStateType::Linked ? TEXT("Linked") : TEXT("State"));
    TSharedPtr<FJsonObject> Children = MakeShared<FJsonObject>();
    for (UStateTreeState* Child : State->Children)
    {
        if (Child) Children->SetObjectField(Child->Name.ToString(), StateToJson(Child));
    }
    Obj->SetObjectField(TEXT("children"), Children);
    return Obj;
}

TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleStateTreeListStates(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    FString STPath;
    if (!Params->TryGetStringField(TEXT("stateTreePath"), STPath))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Missing stateTreePath"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }
    UStateTree* ST = LoadObject<UStateTree>(nullptr, *STPath);
    UStateTreeEditorData* EditorData = ST ? Cast<UStateTreeEditorData>(ST->EditorData) : nullptr;
    if (!EditorData)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("StateTree or EditorData not found"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }

    TSharedPtr<FJsonObject> TreeObj = MakeShared<FJsonObject>();
    // For each sub-tree root:
    for (UStateTreeState* Root : EditorData->SubTrees)
    {
        if (Root) TreeObj->SetObjectField(Root->Name.ToString(), StateToJson(Root));
    }

    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("stateTreeTree"), TreeObj);
    return Response;
}
```

Register: `list_state_tree_states`.

- [ ] **Step 5: Integration**
```javascript
{ scenario: 'StateTree: list states', toolName: 'manage_ai',
  arguments: { action: 'list_state_tree_states', stateTreePath: '/Game/DataTest/ST_Ch7Test' },
  expected: 'success' },
```

- [ ] **Step 6: Compile + run**. Expected PASS with tree containing Root.Combat.

- [ ] **Step 7: Commit**
```bash
git add -u
git commit -m "feat(manage_ai): add list_state_tree_states action

Returns nested {name: {type, children}} hierarchy from EditorData->SubTrees."
```

---

## Task 8: Add `remove_state_tree_state` action

- [ ] **Step 1: Schema — add to action enum**

```typescript
'remove_state_tree_state'
```

- [ ] **Step 2: Unit test**

```typescript
describe('manage_ai: remove_state_tree_state', () => {
  it('forwards stateTreePath + stateName', async () => {
    const mockTools = { executeAutomation: vi.fn().mockResolvedValue({ success: true }) };
    await handleAITools(
      'remove_state_tree_state',
      { stateTreePath: '/Game/ST', stateName: 'Combat' } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(mockTools.executeAutomation).toHaveBeenCalledWith('remove_state_tree_state', expect.anything());
  });
});
```
Run: FAIL.

- [ ] **Step 3: TS handler case**

```typescript
case 'remove_state_tree_state': {
  const stateTreePath = argsRecord.stateTreePath as string | undefined;
  const stateName = argsRecord.stateName as string | undefined;
  if (!stateTreePath) throw new Error('Missing required parameter: stateTreePath');
  if (!stateName) throw new Error('Missing required parameter: stateName');
  const res = await executeAutomationRequest(tools, 'remove_state_tree_state', { stateTreePath, stateName });
  return cleanObject(res as Record<string, unknown>) as Record<string, unknown>;
}
```
Run: PASS.

- [ ] **Step 4: C++ handler**

```cpp
TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleStateTreeRemoveState(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    FString STPath, StateName;
    if (!Params->TryGetStringField(TEXT("stateTreePath"), STPath) ||
        !Params->TryGetStringField(TEXT("stateName"), StateName))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Missing required param"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }
    UStateTree* ST = LoadObject<UStateTree>(nullptr, *STPath);
    UStateTreeEditorData* EditorData = ST ? Cast<UStateTreeEditorData>(ST->EditorData) : nullptr;
    if (!EditorData)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("StateTree not found"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }

    // Find and remove from parent's Children
    auto RemoveRecursive = [&](auto& Self, UStateTreeState* Parent) -> bool
    {
        if (!Parent) return false;
        for (int32 i = 0; i < Parent->Children.Num(); ++i)
        {
            UStateTreeState* Child = Parent->Children[i];
            if (Child && Child->Name == FName(*StateName))
            {
                Parent->Children.RemoveAt(i);
                return true;
            }
            if (Child && Self(Self, Child)) return true;
        }
        return false;
    };

    bool bRemoved = false;
    for (UStateTreeState* Root : EditorData->SubTrees)
    {
        if (Root && Root->Name == FName(*StateName))
        {
            // Remove SubTree root
            EditorData->SubTrees.Remove(Root);
            bRemoved = true;
            break;
        }
        if (Root && RemoveRecursive(RemoveRecursive, Root))
        {
            bRemoved = true;
            break;
        }
    }

    if (!bRemoved)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("State not found: %s"), *StateName));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }

    ST->MarkPackageDirty();
    McpSafeAssetSave(ST);
    Response->SetBoolField(TEXT("success"), true);
    return Response;
}
```

Register: `remove_state_tree_state`.

- [ ] **Step 5: Integration**
```javascript
{ scenario: 'StateTree: remove Combat', toolName: 'manage_ai',
  arguments: { action: 'remove_state_tree_state', stateTreePath: '/Game/DataTest/ST_Ch7Test', stateName: 'Combat' },
  expected: 'success' },
```

- [ ] **Step 6: Compile + run**. Expected PASS; subsequent `list_state_tree_states` shows tree without Combat.

- [ ] **Step 7: Commit**
```bash
git add -u
git commit -m "feat(manage_ai): add remove_state_tree_state action

Recursively searches SubTrees and Children for the named state and
removes from its parent. Top-level SubTree removals handled explicitly."
```

---

## Acceptance Checklist (Ch7)

- [ ] Audit report committed and reviewed
- [ ] 6-7 commits for 补齐 tasks (count depends on whether Task 4 needed code)
- [ ] Unit tests pass
- [ ] 7+ integration scenarios pass
- [ ] `ST_Ch7Test` end state: root-only tree after Task 8 runs
- [ ] `EditorRefreshGameplayTagTree` NOT needed (no tag changes)
- [ ] UE 5.7 plugin compiles with `StateTreeModule` + `StateTreeEditorModule` deps

---

## Notes for Subagent

- **API signature uncertainty is the primary risk** — every task's C++ code above uses the *expected* 5.7 API shape. **You MUST run Task 1's audit first** and correct any divergence before Tasks 2-8.
- **`FInstancedStruct` vs `FStructView`** — UE 5.3+ introduced `FInstancedStruct` for task serialization. 5.7 uses this. The `TaskView = EditorNode.GetMutableTaskInstance()` pseudo-call in Task 5 maps to an `FInstancedStruct::GetMutableValue<T>()` or `GetMutableMemory()` API — confirm exact method from header.
- **Task 4 is optional** — if audit shows transitions already work, skip to Task 5 and note in the audit report.
- **`UStateTreeComponentSchema` is the default context class** — if your war project uses a custom schema subclass, pass it via `contextClass` explicitly.
