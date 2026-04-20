# Ch1 — `manage_blueprint` Patches (reparent + interfaces)

> **Parent plan:** `2026-04-20-mcp-tier123-expansion.md`
> **Spec:** `docs/superpowers/specs/2026-04-20-mcp-tier123-expansion-design.md` §4 Ch1
> **Depends on:** Task 0 (shared scaffolding) merged
> **Estimated:** 0.5 day, 5 commits

**Goal:** Add 4 actions to `manage_blueprint` — `set_parent_class`, `add_interface`, `remove_interface`, `list_interfaces`. Audit existing `add_event` for event dispatcher delegate signature support.

**Architecture:** Pure extension of existing `manage_blueprint` tool. No new C++ helpers needed (uses `FBlueprintEditorUtils` directly).

---

## Task 1: Add `list_interfaces` action (audit-first, easiest)

**Files:**
- Modify: `src/tools/consolidated-tool-definitions.ts:189-200` (manage_blueprint action enum + schema)
- Modify: `src/tools/handlers/blueprint-handlers.ts` (add `case 'list_interfaces'`)
- Modify: `plugins/.../Private/McpAutomationBridge_BlueprintHandlers.cpp` (add handler function)
- Modify: `plugins/.../Private/McpAutomationBridgeSubsystem.cpp` (register handler)
- Create: `src/tools/handlers/blueprint-handlers.test.ts` (extend existing test file if present)
- Modify: `tests/integration.mjs` (add scenario)

- [ ] **Step 1: Extend TS action enum**

Open `src/tools/consolidated-tool-definitions.ts`, locate the `manage_blueprint` action enum (search for `'create_enum', 'create_struct', 'modify_enum'`). Append:

```typescript
'set_parent_class', 'add_interface', 'remove_interface', 'list_interfaces'
```

Into the existing enum array (on the same line/block; maintain alphabetical grouping if present).

- [ ] **Step 2: Add input/output schema fields**

In the same file's `manage_blueprint.inputSchema.properties`, add (near existing `blueprintPath`):

```typescript
parentClass: { type: 'string', description: 'Fully-qualified parent class path (e.g. "/Script/Engine.Pawn" or "/Game/BP_Base.BP_Base_C").' },
interfacePath: { type: 'string', description: 'Interface blueprint path for add_interface/remove_interface.' },
```

In `outputSchema.properties`, add:
```typescript
currentInterfaces: { type: 'array', items: { type: 'string' }, description: 'Implemented interface class paths after operation.' },
oldParent: { type: 'string', description: 'Previous parent class path (for set_parent_class).' },
newParent: { type: 'string', description: 'New parent class path (for set_parent_class).' },
```

- [ ] **Step 3: Write failing unit test**

Create or extend `src/tools/handlers/blueprint-handlers.test.ts`:

```typescript
import { describe, it, expect, vi } from 'vitest';
import { handleBlueprintTools } from './blueprint-handlers.js';

describe('blueprint-handlers: list_interfaces', () => {
  it('forwards list_interfaces action to automation with blueprintPath', async () => {
    const mockTools = {
      executeAutomation: vi.fn().mockResolvedValue({
        success: true,
        interfaces: ['/Game/IF_Test.IF_Test_C']
      })
    };
    const res = await handleBlueprintTools(
      'list_interfaces',
      { blueprintPath: '/Game/BP_Test' } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(res.success).toBe(true);
    expect(res.interfaces).toEqual(['/Game/IF_Test.IF_Test_C']);
    expect(mockTools.executeAutomation).toHaveBeenCalledWith(
      'blueprint_list_interfaces',
      expect.objectContaining({ blueprintPath: '/Game/BP_Test' })
    );
  });
});
```

Run:
```bash
npx vitest run src/tools/handlers/blueprint-handlers.test.ts
```
Expected: **FAIL** — handler has no `list_interfaces` case.

- [ ] **Step 4: Implement TS handler case**

In `src/tools/handlers/blueprint-handlers.ts`, before the `default:` case of the action switch, add:

```typescript
case 'list_interfaces': {
  const blueprintPath = argsTyped.blueprintPath || (argsRecord.path as string | undefined);
  if (!blueprintPath || typeof blueprintPath !== 'string') {
    throw new Error('Missing required parameter: blueprintPath');
  }
  const res = await executeAutomationRequest(tools, 'blueprint_list_interfaces', { blueprintPath });
  return cleanObject(res as Record<string, unknown>) as Record<string, unknown>;
}
```

Run unit test again:
```bash
npx vitest run src/tools/handlers/blueprint-handlers.test.ts
```
Expected: **PASS**.

- [ ] **Step 5: Implement C++ handler**

In `plugins/.../Private/McpAutomationBridge_BlueprintHandlers.cpp`, add a new function (follow the file's existing handler style; look at e.g. `HandleBlueprintCompile` for template):

```cpp
#include "Engine/Blueprint.h"
#include "Interfaces/Interface.h"
#include "Kismet2/BlueprintEditorUtils.h"

TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleBlueprintListInterfaces(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    FString BlueprintPath;
    if (!Params->TryGetStringField(TEXT("blueprintPath"), BlueprintPath))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Missing blueprintPath"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }

    UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!BP)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }

    TArray<TSharedPtr<FJsonValue>> InterfacePaths;
    for (const FBPInterfaceDescription& Iface : BP->ImplementedInterfaces)
    {
        if (Iface.Interface)
        {
            InterfacePaths.Add(MakeShared<FJsonValueString>(Iface.Interface->GetPathName()));
        }
    }
    Response->SetBoolField(TEXT("success"), true);
    Response->SetArrayField(TEXT("interfaces"), InterfacePaths);
    return Response;
}
```

Register in `InitializeHandlers()` of the subsystem (search for existing `"blueprint_compile"` registration and follow the same style):

```cpp
HandlerMap.Add(TEXT("blueprint_list_interfaces"),
    [this](const TSharedPtr<FJsonObject>& Params) { return HandleBlueprintListInterfaces(Params); });
```

Declare in subsystem header or the appropriate private section.

- [ ] **Step 6: Add integration test case**

In `tests/integration.mjs`, append (near existing Blueprint scenarios):

```javascript
{ scenario: 'Blueprint: list interfaces on fresh BP', toolName: 'manage_blueprint',
  arguments: { action: 'list_interfaces', blueprintPath: `${TEST_FOLDER}/BP_IntegrationTest` },
  expected: 'success' },
```

- [ ] **Step 7: Compile and run integration test (requires UE Editor running)**

```bash
npm run build:core
# (plugin rebuilt via UE editor auto-reload or manual UBT build)
npm test
```
Expected: `Blueprint: list interfaces on fresh BP` → PASS with `success: true, interfaces: []`.

- [ ] **Step 8: Commit**

```bash
git add src/tools/consolidated-tool-definitions.ts src/tools/handlers/blueprint-handlers.ts src/tools/handlers/blueprint-handlers.test.ts plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_BlueprintHandlers.cpp plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridgeSubsystem.cpp tests/integration.mjs
git commit -m "feat(manage_blueprint): add list_interfaces action

Returns array of interface class paths implemented by the blueprint.
No engine API surface changes — reads BP->ImplementedInterfaces directly."
```

---

## Task 2: Add `add_interface` action

**Files:** same as Task 1 (extend each)

- [ ] **Step 1: Write failing unit test**

Extend `blueprint-handlers.test.ts`:

```typescript
describe('blueprint-handlers: add_interface', () => {
  it('forwards add_interface with blueprintPath and interfacePath', async () => {
    const mockTools = {
      executeAutomation: vi.fn().mockResolvedValue({
        success: true,
        currentInterfaces: ['/Game/IF_Test.IF_Test_C']
      })
    };
    const res = await handleBlueprintTools(
      'add_interface',
      { blueprintPath: '/Game/BP_Test', interfacePath: '/Game/IF_Test.IF_Test_C' } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(res.success).toBe(true);
    expect(res.currentInterfaces).toEqual(['/Game/IF_Test.IF_Test_C']);
  });
  it('throws on missing interfacePath', async () => {
    const mockTools = { executeAutomation: vi.fn() };
    await expect(handleBlueprintTools(
      'add_interface',
      { blueprintPath: '/Game/BP_Test' } as unknown as Record<string, unknown>,
      mockTools as never
    )).rejects.toThrow(/interfacePath/);
  });
});
```

Run: expect FAIL.

- [ ] **Step 2: Implement TS handler case**

```typescript
case 'add_interface': {
  const blueprintPath = argsTyped.blueprintPath || (argsRecord.path as string | undefined);
  const interfacePath = argsRecord.interfacePath as string | undefined;
  if (!blueprintPath || typeof blueprintPath !== 'string') {
    throw new Error('Missing required parameter: blueprintPath');
  }
  if (!interfacePath || typeof interfacePath !== 'string') {
    throw new Error('Missing required parameter: interfacePath');
  }
  const res = await executeAutomationRequest(tools, 'blueprint_add_interface', { blueprintPath, interfacePath });
  return cleanObject(res as Record<string, unknown>) as Record<string, unknown>;
}
```

Run unit test: PASS.

- [ ] **Step 3: Implement C++ handler**

```cpp
TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleBlueprintAddInterface(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    FString BlueprintPath, InterfacePath;
    if (!Params->TryGetStringField(TEXT("blueprintPath"), BlueprintPath) ||
        !Params->TryGetStringField(TEXT("interfacePath"), InterfacePath))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Missing blueprintPath or interfacePath"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }

    UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!BP)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }

    // Accept either "/Game/IF.IF_C" or "/Game/IF" — normalize
    FString Normalized = InterfacePath;
    if (!Normalized.Contains(TEXT(".")))
    {
        int32 SlashIdx = INDEX_NONE;
        if (Normalized.FindLastChar(TEXT('/'), SlashIdx))
        {
            Normalized = Normalized + TEXT(".") + Normalized.Mid(SlashIdx + 1) + TEXT("_C");
        }
    }

    UClass* InterfaceClass = LoadObject<UClass>(nullptr, *Normalized);
    if (!InterfaceClass || !InterfaceClass->HasAnyClassFlags(CLASS_Interface))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Not an interface class: %s"), *Normalized));
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }

    const bool bAlreadyImplemented = BP->ImplementedInterfaces.ContainsByPredicate(
        [InterfaceClass](const FBPInterfaceDescription& D) { return D.Interface == InterfaceClass; });
    if (!bAlreadyImplemented)
    {
        FBlueprintEditorUtils::ImplementNewInterface(BP, InterfaceClass->GetClassPathName());
        FKismetEditorUtilities::CompileBlueprint(BP);
    }

    TArray<TSharedPtr<FJsonValue>> CurrentIfaces;
    for (const FBPInterfaceDescription& D : BP->ImplementedInterfaces)
    {
        if (D.Interface) CurrentIfaces.Add(MakeShared<FJsonValueString>(D.Interface->GetPathName()));
    }
    Response->SetBoolField(TEXT("success"), true);
    Response->SetArrayField(TEXT("currentInterfaces"), CurrentIfaces);
    return Response;
}
```

Register:
```cpp
HandlerMap.Add(TEXT("blueprint_add_interface"),
    [this](const TSharedPtr<FJsonObject>& Params) { return HandleBlueprintAddInterface(Params); });
```

Include at top of file if missing:
```cpp
#include "Kismet2/KismetEditorUtilities.h"
```

Add `Kismet` to `PrivateDependencyModuleNames` in `McpAutomationBridge.Build.cs` if not already.

- [ ] **Step 4: Integration test**

Append to `tests/integration.mjs`:
```javascript
{ scenario: 'Blueprint: add interface (pre-existing interface asset required)', toolName: 'manage_blueprint',
  arguments: { action: 'add_interface', blueprintPath: `${TEST_FOLDER}/BP_IntegrationTest`, interfacePath: '/Script/Engine.Interface_AssetUserData' },
  expected: 'success' },
```

(Uses engine-native interface to avoid asset dependency.)

- [ ] **Step 5: Compile + run integration**

```bash
npm run build:core && npm test
```
Expected PASS with `currentInterfaces` containing `/Script/Engine.Interface_AssetUserData`.

- [ ] **Step 6: Commit**

```bash
git add -u
git commit -m "feat(manage_blueprint): add add_interface action

Implements FBlueprintEditorUtils::ImplementNewInterface + compile.
Idempotent: skips if already implemented. Normalizes paths lacking _C suffix."
```

---

## Task 3: Add `remove_interface` action

**Files:** same pattern

- [ ] **Step 1: Unit test**

```typescript
describe('blueprint-handlers: remove_interface', () => {
  it('forwards remove_interface with both paths', async () => {
    const mockTools = { executeAutomation: vi.fn().mockResolvedValue({ success: true, currentInterfaces: [] }) };
    const res = await handleBlueprintTools(
      'remove_interface',
      { blueprintPath: '/Game/BP_T', interfacePath: '/Script/Engine.Interface_AssetUserData' } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(res.success).toBe(true);
    expect(res.currentInterfaces).toEqual([]);
  });
});
```
Run: FAIL.

- [ ] **Step 2: TS handler**

```typescript
case 'remove_interface': {
  const blueprintPath = argsTyped.blueprintPath || (argsRecord.path as string | undefined);
  const interfacePath = argsRecord.interfacePath as string | undefined;
  if (!blueprintPath || typeof blueprintPath !== 'string') {
    throw new Error('Missing required parameter: blueprintPath');
  }
  if (!interfacePath || typeof interfacePath !== 'string') {
    throw new Error('Missing required parameter: interfacePath');
  }
  const res = await executeAutomationRequest(tools, 'blueprint_remove_interface', { blueprintPath, interfacePath });
  return cleanObject(res as Record<string, unknown>) as Record<string, unknown>;
}
```
Run: PASS.

- [ ] **Step 3: C++ handler**

```cpp
TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleBlueprintRemoveInterface(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    FString BlueprintPath, InterfacePath;
    if (!Params->TryGetStringField(TEXT("blueprintPath"), BlueprintPath) ||
        !Params->TryGetStringField(TEXT("interfacePath"), InterfacePath))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Missing blueprintPath or interfacePath"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }

    UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!BP)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }

    FString Normalized = InterfacePath;
    if (!Normalized.Contains(TEXT(".")))
    {
        int32 SlashIdx = INDEX_NONE;
        if (Normalized.FindLastChar(TEXT('/'), SlashIdx))
        {
            Normalized = Normalized + TEXT(".") + Normalized.Mid(SlashIdx + 1) + TEXT("_C");
        }
    }
    UClass* InterfaceClass = LoadObject<UClass>(nullptr, *Normalized);
    if (!InterfaceClass)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Interface class not found: %s"), *Normalized));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }

    FBlueprintEditorUtils::RemoveInterface(BP, InterfaceClass->GetClassPathName(), /*bPreserveFunctions=*/false);
    FKismetEditorUtilities::CompileBlueprint(BP);

    TArray<TSharedPtr<FJsonValue>> CurrentIfaces;
    for (const FBPInterfaceDescription& D : BP->ImplementedInterfaces)
    {
        if (D.Interface) CurrentIfaces.Add(MakeShared<FJsonValueString>(D.Interface->GetPathName()));
    }
    Response->SetBoolField(TEXT("success"), true);
    Response->SetArrayField(TEXT("currentInterfaces"), CurrentIfaces);
    return Response;
}
```

Register:
```cpp
HandlerMap.Add(TEXT("blueprint_remove_interface"),
    [this](const TSharedPtr<FJsonObject>& Params) { return HandleBlueprintRemoveInterface(Params); });
```

- [ ] **Step 4: Integration test**
```javascript
{ scenario: 'Blueprint: remove interface', toolName: 'manage_blueprint',
  arguments: { action: 'remove_interface', blueprintPath: `${TEST_FOLDER}/BP_IntegrationTest`, interfacePath: '/Script/Engine.Interface_AssetUserData' },
  expected: 'success' },
```

- [ ] **Step 5: Compile + test**
```bash
npm run build:core && npm test
```
Expected PASS.

- [ ] **Step 6: Commit**
```bash
git add -u
git commit -m "feat(manage_blueprint): add remove_interface action

Mirrors add_interface using FBlueprintEditorUtils::RemoveInterface with
bPreserveFunctions=false for clean removal. Returns updated interface list."
```

---

## Task 4: Add `set_parent_class` action

- [ ] **Step 1: Unit test**

```typescript
describe('blueprint-handlers: set_parent_class', () => {
  it('forwards set_parent_class with blueprintPath and parentClass', async () => {
    const mockTools = { executeAutomation: vi.fn().mockResolvedValue({
      success: true, oldParent: '/Script/Engine.Actor', newParent: '/Script/Engine.Pawn'
    }) };
    const res = await handleBlueprintTools(
      'set_parent_class',
      { blueprintPath: '/Game/BP_T', parentClass: '/Script/Engine.Pawn' } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(res.success).toBe(true);
    expect(res.oldParent).toBe('/Script/Engine.Actor');
    expect(res.newParent).toBe('/Script/Engine.Pawn');
  });
});
```
Run: FAIL.

- [ ] **Step 2: TS handler**

```typescript
case 'set_parent_class': {
  const blueprintPath = argsTyped.blueprintPath || (argsRecord.path as string | undefined);
  const parentClass = argsRecord.parentClass as string | undefined;
  if (!blueprintPath || typeof blueprintPath !== 'string') {
    throw new Error('Missing required parameter: blueprintPath');
  }
  if (!parentClass || typeof parentClass !== 'string') {
    throw new Error('Missing required parameter: parentClass');
  }
  const res = await executeAutomationRequest(tools, 'blueprint_set_parent_class', { blueprintPath, parentClass });
  return cleanObject(res as Record<string, unknown>) as Record<string, unknown>;
}
```
Run: PASS.

- [ ] **Step 3: C++ handler**

```cpp
TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleBlueprintSetParentClass(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    FString BlueprintPath, ParentClassPath;
    if (!Params->TryGetStringField(TEXT("blueprintPath"), BlueprintPath) ||
        !Params->TryGetStringField(TEXT("parentClass"), ParentClassPath))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Missing blueprintPath or parentClass"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }

    UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!BP)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }

    UClass* NewParent = LoadObject<UClass>(nullptr, *ParentClassPath);
    if (!NewParent)
    {
        // Try "/Game/Foo.Foo_C" if raw "/Game/Foo" given
        FString Normalized = ParentClassPath;
        if (!Normalized.Contains(TEXT(".")))
        {
            int32 SlashIdx = INDEX_NONE;
            if (Normalized.FindLastChar(TEXT('/'), SlashIdx))
            {
                Normalized = Normalized + TEXT(".") + Normalized.Mid(SlashIdx + 1) + TEXT("_C");
            }
        }
        NewParent = LoadObject<UClass>(nullptr, *Normalized);
    }
    if (!NewParent)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Parent class not found: %s"), *ParentClassPath));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }

    const FString OldParentPath = BP->ParentClass ? BP->ParentClass->GetPathName() : FString(TEXT("None"));

    BP->Modify();
    BP->ParentClass = NewParent;
    FBlueprintEditorUtils::RefreshAllNodes(BP);
    FKismetEditorUtilities::CompileBlueprint(BP);
    BP->MarkPackageDirty();
    McpSafeAssetSave(BP);

    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("oldParent"), OldParentPath);
    Response->SetStringField(TEXT("newParent"), NewParent->GetPathName());
    return Response;
}
```

Register:
```cpp
HandlerMap.Add(TEXT("blueprint_set_parent_class"),
    [this](const TSharedPtr<FJsonObject>& Params) { return HandleBlueprintSetParentClass(Params); });
```

- [ ] **Step 4: Integration test**
```javascript
{ scenario: 'Blueprint: set parent class Actor->Pawn', toolName: 'manage_blueprint',
  arguments: { action: 'set_parent_class', blueprintPath: `${TEST_FOLDER}/BP_IntegrationTest`, parentClass: '/Script/Engine.Pawn' },
  expected: 'success' },
```

- [ ] **Step 5: Compile + test**
```bash
npm run build:core && npm test
```
Expected PASS with `oldParent: "/Script/Engine.Actor", newParent: "/Script/Engine.Pawn"`.

- [ ] **Step 6: Commit**
```bash
git add -u
git commit -m "feat(manage_blueprint): add set_parent_class action

Direct BP->ParentClass assignment + RefreshAllNodes + Compile + Save.
Falls back to path+_C normalization when bare /Game/X is given.
Preserves SCS components across reparent (UE handles natively)."
```

---

## Task 5: Audit `add_event` for Event Dispatcher delegate signature

**Files:**
- Read-only: `plugins/.../Private/McpAutomationBridge_BlueprintHandlers.cpp` (existing `add_event` handler)
- Read-only: `src/tools/consolidated-tool-definitions.ts` (existing `add_event` schema)
- Modify (if gap found): both + integration test

- [ ] **Step 1: Grep the existing `add_event` handler**

```bash
grep -n "add_event\|AddEventDispatcher\|CreateEventDispatcher\|FBPVariableDescription" plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_BlueprintHandlers.cpp
```

Read the handler function body (~20 lines). Determine:
- Does it create an **event node** in the event graph (UK2Node_Event), OR
- Does it create an **event dispatcher** (FBPVariableDescription with MulticastInlineDelegateProperty)?
- Does it accept a `delegateSignature` parameter (e.g. `inputs: [{name, type}]`)?

- [ ] **Step 2: Record finding inline in the commit message**

Three possible outcomes:

**Outcome A — `add_event` creates event node only, no dispatcher:** Add a NEW action `add_event_dispatcher` with signature param. See Step 3A.

**Outcome B — `add_event` already handles dispatcher but no signature:** Extend existing schema + handler to accept `delegateSignature: [{name, type}]`. See Step 3B.

**Outcome C — fully supported:** Add integration test verifying it and close with a no-code commit. See Step 3C.

- [ ] **Step 3A (if Outcome A): Add `add_event_dispatcher` action**

Extend action enum:
```typescript
'add_event_dispatcher'
```

Schema inputs:
```typescript
dispatcherName: { type: 'string', description: 'Name for the event dispatcher.' },
delegateSignature: {
  type: 'array',
  items: {
    type: 'object',
    properties: {
      name: { type: 'string' },
      type: { type: 'string', description: 'Pin type in Unreal tag form, e.g. "bool", "int", "float", "string", "object /Script/Engine.Actor".' }
    },
    required: ['name', 'type']
  },
  description: 'Input parameters for the dispatcher delegate.'
},
```

TS handler case:
```typescript
case 'add_event_dispatcher': {
  const blueprintPath = argsTyped.blueprintPath || (argsRecord.path as string | undefined);
  const dispatcherName = argsRecord.dispatcherName as string | undefined;
  const delegateSignature = argsRecord.delegateSignature as unknown[] | undefined;
  if (!blueprintPath || typeof blueprintPath !== 'string') throw new Error('Missing required parameter: blueprintPath');
  if (!dispatcherName || typeof dispatcherName !== 'string') throw new Error('Missing required parameter: dispatcherName');
  const res = await executeAutomationRequest(tools, 'blueprint_add_event_dispatcher', {
    blueprintPath, dispatcherName, delegateSignature: delegateSignature ?? []
  });
  return cleanObject(res as Record<string, unknown>) as Record<string, unknown>;
}
```

C++ handler (uses `FBlueprintEditorUtils::AddNewVariable` with `FMulticastInlineDelegateProperty` type):

```cpp
TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleBlueprintAddEventDispatcher(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    FString BlueprintPath, DispatcherName;
    if (!Params->TryGetStringField(TEXT("blueprintPath"), BlueprintPath) ||
        !Params->TryGetStringField(TEXT("dispatcherName"), DispatcherName))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Missing blueprintPath or dispatcherName"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }
    UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!BP) {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("BP not found: %s"), *BlueprintPath));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }

    // Create dispatcher: use Blueprint.EventGraphs path: FBlueprintEditorUtils::CreateNewBlueprintFunction cannot — use AddNewVariable with MulticastInlineDelegateProperty
    FEdGraphPinType DelegatePinType;
    DelegatePinType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;
    // Parse delegateSignature into FEdGraphPinType[] for the signature function
    // For simplicity, create a minimal signature function via UEdGraph infrastructure
    // NOTE: UE does not expose a one-call API; use FBlueprintEditorUtils::CreateMatchingFunction + bind
    // Minimal approach: call UK2Node_CreateDelegate; fallback: add as signature-less multicast

    UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(BP);
    if (!EventGraph) {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("No event graph"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("EngineAPIError"));
        return Response;
    }

    // Add a new event-dispatcher-style delegate variable
    const bool bAdded = FBlueprintEditorUtils::AddNewVariable(BP, FName(*DispatcherName), DelegatePinType, FString());
    if (!bAdded) {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("AddNewVariable failed"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("EngineAPIError"));
        return Response;
    }

    // Signature: for UE 5.7, the supported path is to mark the variable as dispatcher and add a skeleton function.
    // Using FBlueprintEditorUtils::AddFunctionGraph with matching inputs.
    // For the initial version we document this as "signature-less" — callers can use Blueprint Editor to add pins.
    // TODO (tracked): wire delegateSignature into CreateMatchingSignatureFunction.

    FKismetEditorUtilities::CompileBlueprint(BP);
    McpSafeAssetSave(BP);

    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("dispatcherName"), DispatcherName);
    return Response;
}
```

Register handler. Integration test:
```javascript
{ scenario: 'Blueprint: add event dispatcher', toolName: 'manage_blueprint',
  arguments: { action: 'add_event_dispatcher', blueprintPath: `${TEST_FOLDER}/BP_IntegrationTest`, dispatcherName: 'OnTestFired' },
  expected: 'success' },
```

- [ ] **Step 3B (if Outcome B): Extend `add_event` schema**

Add `delegateSignature` parameter to existing schema. Extend the existing handler to read it and wire via `CreateMatchingSignatureFunction`. Keep existing behavior when signature is omitted (backward compat).

- [ ] **Step 3C (if Outcome C): Just add test coverage**

No code change — add integration test exercising signature parameter.

- [ ] **Step 4: Commit (pick matching message)**

Outcome A:
```bash
git commit -m "feat(manage_blueprint): add add_event_dispatcher action

Creates MulticastInlineDelegate variable on BP. Signature wiring deferred
to follow-up (tracked TODO); initial version produces signature-less
dispatcher that callers can extend in Blueprint Editor."
```

Outcome B:
```bash
git commit -m "feat(manage_blueprint): extend add_event with delegateSignature

add_event now accepts optional inputs: [{name, type}] to wire the
dispatcher's signature. Backward-compatible: omitting signature preserves
prior zero-argument behavior."
```

Outcome C:
```bash
git commit -m "test(manage_blueprint): cover add_event dispatcher signature"
```

---

## Acceptance Checklist

- [ ] All 5 tasks committed (or 4 + audit no-op)
- [ ] Unit tests pass: `npx vitest run src/tools/handlers/blueprint-handlers.test.ts`
- [ ] Integration scenarios pass in live UE 5.7 (4 new scenarios all green)
- [ ] `BP_IntegrationTest` state after Ch1: parent = Pawn, ImplementedInterfaces contains Interface_AssetUserData (then removed to empty)
- [ ] Compiles cleanly, no new C++ warnings
- [ ] No `as any` / `@ts-ignore` introduced

---

## Notes for Subagent

- **Existing `add_event` behavior is unknown** — you MUST run the grep in Task 5 Step 1 before writing any code in Task 5. This is a conditional task.
- **Integration tests require live UE Editor** — if running mock-only, integration tests pass-through as `expected: 'success|handled|blocked'`. Live verification required before declaring Ch1 done.
- **Subsystem file location** — `UMcpAutomationBridgeSubsystem` is declared in `McpAutomationBridgeSubsystem.h` and impl split across multiple `_*Handlers.cpp` files. Each new handler: declare in the subsystem header (private section), implement in the appropriate `_BlueprintHandlers.cpp` file.
