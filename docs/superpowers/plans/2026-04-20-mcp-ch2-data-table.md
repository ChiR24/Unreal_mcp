# Ch2 — `manage_data` DataTable (8 actions)

> **Parent plan:** `2026-04-20-mcp-tier123-expansion.md`
> **Spec:** §4 Ch2
> **Depends on:** Task 0 (shared scaffolding) merged
> **Estimated:** 1 day, 9 commits (1 tool boilerplate + 8 actions)

**Goal:** New tool `manage_data` with DataTable CRUD + schema migration. First consumer of `McpStructReflection`.

---

## Task 1: Bootstrap `manage_data` tool

**Files:**
- Modify: `src/tools/consolidated-tool-definitions.ts` (append new tool)
- Create: `src/tools/handlers/data-handlers.ts`
- Create: `src/tools/handlers/data-handlers.test.ts`
- Modify: `src/tools/consolidated-tool-handlers.ts` (import + route)
- Create: `plugins/.../Private/MCP/Tools/McpTool_ManageData.cpp`
- Create: `plugins/.../Private/McpAutomationBridge_DataHandlers.cpp`
- Modify: `plugins/.../Private/McpAutomationBridgeSubsystem.cpp`

- [ ] **Step 1: Add `manage_data` tool definition**

Append to `src/tools/consolidated-tool-definitions.ts` (end of the exported tool array, before the closing `];`):

```typescript
{
  name: 'manage_data',
  category: 'authoring',
  description: 'Create and modify UDataTable / UDataAsset instances (row-level CRUD, property paths, schema migration).',
  inputSchema: {
    type: 'object',
    properties: {
      action: {
        type: 'string',
        enum: [
          'create_data_table', 'add_data_table_row', 'set_data_table_row',
          'update_data_table_row', 'remove_data_table_row', 'get_data_table_rows',
          'list_data_table_rows', 'set_data_table_row_struct',
          'create_data_asset', 'set_data_asset_property', 'get_data_asset_property',
          'list_data_assets_of_class'
        ],
        description: 'Data-layer action to perform.'
      },
      path: { type: 'string', description: 'Package path (/Game/...) for the target asset.' },
      name: { type: 'string', description: 'Asset name (for create actions).' },
      rowStructPath: { type: 'string', description: 'Path to UScriptStruct / UUserDefinedStruct used as DataTable row type.' },
      newRowStructPath: { type: 'string', description: 'Target row struct path for set_data_table_row_struct migration.' },
      rowName: { type: 'string', description: 'DataTable row name.' },
      rowNames: { type: 'array', items: { type: 'string' }, description: 'Optional row filter for get_data_table_rows.' },
      fields: { type: 'object', additionalProperties: true, description: 'Field values (row or DataAsset).' },
      dataAssetClassPath: { type: 'string', description: 'UDataAsset BP or native class path for create_data_asset.' },
      propertyPath: { type: 'string', description: 'Dotted/indexed property path, e.g. "Stats.Health" or "Effects.[0].Value".' },
      value: { description: 'JSON value to set (any type; resolved via reflection).' },
      classPath: { type: 'string', description: 'Class to filter by for list_data_assets_of_class.' },
      searchPaths: { type: 'array', items: { type: 'string' }, description: 'Optional /Game/ subpath roots for scoped search.' }
    },
    required: ['action']
  },
  outputSchema: {
    type: 'object',
    properties: {
      success: { type: 'boolean' },
      error: { type: 'string' },
      errorCategory: { type: 'string' },
      assetPath: { type: 'string' },
      rowName: { type: 'string' },
      rowNames: { type: 'array', items: { type: 'string' } },
      rows: { type: 'object', additionalProperties: true },
      updatedFields: { type: 'array', items: { type: 'string' } },
      rowsMigrated: { type: 'number' },
      value: {},
      assets: { type: 'array', items: { type: 'string' } }
    }
  }
}
```

- [ ] **Step 2: Create `data-handlers.ts` skeleton**

```typescript
// src/tools/handlers/data-handlers.ts
import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';

export async function handleDataTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const argsRecord = args as Record<string, unknown>;
  switch (action) {
    default:
      throw new Error(`Unsupported manage_data action: ${action}`);
  }
}
```

- [ ] **Step 3: Wire tool route**

In `src/tools/consolidated-tool-handlers.ts`:

Add import (near other handler imports):
```typescript
import { handleDataTools } from './handlers/data-handlers.js';
```

Inside the handler dispatch (find where `handleBlueprintTools` is called; add a sibling branch):
```typescript
case 'manage_data':
  return await handleDataTools(action, args, tools);
```

- [ ] **Step 4: Create C++ tool dispatcher**

`plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Tools/McpTool_ManageData.cpp`:

```cpp
#include "McpToolDefinition.h"
#include "McpToolRegistry.h"

namespace McpTool_ManageData
{
    static FMcpToolDefinition MakeDefinition()
    {
        FMcpToolDefinition Def;
        Def.Name = TEXT("manage_data");
        Def.Category = TEXT("authoring");
        Def.Description = TEXT("Create and modify UDataTable / UDataAsset instances.");
        // Schema is authored on the TS side; C++ only needs the name-to-handler mapping
        Def.ActionMapping = {
            {TEXT("create_data_table"), TEXT("data_create_table")},
            {TEXT("add_data_table_row"), TEXT("data_add_row")},
            {TEXT("set_data_table_row"), TEXT("data_set_row")},
            {TEXT("update_data_table_row"), TEXT("data_update_row")},
            {TEXT("remove_data_table_row"), TEXT("data_remove_row")},
            {TEXT("get_data_table_rows"), TEXT("data_get_rows")},
            {TEXT("list_data_table_rows"), TEXT("data_list_rows")},
            {TEXT("set_data_table_row_struct"), TEXT("data_set_row_struct")},
            {TEXT("create_data_asset"), TEXT("data_create_asset")},
            {TEXT("set_data_asset_property"), TEXT("data_set_asset_prop")},
            {TEXT("get_data_asset_property"), TEXT("data_get_asset_prop")},
            {TEXT("list_data_assets_of_class"), TEXT("data_list_assets_of_class")}
        };
        return Def;
    }

    static struct FRegister
    {
        FRegister() { FMcpToolRegistry::Get().Register(MakeDefinition()); }
    } GRegister;
}
```

(Adapt to the actual FMcpToolDefinition/Registry API — read `McpToolDefinition.h` to confirm shape. If the existing tools use a different registration pattern, mirror the nearest neighbor such as `McpTool_ManageBlueprint.cpp`.)

- [ ] **Step 5: Create empty handlers file**

`plugins/.../Private/McpAutomationBridge_DataHandlers.cpp`:

```cpp
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "MCP/Helpers/McpStructReflection.h"
#include "MCP/Helpers/McpGenericAssetFactory.h"
#include "MCP/Helpers/McpPropertyPath.h"
#include "Engine/DataTable.h"
#include "Engine/DataAsset.h"
#include "Engine/UserDefinedStruct.h"
#include "Factories/DataTableFactory.h"
#include "Factories/DataAssetFactory.h"
#include "AssetToolsModule.h"
#include "Modules/ModuleManager.h"

// Handler function bodies will be added per-task below.
```

- [ ] **Step 6: Compile + smoke**

```bash
npm run build:core
# UE plugin auto-rebuild
```
Expected: build succeeds, `manage_data` tool listed by server but all actions throw "Unsupported" (TS) / "Unknown handler" (C++).

- [ ] **Step 7: Commit**

```bash
git add src/tools/consolidated-tool-definitions.ts src/tools/handlers/data-handlers.ts src/tools/consolidated-tool-handlers.ts plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Tools/McpTool_ManageData.cpp plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_DataHandlers.cpp
git commit -m "feat(manage_data): bootstrap tool skeleton

Registers manage_data tool with 12-action enum. Handlers throw
unsupported until per-action implementations land."
```

---

## Task 2: `create_data_table` action

**Files:**
- Modify: `src/tools/handlers/data-handlers.ts`
- Modify: `src/tools/handlers/data-handlers.test.ts`
- Modify: `plugins/.../Private/McpAutomationBridge_DataHandlers.cpp`
- Modify: `plugins/.../Private/McpAutomationBridgeSubsystem.cpp`
- Modify: `tests/integration.mjs`

- [ ] **Step 1: Failing unit test**

```typescript
// src/tools/handlers/data-handlers.test.ts
import { describe, it, expect, vi } from 'vitest';
import { handleDataTools } from './data-handlers.js';

describe('manage_data: create_data_table', () => {
  it('forwards to automation with path, name, rowStructPath', async () => {
    const mockTools = { executeAutomation: vi.fn().mockResolvedValue({
      success: true, assetPath: '/Game/Data/DT_Test'
    }) };
    const res = await handleDataTools(
      'create_data_table',
      { path: '/Game/Data', name: 'DT_Test', rowStructPath: '/Game/Data/ST_Row.ST_Row' } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(res.success).toBe(true);
    expect(res.assetPath).toBe('/Game/Data/DT_Test');
  });
  it('throws on missing rowStructPath', async () => {
    const mockTools = { executeAutomation: vi.fn() };
    await expect(handleDataTools(
      'create_data_table',
      { path: '/Game', name: 'X' } as unknown as Record<string, unknown>,
      mockTools as never
    )).rejects.toThrow(/rowStructPath/);
  });
});
```
Run: FAIL.

- [ ] **Step 2: TS handler case**

Replace the `default` throw in the switch with this case added above it:

```typescript
case 'create_data_table': {
  const path = argsRecord.path as string | undefined;
  const name = argsRecord.name as string | undefined;
  const rowStructPath = argsRecord.rowStructPath as string | undefined;
  if (!path) throw new Error('Missing required parameter: path');
  if (!name) throw new Error('Missing required parameter: name');
  if (!rowStructPath) throw new Error('Missing required parameter: rowStructPath');
  const res = await executeAutomationRequest(tools, 'data_create_table', { path, name, rowStructPath });
  return cleanObject(res as Record<string, unknown>) as Record<string, unknown>;
}
```

Run: PASS.

- [ ] **Step 3: C++ handler**

```cpp
TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleDataCreateTable(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    FString PathStr, NameStr, RowStructPath;
    if (!Params->TryGetStringField(TEXT("path"), PathStr) ||
        !Params->TryGetStringField(TEXT("name"), NameStr) ||
        !Params->TryGetStringField(TEXT("rowStructPath"), RowStructPath))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Missing path/name/rowStructPath"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }

    UScriptStruct* RowStruct = LoadObject<UScriptStruct>(nullptr, *RowStructPath);
    if (!RowStruct)
    {
        // Try UUserDefinedStruct path with _C suffix handling
        RowStruct = Cast<UScriptStruct>(LoadObject<UObject>(nullptr, *RowStructPath));
    }
    if (!RowStruct)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Row struct not found: %s"), *RowStructPath));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }

    FString OutError;
    bool bSaved = false;
    UObject* NewTable = McpGenericAssetFactory::CreateAssetOfClass(
        UDataTable::StaticClass(),
        PathStr,
        NameStr,
        [RowStruct](UObject* Asset) {
            if (UDataTable* DT = Cast<UDataTable>(Asset))
            {
                DT->RowStruct = RowStruct;
            }
        },
        OutError,
        bSaved);

    if (!NewTable)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), OutError);
        Response->SetStringField(TEXT("errorCategory"), TEXT("EngineAPIError"));
        return Response;
    }

    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("assetPath"), NewTable->GetPathName());
    return Response;
}
```

Register in subsystem:
```cpp
HandlerMap.Add(TEXT("data_create_table"),
    [this](const TSharedPtr<FJsonObject>& Params) { return HandleDataCreateTable(Params); });
```

- [ ] **Step 4: Integration test**

Before this scenario, add a prerequisite scenario to create a test struct (if not present):
```javascript
{ scenario: 'Data prep: create test struct', toolName: 'manage_blueprint',
  arguments: { action: 'create_struct', path: '/Game/DataTest', name: 'ST_Ch2Row' },
  expected: 'success|already exists' },
{ scenario: 'Data prep: add field Name:FName', toolName: 'manage_blueprint',
  arguments: { action: 'modify_struct', path: '/Game/DataTest/ST_Ch2Row', op: 'add_variable', varName: 'Name', varType: 'name' },
  expected: 'success|already exists' },
{ scenario: 'Data prep: add field Value:double', toolName: 'manage_blueprint',
  arguments: { action: 'modify_struct', path: '/Game/DataTest/ST_Ch2Row', op: 'add_variable', varName: 'Value', varType: 'double' },
  expected: 'success|already exists' },
{ scenario: 'Data: create DataTable', toolName: 'manage_data',
  arguments: { action: 'create_data_table', path: '/Game/DataTest', name: 'DT_Ch2', rowStructPath: '/Game/DataTest/ST_Ch2Row.ST_Ch2Row' },
  expected: 'success|already exists' },
```

(Verify `modify_struct` schema — the varType / op enums may differ; check `src/tools/consolidated-tool-definitions.ts` for the actual `modify_struct` enum before committing. Adjust accordingly.)

- [ ] **Step 5: Compile + run**
```bash
npm run build:core && npm test
```
Expected: all 4 scenarios success.

- [ ] **Step 6: Commit**
```bash
git add -u
git commit -m "feat(manage_data): add create_data_table action

Creates UDataTable asset, sets RowStruct to provided UScriptStruct
or UUserDefinedStruct, saves via McpGenericAssetFactory."
```

---

## Task 3: `add_data_table_row` action

- [ ] **Step 1: Unit test**

```typescript
describe('manage_data: add_data_table_row', () => {
  it('forwards with path, rowName, fields', async () => {
    const mockTools = { executeAutomation: vi.fn().mockResolvedValue({ success: true, rowName: 'R1' }) };
    const res = await handleDataTools(
      'add_data_table_row',
      { path: '/Game/DT', rowName: 'R1', fields: { Name: 'A', Value: 1.5 } } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(res.success).toBe(true);
    expect(res.rowName).toBe('R1');
  });
  it('allows omitted fields (defaults)', async () => {
    const mockTools = { executeAutomation: vi.fn().mockResolvedValue({ success: true, rowName: 'R1' }) };
    await handleDataTools(
      'add_data_table_row',
      { path: '/Game/DT', rowName: 'R1' } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(mockTools.executeAutomation).toHaveBeenCalledWith(
      'data_add_row',
      expect.objectContaining({ fields: {} })
    );
  });
});
```
Run: FAIL.

- [ ] **Step 2: TS handler**

```typescript
case 'add_data_table_row': {
  const path = argsRecord.path as string | undefined;
  const rowName = argsRecord.rowName as string | undefined;
  const fields = (argsRecord.fields as Record<string, unknown> | undefined) ?? {};
  if (!path) throw new Error('Missing required parameter: path');
  if (!rowName) throw new Error('Missing required parameter: rowName');
  const res = await executeAutomationRequest(tools, 'data_add_row', { path, rowName, fields });
  return cleanObject(res as Record<string, unknown>) as Record<string, unknown>;
}
```
Run: PASS.

- [ ] **Step 3: C++ handler**

```cpp
TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleDataAddRow(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    FString PathStr, RowNameStr;
    if (!Params->TryGetStringField(TEXT("path"), PathStr) ||
        !Params->TryGetStringField(TEXT("rowName"), RowNameStr))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Missing path or rowName"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }

    UDataTable* DT = LoadObject<UDataTable>(nullptr, *PathStr);
    if (!DT)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("DataTable not found: %s"), *PathStr));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }
    if (!DT->RowStruct)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("DataTable has no RowStruct"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("ConflictState"));
        return Response;
    }
    const FName RowName(*RowNameStr);
    if (DT->GetRowMap().Contains(RowName))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Row already exists: %s"), *RowNameStr));
        Response->SetStringField(TEXT("errorCategory"), TEXT("ConflictState"));
        return Response;
    }

    // Alloc + init row data
    uint8* NewRow = (uint8*)FMemory::Malloc(DT->RowStruct->GetStructureSize());
    DT->RowStruct->InitializeStruct(NewRow);

    // Apply fields
    const TSharedPtr<FJsonObject>* FieldsObj = nullptr;
    if (Params->TryGetObjectField(TEXT("fields"), FieldsObj) && FieldsObj && (*FieldsObj).IsValid())
    {
        FString StructError;
        if (!McpStructReflection::SetStructFieldsFromJsonObject(DT->RowStruct, NewRow, *FieldsObj, StructError))
        {
            DT->RowStruct->DestroyStruct(NewRow);
            FMemory::Free(NewRow);
            Response->SetBoolField(TEXT("success"), false);
            Response->SetStringField(TEXT("error"), StructError);
            Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
            return Response;
        }
    }

    DT->AddRow(RowName, *reinterpret_cast<FTableRowBase*>(NewRow));
    // AddRow copies; free the temp buffer
    DT->RowStruct->DestroyStruct(NewRow);
    FMemory::Free(NewRow);

    DT->MarkPackageDirty();
    McpSafeAssetSave(DT);

    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("rowName"), RowNameStr);
    return Response;
}
```

Register:
```cpp
HandlerMap.Add(TEXT("data_add_row"),
    [this](const TSharedPtr<FJsonObject>& Params) { return HandleDataAddRow(Params); });
```

- [ ] **Step 4: Integration scenario**
```javascript
{ scenario: 'Data: add DT row with fields', toolName: 'manage_data',
  arguments: { action: 'add_data_table_row', path: '/Game/DataTest/DT_Ch2', rowName: 'Row1', fields: { Name: 'Alpha', Value: 1.5 } },
  expected: 'success|already exists' },
```

- [ ] **Step 5: Compile + run**
```bash
npm run build:core && npm test
```
Expected PASS.

- [ ] **Step 6: Commit**
```bash
git add -u
git commit -m "feat(manage_data): add add_data_table_row action

Allocates row via RowStruct->InitializeStruct, applies JSON fields
via McpStructReflection, copies into DataTable via AddRow, saves."
```

---

## Task 4: `set_data_table_row` action (overwrite)

- [ ] **Step 1: Unit test**

```typescript
describe('manage_data: set_data_table_row', () => {
  it('forwards with fields overwriting full row', async () => {
    const mockTools = { executeAutomation: vi.fn().mockResolvedValue({ success: true }) };
    await handleDataTools(
      'set_data_table_row',
      { path: '/Game/DT', rowName: 'R1', fields: { Name: 'B', Value: 9 } } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(mockTools.executeAutomation).toHaveBeenCalledWith('data_set_row', expect.objectContaining({ rowName: 'R1' }));
  });
  it('throws on missing fields', async () => {
    const mockTools = { executeAutomation: vi.fn() };
    await expect(handleDataTools(
      'set_data_table_row',
      { path: '/Game/DT', rowName: 'R1' } as unknown as Record<string, unknown>,
      mockTools as never
    )).rejects.toThrow(/fields/);
  });
});
```
Run: FAIL.

- [ ] **Step 2: TS handler**

```typescript
case 'set_data_table_row': {
  const path = argsRecord.path as string | undefined;
  const rowName = argsRecord.rowName as string | undefined;
  const fields = argsRecord.fields as Record<string, unknown> | undefined;
  if (!path) throw new Error('Missing required parameter: path');
  if (!rowName) throw new Error('Missing required parameter: rowName');
  if (!fields || typeof fields !== 'object') throw new Error('Missing required parameter: fields');
  const res = await executeAutomationRequest(tools, 'data_set_row', { path, rowName, fields });
  return cleanObject(res as Record<string, unknown>) as Record<string, unknown>;
}
```
Run: PASS.

- [ ] **Step 3: C++ handler**

```cpp
TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleDataSetRow(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    FString PathStr, RowNameStr;
    const TSharedPtr<FJsonObject>* FieldsObj = nullptr;
    if (!Params->TryGetStringField(TEXT("path"), PathStr) ||
        !Params->TryGetStringField(TEXT("rowName"), RowNameStr) ||
        !Params->TryGetObjectField(TEXT("fields"), FieldsObj))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Missing path/rowName/fields"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }
    UDataTable* DT = LoadObject<UDataTable>(nullptr, *PathStr);
    if (!DT || !DT->RowStruct)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("DataTable not found or missing RowStruct"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }
    const FName RowName(*RowNameStr);
    uint8* ExistingRow = DT->GetRowMap().FindRef(RowName);
    if (!ExistingRow)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Row not found: %s"), *RowNameStr));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }

    // Reset row to struct defaults, then apply fields
    DT->RowStruct->DestroyStruct(ExistingRow);
    DT->RowStruct->InitializeStruct(ExistingRow);
    FString StructError;
    if (!McpStructReflection::SetStructFieldsFromJsonObject(DT->RowStruct, ExistingRow, *FieldsObj, StructError))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), StructError);
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }

    DT->MarkPackageDirty();
    McpSafeAssetSave(DT);
    Response->SetBoolField(TEXT("success"), true);
    return Response;
}
```

Register: `data_set_row` → `HandleDataSetRow`.

- [ ] **Step 4: Integration**
```javascript
{ scenario: 'Data: set DT row (overwrite)', toolName: 'manage_data',
  arguments: { action: 'set_data_table_row', path: '/Game/DataTest/DT_Ch2', rowName: 'Row1', fields: { Name: 'Beta', Value: 42 } },
  expected: 'success' },
```

- [ ] **Step 5: Compile + run**. Expected PASS.

- [ ] **Step 6: Commit**
```bash
git add -u
git commit -m "feat(manage_data): add set_data_table_row action

Destroy-then-init row struct in-place, apply JSON fields. Semantics:
fields replace the row (missing fields → struct defaults)."
```

---

## Task 5: `update_data_table_row` action (partial patch)

- [ ] **Step 1: Unit test**

```typescript
describe('manage_data: update_data_table_row', () => {
  it('forwards partial fields with updatedFields array', async () => {
    const mockTools = { executeAutomation: vi.fn().mockResolvedValue({ success: true, updatedFields: ['Value'] }) };
    const res = await handleDataTools(
      'update_data_table_row',
      { path: '/Game/DT', rowName: 'R1', fields: { Value: 99 } } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(res.updatedFields).toEqual(['Value']);
  });
});
```
Run: FAIL.

- [ ] **Step 2: TS handler**

```typescript
case 'update_data_table_row': {
  const path = argsRecord.path as string | undefined;
  const rowName = argsRecord.rowName as string | undefined;
  const fields = argsRecord.fields as Record<string, unknown> | undefined;
  if (!path) throw new Error('Missing required parameter: path');
  if (!rowName) throw new Error('Missing required parameter: rowName');
  if (!fields || typeof fields !== 'object') throw new Error('Missing required parameter: fields');
  const res = await executeAutomationRequest(tools, 'data_update_row', { path, rowName, fields });
  return cleanObject(res as Record<string, unknown>) as Record<string, unknown>;
}
```
Run: PASS.

- [ ] **Step 3: C++ handler**

```cpp
TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleDataUpdateRow(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    FString PathStr, RowNameStr;
    const TSharedPtr<FJsonObject>* FieldsObj = nullptr;
    if (!Params->TryGetStringField(TEXT("path"), PathStr) ||
        !Params->TryGetStringField(TEXT("rowName"), RowNameStr) ||
        !Params->TryGetObjectField(TEXT("fields"), FieldsObj))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Missing path/rowName/fields"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }
    UDataTable* DT = LoadObject<UDataTable>(nullptr, *PathStr);
    if (!DT || !DT->RowStruct)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("DataTable not found or missing RowStruct"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }
    uint8* ExistingRow = DT->GetRowMap().FindRef(FName(*RowNameStr));
    if (!ExistingRow)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Row not found: %s"), *RowNameStr));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }

    TArray<TSharedPtr<FJsonValue>> UpdatedFieldNames;
    for (const auto& Pair : (*FieldsObj)->Values)
    {
        const FName ResolvedName = McpStructReflection::ResolveFieldName(DT->RowStruct, Pair.Key);
        if (ResolvedName == NAME_None)
        {
            Response->SetBoolField(TEXT("success"), false);
            Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown field: %s"), *Pair.Key));
            Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
            return Response;
        }
        FString SetError;
        if (!McpStructReflection::SetStructFieldFromJson(DT->RowStruct, ExistingRow, ResolvedName, Pair.Value, SetError))
        {
            Response->SetBoolField(TEXT("success"), false);
            Response->SetStringField(TEXT("error"), SetError);
            Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
            return Response;
        }
        UpdatedFieldNames.Add(MakeShared<FJsonValueString>(Pair.Key));
    }

    DT->MarkPackageDirty();
    McpSafeAssetSave(DT);

    Response->SetBoolField(TEXT("success"), true);
    Response->SetArrayField(TEXT("updatedFields"), UpdatedFieldNames);
    return Response;
}
```

Register: `data_update_row`.

- [ ] **Step 4: Integration**
```javascript
{ scenario: 'Data: update DT row (partial)', toolName: 'manage_data',
  arguments: { action: 'update_data_table_row', path: '/Game/DataTest/DT_Ch2', rowName: 'Row1', fields: { Value: 100 } },
  expected: 'success' },
```

- [ ] **Step 5: Compile + run**. PASS.

- [ ] **Step 6: Commit**
```bash
git add -u
git commit -m "feat(manage_data): add update_data_table_row action

Partial field patch (preserves un-specified fields). Returns updatedFields
array echoing the logical field names patched."
```

---

## Task 6: `remove_data_table_row` action

- [ ] **Step 1: Unit test**

```typescript
describe('manage_data: remove_data_table_row', () => {
  it('forwards path + rowName', async () => {
    const mockTools = { executeAutomation: vi.fn().mockResolvedValue({ success: true }) };
    await handleDataTools(
      'remove_data_table_row',
      { path: '/Game/DT', rowName: 'R1' } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(mockTools.executeAutomation).toHaveBeenCalledWith('data_remove_row', expect.objectContaining({ rowName: 'R1' }));
  });
});
```
Run: FAIL.

- [ ] **Step 2: TS handler**

```typescript
case 'remove_data_table_row': {
  const path = argsRecord.path as string | undefined;
  const rowName = argsRecord.rowName as string | undefined;
  if (!path) throw new Error('Missing required parameter: path');
  if (!rowName) throw new Error('Missing required parameter: rowName');
  const res = await executeAutomationRequest(tools, 'data_remove_row', { path, rowName });
  return cleanObject(res as Record<string, unknown>) as Record<string, unknown>;
}
```
Run: PASS.

- [ ] **Step 3: C++ handler**

```cpp
TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleDataRemoveRow(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    FString PathStr, RowNameStr;
    if (!Params->TryGetStringField(TEXT("path"), PathStr) ||
        !Params->TryGetStringField(TEXT("rowName"), RowNameStr))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Missing path or rowName"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }
    UDataTable* DT = LoadObject<UDataTable>(nullptr, *PathStr);
    if (!DT)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("DataTable not found"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }
    const FName RowName(*RowNameStr);
    if (!DT->GetRowMap().Contains(RowName))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Row not found: %s"), *RowNameStr));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }
    DT->RemoveRow(RowName);
    DT->MarkPackageDirty();
    McpSafeAssetSave(DT);
    Response->SetBoolField(TEXT("success"), true);
    return Response;
}
```

Register: `data_remove_row`.

- [ ] **Step 4: Integration**
```javascript
{ scenario: 'Data: remove DT row', toolName: 'manage_data',
  arguments: { action: 'remove_data_table_row', path: '/Game/DataTest/DT_Ch2', rowName: 'Row1' },
  expected: 'success' },
```

- [ ] **Step 5: Compile + run**. PASS.

- [ ] **Step 6: Commit**
```bash
git add -u
git commit -m "feat(manage_data): add remove_data_table_row action"
```

---

## Task 7: `get_data_table_rows` action

- [ ] **Step 1: Unit test**

```typescript
describe('manage_data: get_data_table_rows', () => {
  it('returns rows object', async () => {
    const mockTools = { executeAutomation: vi.fn().mockResolvedValue({
      success: true, rows: { Row1: { Name: 'A', Value: 1 } }
    }) };
    const res = await handleDataTools(
      'get_data_table_rows',
      { path: '/Game/DT' } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(res.rows).toEqual({ Row1: { Name: 'A', Value: 1 } });
  });
});
```
Run: FAIL.

- [ ] **Step 2: TS handler**

```typescript
case 'get_data_table_rows': {
  const path = argsRecord.path as string | undefined;
  const rowNames = argsRecord.rowNames as string[] | undefined;
  if (!path) throw new Error('Missing required parameter: path');
  const res = await executeAutomationRequest(tools, 'data_get_rows', { path, rowNames });
  return cleanObject(res as Record<string, unknown>) as Record<string, unknown>;
}
```
Run: PASS.

- [ ] **Step 3: C++ handler**

```cpp
TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleDataGetRows(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    FString PathStr;
    if (!Params->TryGetStringField(TEXT("path"), PathStr))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Missing path"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }
    UDataTable* DT = LoadObject<UDataTable>(nullptr, *PathStr);
    if (!DT || !DT->RowStruct)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("DataTable not found or missing RowStruct"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }

    TSet<FString> Filter;
    const TArray<TSharedPtr<FJsonValue>>* NamesArr = nullptr;
    if (Params->TryGetArrayField(TEXT("rowNames"), NamesArr))
    {
        for (const auto& V : *NamesArr) Filter.Add(V->AsString());
    }

    TSharedPtr<FJsonObject> RowsObj = MakeShared<FJsonObject>();
    for (const auto& Pair : DT->GetRowMap())
    {
        const FString RowName = Pair.Key.ToString();
        if (Filter.Num() > 0 && !Filter.Contains(RowName)) continue;
        TSharedPtr<FJsonObject> RowJson = McpStructReflection::StructInstanceToJson(DT->RowStruct, Pair.Value);
        RowsObj->SetObjectField(RowName, RowJson);
    }

    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("rows"), RowsObj);
    return Response;
}
```

Register: `data_get_rows`.

- [ ] **Step 4: Integration**
```javascript
{ scenario: 'Data: get DT rows', toolName: 'manage_data',
  arguments: { action: 'get_data_table_rows', path: '/Game/DataTest/DT_Ch2' },
  expected: 'success' },
```

- [ ] **Step 5: Compile + run**. PASS.

- [ ] **Step 6: Commit**
```bash
git add -u
git commit -m "feat(manage_data): add get_data_table_rows action

Returns rows as {rowName: fields} map. Optional rowNames filter for
scoped reads. Field names are logical (GUID suffix stripped for UDS)."
```

---

## Task 8: `list_data_table_rows` action (lightweight)

- [ ] **Step 1: Unit test**

```typescript
describe('manage_data: list_data_table_rows', () => {
  it('returns row name array', async () => {
    const mockTools = { executeAutomation: vi.fn().mockResolvedValue({ success: true, rowNames: ['A', 'B'] }) };
    const res = await handleDataTools(
      'list_data_table_rows',
      { path: '/Game/DT' } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(res.rowNames).toEqual(['A', 'B']);
  });
});
```
Run: FAIL.

- [ ] **Step 2: TS handler**

```typescript
case 'list_data_table_rows': {
  const path = argsRecord.path as string | undefined;
  if (!path) throw new Error('Missing required parameter: path');
  const res = await executeAutomationRequest(tools, 'data_list_rows', { path });
  return cleanObject(res as Record<string, unknown>) as Record<string, unknown>;
}
```
Run: PASS.

- [ ] **Step 3: C++ handler**

```cpp
TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleDataListRows(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    FString PathStr;
    if (!Params->TryGetStringField(TEXT("path"), PathStr))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Missing path"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }
    UDataTable* DT = LoadObject<UDataTable>(nullptr, *PathStr);
    if (!DT)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("DataTable not found"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }
    TArray<TSharedPtr<FJsonValue>> Names;
    for (const auto& Pair : DT->GetRowMap())
    {
        Names.Add(MakeShared<FJsonValueString>(Pair.Key.ToString()));
    }
    Response->SetBoolField(TEXT("success"), true);
    Response->SetArrayField(TEXT("rowNames"), Names);
    return Response;
}
```

Register: `data_list_rows`.

- [ ] **Step 4: Integration**
```javascript
{ scenario: 'Data: list DT rows', toolName: 'manage_data',
  arguments: { action: 'list_data_table_rows', path: '/Game/DataTest/DT_Ch2' },
  expected: 'success' },
```

- [ ] **Step 5: Compile + run**. PASS.

- [ ] **Step 6: Commit**
```bash
git add -u
git commit -m "feat(manage_data): add list_data_table_rows action

Lightweight name-only listing; skips field deserialization for large tables."
```

---

## Task 9: `set_data_table_row_struct` action (schema migration)

- [ ] **Step 1: Unit test**

```typescript
describe('manage_data: set_data_table_row_struct', () => {
  it('forwards path + newRowStructPath', async () => {
    const mockTools = { executeAutomation: vi.fn().mockResolvedValue({ success: true, rowsMigrated: 3 }) };
    const res = await handleDataTools(
      'set_data_table_row_struct',
      { path: '/Game/DT', newRowStructPath: '/Game/ST_New.ST_New' } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(res.rowsMigrated).toBe(3);
  });
});
```
Run: FAIL.

- [ ] **Step 2: TS handler**

```typescript
case 'set_data_table_row_struct': {
  const path = argsRecord.path as string | undefined;
  const newRowStructPath = argsRecord.newRowStructPath as string | undefined;
  if (!path) throw new Error('Missing required parameter: path');
  if (!newRowStructPath) throw new Error('Missing required parameter: newRowStructPath');
  const res = await executeAutomationRequest(tools, 'data_set_row_struct', { path, newRowStructPath });
  return cleanObject(res as Record<string, unknown>) as Record<string, unknown>;
}
```
Run: PASS.

- [ ] **Step 3: C++ handler**

```cpp
TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleDataSetRowStruct(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    FString PathStr, NewStructPath;
    if (!Params->TryGetStringField(TEXT("path"), PathStr) ||
        !Params->TryGetStringField(TEXT("newRowStructPath"), NewStructPath))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Missing path or newRowStructPath"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }
    UDataTable* DT = LoadObject<UDataTable>(nullptr, *PathStr);
    UScriptStruct* NewStruct = LoadObject<UScriptStruct>(nullptr, *NewStructPath);
    if (!DT || !NewStruct)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("DataTable or new struct not found"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }

    const int32 RowCount = DT->GetRowMap().Num();
    DT->CleanBeforeStructChange();
    DT->RowStruct = NewStruct;
    DT->OnDataTableChanged().Broadcast();
    DT->MarkPackageDirty();
    McpSafeAssetSave(DT);

    Response->SetBoolField(TEXT("success"), true);
    Response->SetNumberField(TEXT("rowsMigrated"), RowCount);
    return Response;
}
```

Register: `data_set_row_struct`.

- [ ] **Step 4: Integration**

Create a second struct first:
```javascript
{ scenario: 'Data prep: alt struct', toolName: 'manage_blueprint',
  arguments: { action: 'create_struct', path: '/Game/DataTest', name: 'ST_Ch2RowV2' },
  expected: 'success|already exists' },
// (add fields matching ST_Ch2Row + a new one for realism; or leave as-is for migration smoke)
{ scenario: 'Data: migrate DT row struct', toolName: 'manage_data',
  arguments: { action: 'set_data_table_row_struct', path: '/Game/DataTest/DT_Ch2', newRowStructPath: '/Game/DataTest/ST_Ch2RowV2.ST_Ch2RowV2' },
  expected: 'success' },
```

- [ ] **Step 5: Compile + run**. Expected PASS.

- [ ] **Step 6: Commit**
```bash
git add -u
git commit -m "feat(manage_data): add set_data_table_row_struct action

Swaps RowStruct via CleanBeforeStructChange + Broadcast. Data loss
warning: field values in removed columns are not preserved; call site
should copy rows out via get_data_table_rows if needed."
```

---

## Acceptance Checklist (Ch2)

- [ ] 9 commits (1 bootstrap + 8 actions)
- [ ] All data-handlers unit tests pass (`npx vitest run src/tools/handlers/data-handlers.test.ts`)
- [ ] 9+ new integration scenarios pass in live UE 5.7
- [ ] `/Game/DataTest/DT_Ch2` asset ends with the migrated `ST_Ch2RowV2` row struct
- [ ] No `as any`, no `UPackage::SavePackage`, no `ANY_PACKAGE`
- [ ] `McpStructReflection` exercised on `UUserDefinedStruct` successfully (GUID suffix handling works)

---

## Notes for Subagent

- **`modify_struct` action shape must be verified** — the integration test assumes `{ op: 'add_variable', varName, varType }`. If the actual schema differs (check `consolidated-tool-definitions.ts` for `modify_struct`), adapt the prep scenarios before running.
- **UE DataTable write path uses `FMemory::Malloc` / `FMemory::Free`** — the handler code here creates a temp struct instance. Verify no leaks with `stat malloc` in a debug build if worried.
- **`OnDataTableChanged` is multi-cast** — broadcasting it refreshes any open Editor windows but does NOT re-validate existing rows against the new struct. Callers should explicitly re-set rows after migration if field types changed.
