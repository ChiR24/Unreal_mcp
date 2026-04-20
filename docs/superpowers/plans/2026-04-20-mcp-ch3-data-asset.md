# Ch3 — `manage_data` DataAsset (4 actions)

> **Parent plan:** `2026-04-20-mcp-tier123-expansion.md`
> **Spec:** §4 Ch3
> **Depends on:** Ch2 merged (reuses `data-handlers.ts`, `McpAutomationBridge_DataHandlers.cpp`, `McpPropertyPath`)
> **Estimated:** 0.5 day, 4 commits

**Goal:** Extend `manage_data` tool with UDataAsset instance CRUD + nested property-path access. First consumer of `McpPropertyPath`.

---

## Task 1: `create_data_asset` action

**Files:**
- Modify: `src/tools/handlers/data-handlers.ts`
- Modify: `src/tools/handlers/data-handlers.test.ts`
- Modify: `plugins/.../Private/McpAutomationBridge_DataHandlers.cpp`
- Modify: `plugins/.../Private/McpAutomationBridgeSubsystem.cpp`
- Modify: `tests/integration.mjs`

- [ ] **Step 1: Failing unit test**

```typescript
describe('manage_data: create_data_asset', () => {
  it('forwards path + name + dataAssetClassPath', async () => {
    const mockTools = { executeAutomation: vi.fn().mockResolvedValue({
      success: true, assetPath: '/Game/DA/DA_Item'
    }) };
    const res = await handleDataTools(
      'create_data_asset',
      { path: '/Game/DA', name: 'DA_Item', dataAssetClassPath: '/Game/BP_ItemData.BP_ItemData_C' } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(res.success).toBe(true);
    expect(res.assetPath).toBe('/Game/DA/DA_Item');
  });
});
```
Run: FAIL.

- [ ] **Step 2: TS handler**

```typescript
case 'create_data_asset': {
  const path = argsRecord.path as string | undefined;
  const name = argsRecord.name as string | undefined;
  const dataAssetClassPath = argsRecord.dataAssetClassPath as string | undefined;
  if (!path) throw new Error('Missing required parameter: path');
  if (!name) throw new Error('Missing required parameter: name');
  if (!dataAssetClassPath) throw new Error('Missing required parameter: dataAssetClassPath');
  const res = await executeAutomationRequest(tools, 'data_create_asset', { path, name, dataAssetClassPath });
  return cleanObject(res as Record<string, unknown>) as Record<string, unknown>;
}
```
Run: PASS.

- [ ] **Step 3: C++ handler**

```cpp
TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleDataCreateAsset(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    FString PathStr, NameStr, ClassPathStr;
    if (!Params->TryGetStringField(TEXT("path"), PathStr) ||
        !Params->TryGetStringField(TEXT("name"), NameStr) ||
        !Params->TryGetStringField(TEXT("dataAssetClassPath"), ClassPathStr))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Missing path/name/dataAssetClassPath"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }

    UClass* AssetClass = LoadObject<UClass>(nullptr, *ClassPathStr);
    if (!AssetClass)
    {
        FString Normalized = ClassPathStr;
        if (!Normalized.Contains(TEXT(".")))
        {
            int32 SlashIdx = INDEX_NONE;
            if (Normalized.FindLastChar(TEXT('/'), SlashIdx))
            {
                Normalized = Normalized + TEXT(".") + Normalized.Mid(SlashIdx + 1) + TEXT("_C");
            }
        }
        AssetClass = LoadObject<UClass>(nullptr, *Normalized);
    }
    if (!AssetClass || !AssetClass->IsChildOf(UDataAsset::StaticClass()))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Not a UDataAsset subclass: %s"), *ClassPathStr));
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }

    FString OutError;
    UObject* NewAsset = McpGenericAssetFactory::CreateAssetOfClass(
        AssetClass, PathStr, NameStr, nullptr, OutError);
    if (!NewAsset)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), OutError);
        Response->SetStringField(TEXT("errorCategory"), TEXT("EngineAPIError"));
        return Response;
    }

    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("assetPath"), NewAsset->GetPathName());
    return Response;
}
```

Register: `data_create_asset` → `HandleDataCreateAsset`.

- [ ] **Step 4: Integration**

Need a UDataAsset BP class first:
```javascript
{ scenario: 'DA prep: create DataAsset parent BP', toolName: 'manage_blueprint',
  arguments: { action: 'create', name: 'BP_Ch3ItemData', path: '/Game/DataTest', parentClass: '/Script/Engine.DataAsset' },
  expected: 'success|already exists' },
{ scenario: 'DA: create data asset instance', toolName: 'manage_data',
  arguments: { action: 'create_data_asset', path: '/Game/DataTest', name: 'DA_Ch3Sword', dataAssetClassPath: '/Game/DataTest/BP_Ch3ItemData.BP_Ch3ItemData_C' },
  expected: 'success|already exists' },
```

- [ ] **Step 5: Compile + run**. Expected PASS.

- [ ] **Step 6: Commit**
```bash
git add -u
git commit -m "feat(manage_data): add create_data_asset action

Creates UDataAsset subclass instance via McpGenericAssetFactory.
Validates class ancestry; normalizes /Game/Foo → /Game/Foo.Foo_C."
```

---

## Task 2: `set_data_asset_property` action

- [ ] **Step 1: Unit test**

```typescript
describe('manage_data: set_data_asset_property', () => {
  it('forwards path + propertyPath + value', async () => {
    const mockTools = { executeAutomation: vi.fn().mockResolvedValue({ success: true }) };
    await handleDataTools(
      'set_data_asset_property',
      { path: '/Game/DA_T', propertyPath: 'Stats.Health', value: 100 } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(mockTools.executeAutomation).toHaveBeenCalledWith('data_set_asset_prop', expect.objectContaining({
      propertyPath: 'Stats.Health', value: 100
    }));
  });
  it('accepts nested array path', async () => {
    const mockTools = { executeAutomation: vi.fn().mockResolvedValue({ success: true }) };
    await handleDataTools(
      'set_data_asset_property',
      { path: '/Game/DA_T', propertyPath: 'Effects.[0].Value', value: 3.14 } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(mockTools.executeAutomation).toHaveBeenCalled();
  });
});
```
Run: FAIL.

- [ ] **Step 2: TS handler**

```typescript
case 'set_data_asset_property': {
  const path = argsRecord.path as string | undefined;
  const propertyPath = argsRecord.propertyPath as string | undefined;
  const value = argsRecord.value;
  if (!path) throw new Error('Missing required parameter: path');
  if (!propertyPath) throw new Error('Missing required parameter: propertyPath');
  if (value === undefined) throw new Error('Missing required parameter: value');
  const res = await executeAutomationRequest(tools, 'data_set_asset_prop', { path, propertyPath, value });
  return cleanObject(res as Record<string, unknown>) as Record<string, unknown>;
}
```
Run: PASS.

- [ ] **Step 3: C++ handler**

```cpp
TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleDataSetAssetProp(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    FString PathStr, PropPath;
    if (!Params->TryGetStringField(TEXT("path"), PathStr) ||
        !Params->TryGetStringField(TEXT("propertyPath"), PropPath))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Missing path or propertyPath"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }
    const TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("value"));
    if (!Value.IsValid())
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Missing value"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }

    UObject* Asset = LoadObject<UObject>(nullptr, *PathStr);
    if (!Asset)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Asset not found: %s"), *PathStr));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }

    FString WalkError;
    if (!McpPropertyPath::SetValueAtPath(Asset, PropPath, Value, WalkError))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), WalkError);
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }

    McpSafeAssetSave(Asset);
    Response->SetBoolField(TEXT("success"), true);
    return Response;
}
```

Register: `data_set_asset_prop`.

- [ ] **Step 4: Integration**

(First you need a property on `BP_Ch3ItemData` — add a variable via existing `manage_blueprint/add_variable` in a prep scenario; or rely on an existing engine DataAsset. Simplest: add scalar `Value: double` variable to BP_Ch3ItemData.)

```javascript
{ scenario: 'DA prep: add Value variable to DA BP', toolName: 'manage_blueprint',
  arguments: { action: 'add_variable', blueprintPath: '/Game/DataTest/BP_Ch3ItemData', name: 'Value', type: 'double' },
  expected: 'success|already exists' },
{ scenario: 'DA: set Value property', toolName: 'manage_data',
  arguments: { action: 'set_data_asset_property', path: '/Game/DataTest/DA_Ch3Sword', propertyPath: 'Value', value: 42.5 },
  expected: 'success' },
```

- [ ] **Step 5: Compile + run**. Expected PASS.

- [ ] **Step 6: Commit**
```bash
git add -u
git commit -m "feat(manage_data): add set_data_asset_property action

Uses McpPropertyPath to walk dotted/indexed paths and write JSON values
via reflection. Supports nested structs and TArray element writes."
```

---

## Task 3: `get_data_asset_property` action

- [ ] **Step 1: Unit test**

```typescript
describe('manage_data: get_data_asset_property', () => {
  it('returns value for propertyPath', async () => {
    const mockTools = { executeAutomation: vi.fn().mockResolvedValue({ success: true, value: 42.5 }) };
    const res = await handleDataTools(
      'get_data_asset_property',
      { path: '/Game/DA_T', propertyPath: 'Value' } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(res.value).toBe(42.5);
  });
});
```
Run: FAIL.

- [ ] **Step 2: TS handler**

```typescript
case 'get_data_asset_property': {
  const path = argsRecord.path as string | undefined;
  const propertyPath = argsRecord.propertyPath as string | undefined;
  if (!path) throw new Error('Missing required parameter: path');
  if (!propertyPath) throw new Error('Missing required parameter: propertyPath');
  const res = await executeAutomationRequest(tools, 'data_get_asset_prop', { path, propertyPath });
  return cleanObject(res as Record<string, unknown>) as Record<string, unknown>;
}
```
Run: PASS.

- [ ] **Step 3: C++ handler**

```cpp
TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleDataGetAssetProp(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    FString PathStr, PropPath;
    if (!Params->TryGetStringField(TEXT("path"), PathStr) ||
        !Params->TryGetStringField(TEXT("propertyPath"), PropPath))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Missing path or propertyPath"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }
    UObject* Asset = LoadObject<UObject>(nullptr, *PathStr);
    if (!Asset)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Asset not found: %s"), *PathStr));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }
    FString WalkError;
    TSharedPtr<FJsonValue> Val = McpPropertyPath::GetValueAtPath(Asset, PropPath, WalkError);
    if (!Val.IsValid())
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), WalkError);
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }
    Response->SetBoolField(TEXT("success"), true);
    Response->SetField(TEXT("value"), Val);
    return Response;
}
```

Register: `data_get_asset_prop`.

- [ ] **Step 4: Integration**
```javascript
{ scenario: 'DA: get Value property', toolName: 'manage_data',
  arguments: { action: 'get_data_asset_property', path: '/Game/DataTest/DA_Ch3Sword', propertyPath: 'Value' },
  expected: 'success' },
```

- [ ] **Step 5: Compile + run**. Expected PASS with `value: 42.5`.

- [ ] **Step 6: Commit**
```bash
git add -u
git commit -m "feat(manage_data): add get_data_asset_property action"
```

---

## Task 4: `list_data_assets_of_class` action

- [ ] **Step 1: Unit test**

```typescript
describe('manage_data: list_data_assets_of_class', () => {
  it('returns asset paths array', async () => {
    const mockTools = { executeAutomation: vi.fn().mockResolvedValue({ success: true, assets: ['/Game/DA_A', '/Game/DA_B'] }) };
    const res = await handleDataTools(
      'list_data_assets_of_class',
      { classPath: '/Game/BP_ItemData.BP_ItemData_C' } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(res.assets).toEqual(['/Game/DA_A', '/Game/DA_B']);
  });
});
```
Run: FAIL.

- [ ] **Step 2: TS handler**

```typescript
case 'list_data_assets_of_class': {
  const classPath = argsRecord.classPath as string | undefined;
  const searchPaths = argsRecord.searchPaths as string[] | undefined;
  if (!classPath) throw new Error('Missing required parameter: classPath');
  const res = await executeAutomationRequest(tools, 'data_list_assets_of_class', { classPath, searchPaths });
  return cleanObject(res as Record<string, unknown>) as Record<string, unknown>;
}
```
Run: PASS.

- [ ] **Step 3: C++ handler**

```cpp
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleDataListAssetsOfClass(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    FString ClassPath;
    if (!Params->TryGetStringField(TEXT("classPath"), ClassPath))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Missing classPath"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }
    UClass* Target = LoadObject<UClass>(nullptr, *ClassPath);
    if (!Target)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Class not found: %s"), *ClassPath));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }

    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    FARFilter Filter;
    Filter.bRecursiveClasses = true;
    Filter.ClassPaths.Add(Target->GetClassPathName());

    const TArray<TSharedPtr<FJsonValue>>* PathsArr = nullptr;
    if (Params->TryGetArrayField(TEXT("searchPaths"), PathsArr))
    {
        for (const auto& V : *PathsArr) Filter.PackagePaths.Add(FName(*V->AsString()));
        Filter.bRecursivePaths = true;
    }

    TArray<FAssetData> Results;
    ARM.Get().GetAssets(Filter, Results);

    TArray<TSharedPtr<FJsonValue>> Paths;
    for (const FAssetData& AD : Results)
    {
        Paths.Add(MakeShared<FJsonValueString>(AD.GetObjectPathString()));
    }
    Response->SetBoolField(TEXT("success"), true);
    Response->SetArrayField(TEXT("assets"), Paths);
    return Response;
}
```

Register: `data_list_assets_of_class`.

Add `AssetRegistry` to `PrivateDependencyModuleNames` in `McpAutomationBridge.Build.cs` if not already.

- [ ] **Step 4: Integration**
```javascript
{ scenario: 'DA: list assets of class', toolName: 'manage_data',
  arguments: { action: 'list_data_assets_of_class', classPath: '/Game/DataTest/BP_Ch3ItemData.BP_Ch3ItemData_C' },
  expected: 'success' },
```

- [ ] **Step 5: Compile + run**. Expected PASS with `assets` containing `/Game/DataTest/DA_Ch3Sword`.

- [ ] **Step 6: Commit**
```bash
git add -u
git commit -m "feat(manage_data): add list_data_assets_of_class action

AssetRegistry query with recursive class matching and optional
searchPaths scoping to /Game/ subtrees."
```

---

## Acceptance Checklist (Ch3)

- [ ] 4 commits
- [ ] All DataAsset unit tests pass
- [ ] 4+ new integration scenarios pass
- [ ] `DA_Ch3Sword.Value` round-trips 42.5 through set/get
- [ ] `list_data_assets_of_class` finds `DA_Ch3Sword`
- [ ] No `as any` / `SavePackage` / `ANY_PACKAGE`

---

## Notes for Subagent

- **`McpPropertyPath` array-index semantics**: the walker in Task 0 treats `ArrayIndex >= 0` segments as dereference-to-element. If array element is a struct and you write `.[0].X`, that's TWO segments: `[0]` then `X`. Verify path parser handles this.
- **`value` field of type `any`**: do NOT coerce to string in TS. Pass through as-is; JsonValue on C++ side preserves the shape.
- **AssetRegistry prescan**: if integration test fails with empty `assets`, it may be because AssetRegistry is still scanning. Call `ARM.Get().SearchAllAssets(/*bSynchronousSearch=*/true)` before the query if reliability is critical.
