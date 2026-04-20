# Ch5 — `manage_curve` (4 actions)

> **Parent plan:** `2026-04-20-mcp-tier123-expansion.md`
> **Spec:** §4 Ch5
> **Depends on:** Task 0 (uses `McpGenericAssetFactory`)
> **Estimated:** 0.5 day, 5 commits

**Goal:** New `manage_curve` tool for `UCurveFloat` create/read/modify.

---

## Task 1: Bootstrap `manage_curve` tool

**Files:**
- Modify: `src/tools/consolidated-tool-definitions.ts`
- Create: `src/tools/handlers/curve-handlers.ts`
- Create: `src/tools/handlers/curve-handlers.test.ts`
- Modify: `src/tools/consolidated-tool-handlers.ts`
- Create: `plugins/.../Private/MCP/Tools/McpTool_ManageCurve.cpp`
- Create: `plugins/.../Private/McpAutomationBridge_CurveHandlers.cpp`

- [ ] **Step 1: Append tool definition**

```typescript
{
  name: 'manage_curve',
  category: 'authoring',
  description: 'Create and edit UCurveFloat assets (keyframe editing with interp modes).',
  inputSchema: {
    type: 'object',
    properties: {
      action: {
        type: 'string',
        enum: ['create_curve_float', 'set_curve_keys', 'get_curve_keys', 'inspect_curve']
      },
      path: { type: 'string', description: 'Package path (/Game/...).' },
      name: { type: 'string', description: 'Asset name (for create_curve_float).' },
      keys: {
        type: 'array',
        items: {
          type: 'object',
          properties: {
            time: { type: 'number' },
            value: { type: 'number' },
            interpMode: { type: 'string', enum: ['Auto', 'Linear', 'Constant', 'CubicBreak'] }
          },
          required: ['time', 'value']
        },
        description: 'Keyframes for set_curve_keys.'
      }
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
      keyCount: { type: 'number' },
      minTime: { type: 'number' },
      maxTime: { type: 'number' },
      keys: {
        type: 'array',
        items: { type: 'object', additionalProperties: true }
      }
    }
  }
}
```

- [ ] **Step 2: Handler skeleton + Task 2-5 all-at-once scaffolding**

`src/tools/handlers/curve-handlers.ts`:
```typescript
import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';

export async function handleCurveTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const argsRecord = args as Record<string, unknown>;
  switch (action) {
    default:
      throw new Error(`Unsupported manage_curve action: ${action}`);
  }
}
```

Route in `consolidated-tool-handlers.ts`:
```typescript
import { handleCurveTools } from './handlers/curve-handlers.js';
// ...
case 'manage_curve':
  return await handleCurveTools(action, args, tools);
```

- [ ] **Step 3: C++ tool dispatcher**

`plugins/.../Private/MCP/Tools/McpTool_ManageCurve.cpp`:
```cpp
#include "McpToolDefinition.h"
#include "McpToolRegistry.h"

namespace McpTool_ManageCurve
{
    static FMcpToolDefinition MakeDefinition()
    {
        FMcpToolDefinition Def;
        Def.Name = TEXT("manage_curve");
        Def.Category = TEXT("authoring");
        Def.Description = TEXT("UCurveFloat authoring.");
        Def.ActionMapping = {
            {TEXT("create_curve_float"), TEXT("curve_create_float")},
            {TEXT("set_curve_keys"), TEXT("curve_set_keys")},
            {TEXT("get_curve_keys"), TEXT("curve_get_keys")},
            {TEXT("inspect_curve"), TEXT("curve_inspect")}
        };
        return Def;
    }
    static struct FRegister { FRegister() { FMcpToolRegistry::Get().Register(MakeDefinition()); } } GRegister;
}
```

- [ ] **Step 4: Handlers file skeleton**

`plugins/.../Private/McpAutomationBridge_CurveHandlers.cpp`:
```cpp
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "MCP/Helpers/McpGenericAssetFactory.h"
#include "Curves/CurveFloat.h"
#include "Factories/CurveFloatFactory.h"
```

- [ ] **Step 5: Commit bootstrap**
```bash
git add -u src/ plugins/
git commit -m "feat(manage_curve): bootstrap tool skeleton"
```

---

## Task 2: `create_curve_float` action

- [ ] **Step 1: Unit test**

```typescript
import { describe, it, expect, vi } from 'vitest';
import { handleCurveTools } from './curve-handlers.js';

describe('manage_curve: create_curve_float', () => {
  it('forwards path + name', async () => {
    const mockTools = { executeAutomation: vi.fn().mockResolvedValue({ success: true, assetPath: '/Game/C' }) };
    const res = await handleCurveTools(
      'create_curve_float',
      { path: '/Game/Curves', name: 'C_Test' } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(res.success).toBe(true);
  });
});
```
Run: FAIL.

- [ ] **Step 2: TS handler**

```typescript
case 'create_curve_float': {
  const path = argsRecord.path as string | undefined;
  const name = argsRecord.name as string | undefined;
  if (!path) throw new Error('Missing required parameter: path');
  if (!name) throw new Error('Missing required parameter: name');
  const res = await executeAutomationRequest(tools, 'curve_create_float', { path, name });
  return cleanObject(res as Record<string, unknown>) as Record<string, unknown>;
}
```
Run: PASS.

- [ ] **Step 3: C++ handler**

```cpp
TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleCurveCreateFloat(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    FString PathStr, NameStr;
    if (!Params->TryGetStringField(TEXT("path"), PathStr) ||
        !Params->TryGetStringField(TEXT("name"), NameStr))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Missing path or name"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }

    FString OutError;
    UObject* NewCurve = McpGenericAssetFactory::CreateAssetOfClass(
        UCurveFloat::StaticClass(), PathStr, NameStr, nullptr, OutError);
    if (!NewCurve)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), OutError);
        Response->SetStringField(TEXT("errorCategory"), TEXT("EngineAPIError"));
        return Response;
    }
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("assetPath"), NewCurve->GetPathName());
    return Response;
}
```

Register: `curve_create_float`.

- [ ] **Step 4: Integration**
```javascript
{ scenario: 'Curve: create float curve', toolName: 'manage_curve',
  arguments: { action: 'create_curve_float', path: '/Game/DataTest', name: 'C_Ch5Test' },
  expected: 'success|already exists' },
```

- [ ] **Step 5: Compile + run**. Expected PASS.

- [ ] **Step 6: Commit**
```bash
git add -u
git commit -m "feat(manage_curve): add create_curve_float action"
```

---

## Task 3: `set_curve_keys` action

- [ ] **Step 1: Unit test**

```typescript
describe('manage_curve: set_curve_keys', () => {
  it('forwards keys array', async () => {
    const mockTools = { executeAutomation: vi.fn().mockResolvedValue({ success: true, keyCount: 3 }) };
    const res = await handleCurveTools(
      'set_curve_keys',
      { path: '/Game/C', keys: [{ time: 0, value: 0, interpMode: 'Linear' }, { time: 1, value: 1, interpMode: 'Auto' }] } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(res.success).toBe(true);
    expect(res.keyCount).toBe(3);
  });
});
```
Run: FAIL.

- [ ] **Step 2: TS handler**

```typescript
case 'set_curve_keys': {
  const path = argsRecord.path as string | undefined;
  const keys = argsRecord.keys as unknown[] | undefined;
  if (!path) throw new Error('Missing required parameter: path');
  if (!Array.isArray(keys)) throw new Error('Missing required parameter: keys (array)');
  const res = await executeAutomationRequest(tools, 'curve_set_keys', { path, keys });
  return cleanObject(res as Record<string, unknown>) as Record<string, unknown>;
}
```
Run: PASS.

- [ ] **Step 3: C++ handler**

```cpp
static ERichCurveInterpMode ParseInterpMode(const FString& S, ERichCurveTangentMode& OutTangent)
{
    OutTangent = RCTM_Auto;
    if (S.Equals(TEXT("Linear"), ESearchCase::IgnoreCase)) return RCIM_Linear;
    if (S.Equals(TEXT("Constant"), ESearchCase::IgnoreCase)) return RCIM_Constant;
    if (S.Equals(TEXT("CubicBreak"), ESearchCase::IgnoreCase)) { OutTangent = RCTM_Break; return RCIM_Cubic; }
    // Default Auto
    OutTangent = RCTM_Auto;
    return RCIM_Cubic;
}

TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleCurveSetKeys(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    FString PathStr;
    const TArray<TSharedPtr<FJsonValue>>* KeysArr = nullptr;
    if (!Params->TryGetStringField(TEXT("path"), PathStr) ||
        !Params->TryGetArrayField(TEXT("keys"), KeysArr))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Missing path or keys"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }
    UCurveFloat* Curve = LoadObject<UCurveFloat>(nullptr, *PathStr);
    if (!Curve)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Curve not found: %s"), *PathStr));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }

    Curve->FloatCurve.Reset();
    for (const auto& V : *KeysArr)
    {
        const TSharedPtr<FJsonObject>& Obj = V->AsObject();
        if (!Obj.IsValid()) continue;
        const double Time = Obj->GetNumberField(TEXT("time"));
        const double Value = Obj->GetNumberField(TEXT("value"));
        FString InterpStr;
        Obj->TryGetStringField(TEXT("interpMode"), InterpStr);
        ERichCurveTangentMode TangentMode;
        const ERichCurveInterpMode InterpMode = ParseInterpMode(InterpStr, TangentMode);

        const FKeyHandle Handle = Curve->FloatCurve.AddKey(Time, Value);
        Curve->FloatCurve.SetKeyInterpMode(Handle, InterpMode);
        Curve->FloatCurve.SetKeyTangentMode(Handle, TangentMode);
    }

    Curve->MarkPackageDirty();
    McpSafeAssetSave(Curve);

    Response->SetBoolField(TEXT("success"), true);
    Response->SetNumberField(TEXT("keyCount"), Curve->FloatCurve.GetNumKeys());
    return Response;
}
```

Register: `curve_set_keys`.

- [ ] **Step 4: Integration**
```javascript
{ scenario: 'Curve: set keys', toolName: 'manage_curve',
  arguments: { action: 'set_curve_keys', path: '/Game/DataTest/C_Ch5Test',
    keys: [{ time: 0, value: 0, interpMode: 'Linear' }, { time: 1, value: 1, interpMode: 'Auto' }, { time: 2, value: 0, interpMode: 'Constant' }] },
  expected: 'success' },
```

- [ ] **Step 5: Compile + run**. Expected PASS with `keyCount: 3`.

- [ ] **Step 6: Commit**
```bash
git add -u
git commit -m "feat(manage_curve): add set_curve_keys action

Resets FloatCurve and replays all keys with interp/tangent modes.
Supports Auto, Linear, Constant, CubicBreak per spec mapping."
```

---

## Task 4: `get_curve_keys` action

- [ ] **Step 1: Unit test**

```typescript
describe('manage_curve: get_curve_keys', () => {
  it('returns keys array', async () => {
    const mockTools = { executeAutomation: vi.fn().mockResolvedValue({
      success: true, keys: [{ time: 0, value: 0, interpMode: 'Linear' }]
    }) };
    const res = await handleCurveTools(
      'get_curve_keys',
      { path: '/Game/C' } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(Array.isArray(res.keys)).toBe(true);
  });
});
```
Run: FAIL.

- [ ] **Step 2: TS handler**

```typescript
case 'get_curve_keys': {
  const path = argsRecord.path as string | undefined;
  if (!path) throw new Error('Missing required parameter: path');
  const res = await executeAutomationRequest(tools, 'curve_get_keys', { path });
  return cleanObject(res as Record<string, unknown>) as Record<string, unknown>;
}
```
Run: PASS.

- [ ] **Step 3: C++ handler**

```cpp
static FString InterpModeToString(ERichCurveInterpMode M, ERichCurveTangentMode T)
{
    if (M == RCIM_Linear) return TEXT("Linear");
    if (M == RCIM_Constant) return TEXT("Constant");
    if (M == RCIM_Cubic && T == RCTM_Break) return TEXT("CubicBreak");
    return TEXT("Auto");
}

TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleCurveGetKeys(const TSharedPtr<FJsonObject>& Params)
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
    UCurveFloat* Curve = LoadObject<UCurveFloat>(nullptr, *PathStr);
    if (!Curve)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Curve not found: %s"), *PathStr));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }

    TArray<TSharedPtr<FJsonValue>> Keys;
    for (auto It = Curve->FloatCurve.GetKeyHandleIterator(); It; ++It)
    {
        const FKeyHandle Handle = *It;
        const FRichCurveKey& K = Curve->FloatCurve.GetKey(Handle);
        TSharedPtr<FJsonObject> KObj = MakeShared<FJsonObject>();
        KObj->SetNumberField(TEXT("time"), K.Time);
        KObj->SetNumberField(TEXT("value"), K.Value);
        KObj->SetStringField(TEXT("interpMode"), InterpModeToString(K.InterpMode, K.TangentMode));
        Keys.Add(MakeShared<FJsonValueObject>(KObj));
    }
    Response->SetBoolField(TEXT("success"), true);
    Response->SetArrayField(TEXT("keys"), Keys);
    return Response;
}
```

Register: `curve_get_keys`.

- [ ] **Step 4: Integration**
```javascript
{ scenario: 'Curve: get keys', toolName: 'manage_curve',
  arguments: { action: 'get_curve_keys', path: '/Game/DataTest/C_Ch5Test' },
  expected: 'success' },
```

- [ ] **Step 5: Compile + run**. Expected PASS; `keys.length == 3`.

- [ ] **Step 6: Commit**
```bash
git add -u
git commit -m "feat(manage_curve): add get_curve_keys action

Iterates FloatCurve key handles; maps ERichCurveInterpMode+TangentMode
back to the string schema (Auto/Linear/Constant/CubicBreak)."
```

---

## Task 5: `inspect_curve` action

- [ ] **Step 1: Unit test**

```typescript
describe('manage_curve: inspect_curve', () => {
  it('returns keyCount, minTime, maxTime', async () => {
    const mockTools = { executeAutomation: vi.fn().mockResolvedValue({
      success: true, keyCount: 3, minTime: 0, maxTime: 2, keys: []
    }) };
    const res = await handleCurveTools(
      'inspect_curve',
      { path: '/Game/C' } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(res.keyCount).toBe(3);
    expect(res.minTime).toBe(0);
    expect(res.maxTime).toBe(2);
  });
});
```
Run: FAIL.

- [ ] **Step 2: TS handler**

```typescript
case 'inspect_curve': {
  const path = argsRecord.path as string | undefined;
  if (!path) throw new Error('Missing required parameter: path');
  const res = await executeAutomationRequest(tools, 'curve_inspect', { path });
  return cleanObject(res as Record<string, unknown>) as Record<string, unknown>;
}
```
Run: PASS.

- [ ] **Step 3: C++ handler**

```cpp
TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleCurveInspect(const TSharedPtr<FJsonObject>& Params)
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
    UCurveFloat* Curve = LoadObject<UCurveFloat>(nullptr, *PathStr);
    if (!Curve)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Curve not found: %s"), *PathStr));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }
    const int32 Count = Curve->FloatCurve.GetNumKeys();
    float MinTime = 0, MaxTime = 0;
    Curve->FloatCurve.GetTimeRange(MinTime, MaxTime);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetNumberField(TEXT("keyCount"), Count);
    Response->SetNumberField(TEXT("minTime"), MinTime);
    Response->SetNumberField(TEXT("maxTime"), MaxTime);
    return Response;
}
```

Register: `curve_inspect`.

- [ ] **Step 4: Integration**
```javascript
{ scenario: 'Curve: inspect', toolName: 'manage_curve',
  arguments: { action: 'inspect_curve', path: '/Game/DataTest/C_Ch5Test' },
  expected: 'success' },
```

- [ ] **Step 5: Compile + run**. Expected PASS with `{keyCount: 3, minTime: 0, maxTime: 2}`.

- [ ] **Step 6: Commit**
```bash
git add -u
git commit -m "feat(manage_curve): add inspect_curve action"
```

---

## Acceptance Checklist (Ch5)

- [ ] 5 commits
- [ ] Unit tests pass
- [ ] 5 integration scenarios pass
- [ ] `C_Ch5Test` has 3 keys after Ch5; inspect reports time range [0, 2]
- [ ] Interp mode round-trips string → enum → string correctly

---

## Notes for Subagent

- **CurveVector / CurveLinearColor / CurveTable are NOT in scope** — defer to a follow-up ticket.
- **`FRichCurve::AddKey`** returns an `FKeyHandle`; retain and use it immediately to set interp/tangent before the next AddKey call.
- **TangentMode `RCTM_Break`** is the Cubic-with-asymmetric-tangents form; other tangent modes (User, Auto) are inferred per InterpMode. If you need finer control, extend the `interpMode` enum in a follow-up.
