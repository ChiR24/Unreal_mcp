# Inspect Reflection Symmetry (Phase 1) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:**补齐 `inspect` 工具族的反射对称性 — 让 `inspect_class` / `inspect_function` / `list_graph_nodes` / `get_widget_info` 真正可用于诊断第三方 codegen 插件（Puerts AutoMode 等）生成的 `UTypeScriptGeneratedClass` 蓝图。

**Architecture:** 纯只读反射扩展，TS 侧路由 + 参数过滤透传，C++ 侧用 `TFieldIterator` / `UEdGraph::Nodes` / `UWidgetTree->RootWidget` 递归组装。默认 `selfOnly`（不展开继承链）以控制 payload 尺寸；`includeInherited` + `*Filter` 参数开放更宽过滤。

**Tech Stack:** TypeScript (NodeNext ESM) + vitest，C++ UE 5.0-5.7 plugin（`McpAutomationBridge`），现有 `executeAutomationRequest` WebSocket bridge。

---

## 文件结构

### 创建
- `src/tools/handlers/inspect-handlers.test.ts` — vitest 单元测试，覆盖 4 个 action 的参数路由

### 修改
- `src/tools/consolidated-tool-definitions.ts` — `inspect` 枚举增 `inspect_function`；`manage_blueprint` 枚举增 `list_graph_nodes`；schema 增新字段
- `src/tools/handlers/inspect-handlers.ts:644-688` — `case 'inspect_class'` 透传新参数；新增 `case 'inspect_function'`
- `src/tools/handlers/blueprint-handlers.ts:295` 附近 — 新增 `case 'list_graph_nodes'`
- `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Tools/McpTool_Inspect.cpp:27-85` — schema 注册新字段 + 新 action 枚举
- `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Tools/McpTool_ManageBlueprint.cpp:50` 附近 — action 枚举增 `list_graph_nodes`
- `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_EnvironmentHandlers.cpp:1721-1757` — 重写 `inspect_class` 分支，加入 `BuildFunctionsArray` / `BuildPropertiesArray` helper；新增 `inspect_function` 分支
- `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_BlueprintGraphHandlers.cpp` — 新增 `list_graph_nodes` sub-action 分支
- `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_WidgetAuthoringHandlers.cpp:1768-1822` — `get_widget_info` 递归构造 `tree` 字段

### 测试命令
- TS 单元测试: `npm run test:unit -- inspect-handlers`
- TS 全量构建: `npm run build:core`
- C++ 集成测试: 需 Editor，见 Task 5 手动验证步骤

---

## Task 1: `inspect_class detailed=true` 返回 functions / properties / interfaces（含 filter）

**Files:**
- Modify: `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Tools/McpTool_Inspect.cpp:65-83`
- Modify: `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_EnvironmentHandlers.cpp:1721-1757`
- Modify: `src/tools/consolidated-tool-definitions.ts:886-922`
- Modify: `src/tools/handlers/inspect-handlers.ts:644-688`
- Create: `src/tools/handlers/inspect-handlers.test.ts`

- [ ] **Step 1.1: 写失败的 TS 单元测试**

创建 `src/tools/handlers/inspect-handlers.test.ts`：

```typescript
import { beforeEach, describe, expect, it, vi } from 'vitest';

const { executeAutomationRequestMock } = vi.hoisted(() => ({
  executeAutomationRequestMock: vi.fn(async () => ({ success: true, functions: [], properties: [] }))
}));

vi.mock('./common-handlers.js', () => ({
  executeAutomationRequest: executeAutomationRequestMock,
  requireNonEmptyString: (value: unknown, fieldName: string) => {
    if (typeof value !== 'string' || value.trim().length === 0) {
      throw new Error(`Missing required parameter: ${fieldName}`);
    }
  }
}));

import { handleInspectTools } from './inspect-handlers.js';

describe('inspect_class detailed reflection', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockClear();
  });

  it('forwards detailed/includeInherited/functionFilter to bridge', async () => {
    await handleInspectTools(
      'inspect_class',
      {
        action: 'inspect_class',
        className: '/Game/Blueprints/Foo.Foo_C',
        detailed: true,
        includeInherited: true,
        functionFilter: 'OnPaint',
        functionFlagFilter: ['FUNC_BlueprintEvent'],
        propertyFilter: 'bIs'
      },
      {} as never
    );

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'inspect',
      expect.objectContaining({
        action: 'inspect_class',
        className: '/Game/Blueprints/Foo.Foo_C',
        detailed: true,
        includeInherited: true,
        functionFilter: 'OnPaint',
        functionFlagFilter: ['FUNC_BlueprintEvent'],
        propertyFilter: 'bIs'
      })
    );
  });

  it('defaults detailed/includeInherited to undefined when not provided', async () => {
    await handleInspectTools(
      'inspect_class',
      { action: 'inspect_class', className: 'Actor' },
      {} as never
    );
    const call = executeAutomationRequestMock.mock.calls[0][2] as Record<string, unknown>;
    expect(call.detailed).toBeUndefined();
    expect(call.includeInherited).toBeUndefined();
    expect(call.functionFilter).toBeUndefined();
  });
});
```

- [ ] **Step 1.2: 跑测试确认失败**

Run: `npm run test:unit -- inspect-handlers`
Expected: FAIL — `detailed / includeInherited / functionFilter / functionFlagFilter / propertyFilter` 未出现在 mock call args 中。

- [ ] **Step 1.3: TS 侧修改 `inspect-handlers.ts` 的 `inspect_class` case**

Modify `src/tools/handlers/inspect-handlers.ts:644-688`，替换整个 `case 'inspect_class'` 块：

```typescript
    case 'inspect_class': {
      const params = normalizeArgs(args, [
        { key: 'className', aliases: ['classPath'], required: true }
      ]);
      let className = extractString(params, 'className');

      if (className && !className.includes('/') && !className.includes('.')) {
        if (className === 'Landscape') {
          className = '/Script/Landscape.Landscape';
        } else if (['Actor', 'Pawn', 'Character', 'StaticMeshActor'].includes(className)) {
          className = `/Script/Engine.${className}`;
        }
      }

      const argsRecord = args as Record<string, unknown>;
      const payload: Record<string, unknown> = {
        action: 'inspect_class',
        className,
        detailed: argsRecord.detailed as boolean | undefined,
        includeInherited: argsRecord.includeInherited as boolean | undefined,
        functionFilter: argsRecord.functionFilter as string | undefined,
        functionFlagFilter: argsRecord.functionFlagFilter as string[] | undefined,
        propertyFilter: argsRecord.propertyFilter as string | undefined
      };

      const res = await executeAutomationRequest(tools, 'inspect', payload) as InspectResponse;
      if (!res || res.success === false) {
        const originalClassName = typeof argsTyped.className === 'string' ? argsTyped.className : '';
        if (originalClassName && !originalClassName.includes('/') && !className.startsWith('/Script/')) {
          const retryName = `/Script/Engine.${originalClassName}`;
          const resRetry = await executeAutomationRequest(tools, 'inspect', {
            ...payload, className: retryName
          }) as InspectResponse;
          if (resRetry && resRetry.success) {
            return cleanObject(resRetry);
          }
        }
        return cleanObject({
          success: false,
          error: res?.error || 'OPERATION_FAILED',
          message: res?.message || `inspect_class failed for '${className}'`,
          className,
          cdo: res?.cdo ?? null
        });
      }
      return cleanObject(res);
    }
```

- [ ] **Step 1.4: 扩展 `consolidated-tool-definitions.ts` inspect schema**

Modify `src/tools/consolidated-tool-definitions.ts:884-922`，在 `inspect` 的 `inputSchema.properties` 里新增（紧接 `propertyNames: commonSchemas.arrayOfStrings` 之后）：

```typescript
        propertyNames: commonSchemas.arrayOfStrings,
        includeInherited: commonSchemas.booleanProp,
        functionFilter: commonSchemas.stringProp,
        functionFlagFilter: commonSchemas.arrayOfStrings,
        propertyFilter: commonSchemas.stringProp
```

在同一对象 `outputSchema.properties` 里新增：

```typescript
        properties: commonSchemas.objectProp,
        functions: commonSchemas.arrayOfObjects,
        interfaces: commonSchemas.arrayOfStrings
```

并更新 `inspect` 的 `description`（第 883 行）末尾加一句：
`' When detailed=true, inspect_class returns functions/properties/interfaces arrays (filter via functionFilter/functionFlagFilter/propertyFilter, includeInherited=true expands super chain).'`

- [ ] **Step 1.5: 跑 TS 单元测试确认通过**

Run: `npm run test:unit -- inspect-handlers`
Expected: PASS — 2 tests pass.

也跑 `npm run build:core`
Expected: PASS — 零 TypeScript 错误。

- [ ] **Step 1.6: 扩展 C++ schema**

Modify `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Tools/McpTool_Inspect.cpp:82`，在 `.Array(TEXT("propertyNames"), TEXT(""))` 之后插入：

```cpp
				.Array(TEXT("propertyNames"), TEXT(""))
				.Bool(TEXT("includeInherited"), TEXT("inspect_class: include super-class members (default false)."))
				.String(TEXT("functionFilter"), TEXT("inspect_class: case-insensitive substring filter on function name."))
				.Array(TEXT("functionFlagFilter"), TEXT("inspect_class: AND-match flag names (e.g. FUNC_BlueprintEvent)."))
				.String(TEXT("propertyFilter"), TEXT("inspect_class: case-insensitive substring filter on property name."))
```

- [ ] **Step 1.7: 在 `EnvironmentHandlers.cpp` 顶部加反射 helper**

Modify `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_EnvironmentHandlers.cpp`，在文件开头 `#include` 区域之后、任何 namespace/handler 之前（找一个 `namespace McpInspectReflection` 不存在的位置），加入：

```cpp
#include "UObject/UnrealType.h"
#include "UObject/Class.h"
#include "UObject/Interface.h"

namespace McpInspectReflection
{
    static TArray<FString> DescribeFunctionFlags(EFunctionFlags Flags)
    {
        TArray<FString> Out;
        if (Flags & FUNC_Final)            Out.Add(TEXT("FUNC_Final"));
        if (Flags & FUNC_BlueprintCallable) Out.Add(TEXT("FUNC_BlueprintCallable"));
        if (Flags & FUNC_BlueprintEvent)   Out.Add(TEXT("FUNC_BlueprintEvent"));
        if (Flags & FUNC_BlueprintPure)    Out.Add(TEXT("FUNC_BlueprintPure"));
        if (Flags & FUNC_Event)            Out.Add(TEXT("FUNC_Event"));
        if (Flags & FUNC_Native)           Out.Add(TEXT("FUNC_Native"));
        if (Flags & FUNC_Net)              Out.Add(TEXT("FUNC_Net"));
        if (Flags & FUNC_NetServer)        Out.Add(TEXT("FUNC_NetServer"));
        if (Flags & FUNC_NetClient)        Out.Add(TEXT("FUNC_NetClient"));
        if (Flags & FUNC_NetMulticast)     Out.Add(TEXT("FUNC_NetMulticast"));
        if (Flags & FUNC_Static)           Out.Add(TEXT("FUNC_Static"));
        if (Flags & FUNC_Exec)             Out.Add(TEXT("FUNC_Exec"));
        if (Flags & FUNC_Public)           Out.Add(TEXT("FUNC_Public"));
        if (Flags & FUNC_Protected)        Out.Add(TEXT("FUNC_Protected"));
        if (Flags & FUNC_Private)          Out.Add(TEXT("FUNC_Private"));
        return Out;
    }

    static bool MatchesFlagFilter(EFunctionFlags Flags, const TArray<FString>& Required)
    {
        for (const FString& Name : Required)
        {
            if (Name.Equals(TEXT("FUNC_BlueprintCallable"), ESearchCase::IgnoreCase) && !(Flags & FUNC_BlueprintCallable)) return false;
            if (Name.Equals(TEXT("FUNC_BlueprintEvent"),    ESearchCase::IgnoreCase) && !(Flags & FUNC_BlueprintEvent))    return false;
            if (Name.Equals(TEXT("FUNC_BlueprintPure"),     ESearchCase::IgnoreCase) && !(Flags & FUNC_BlueprintPure))     return false;
            if (Name.Equals(TEXT("FUNC_Event"),             ESearchCase::IgnoreCase) && !(Flags & FUNC_Event))             return false;
            if (Name.Equals(TEXT("FUNC_Native"),            ESearchCase::IgnoreCase) && !(Flags & FUNC_Native))            return false;
            if (Name.Equals(TEXT("FUNC_Net"),               ESearchCase::IgnoreCase) && !(Flags & FUNC_Net))               return false;
            if (Name.Equals(TEXT("FUNC_Static"),            ESearchCase::IgnoreCase) && !(Flags & FUNC_Static))            return false;
        }
        return true;
    }

    static TSharedPtr<FJsonObject> DescribeFunction(UFunction* Func, UClass* TargetClass)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("name"), Func->GetName());
        UClass* Owner = Func->GetOwnerClass();
        Obj->SetStringField(TEXT("definedIn"), Owner ? Owner->GetName() : TEXT("<none>"));
        Obj->SetStringField(TEXT("definedInPath"), Owner ? Owner->GetPathName() : TEXT(""));
        Obj->SetBoolField(TEXT("isInherited"), Owner != TargetClass);

        Obj->SetNumberField(TEXT("flagsMask"), static_cast<double>(Func->FunctionFlags));
        TArray<TSharedPtr<FJsonValue>> FlagNames;
        for (const FString& F : DescribeFunctionFlags(Func->FunctionFlags))
        {
            FlagNames.Add(MakeShared<FJsonValueString>(F));
        }
        Obj->SetArrayField(TEXT("flagNames"), FlagNames);

        TArray<TSharedPtr<FJsonValue>> Params;
        FString ReturnTypeStr = TEXT("void");
        for (TFieldIterator<FProperty> PIt(Func); PIt; ++PIt)
        {
            FProperty* P = *PIt;
            const bool bReturn = P->HasAnyPropertyFlags(CPF_ReturnParm);
            const bool bOut    = P->HasAnyPropertyFlags(CPF_OutParm) && !bReturn;
            const bool bRef    = P->HasAnyPropertyFlags(CPF_ReferenceParm);
            FString TypeCpp = P->GetCPPType();
            if (bReturn)
            {
                ReturnTypeStr = TypeCpp;
                continue;
            }
            TSharedPtr<FJsonObject> ParamObj = MakeShared<FJsonObject>();
            ParamObj->SetStringField(TEXT("name"), P->GetName());
            ParamObj->SetStringField(TEXT("type"), TypeCpp);
            ParamObj->SetBoolField(TEXT("isOut"), bOut);
            ParamObj->SetBoolField(TEXT("isRef"), bRef);
            Params.Add(MakeShared<FJsonValueObject>(ParamObj));
        }
        Obj->SetArrayField(TEXT("parameters"), Params);
        Obj->SetStringField(TEXT("returnType"), ReturnTypeStr);

        Obj->SetBoolField(TEXT("hasScript"), Func->Script.Num() > 0);
        Obj->SetNumberField(TEXT("scriptBytecodeSize"), static_cast<double>(Func->Script.Num()));

        return Obj;
    }

    static TSharedPtr<FJsonObject> DescribeProperty(FProperty* Prop, UClass* TargetClass)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("name"), Prop->GetName());
        Obj->SetStringField(TEXT("type"), Prop->GetCPPType());
        UClass* Owner = Prop->GetOwnerClass();
        Obj->SetStringField(TEXT("definedIn"), Owner ? Owner->GetName() : TEXT("<none>"));
        Obj->SetBoolField(TEXT("isInherited"), Owner != TargetClass);
        Obj->SetNumberField(TEXT("flagsMask"), static_cast<double>(static_cast<uint64>(Prop->PropertyFlags)));
        TArray<TSharedPtr<FJsonValue>> FlagNames;
        if (Prop->PropertyFlags & CPF_Edit)          FlagNames.Add(MakeShared<FJsonValueString>(TEXT("CPF_Edit")));
        if (Prop->PropertyFlags & CPF_BlueprintVisible) FlagNames.Add(MakeShared<FJsonValueString>(TEXT("CPF_BlueprintVisible")));
        if (Prop->PropertyFlags & CPF_BlueprintReadOnly) FlagNames.Add(MakeShared<FJsonValueString>(TEXT("CPF_BlueprintReadOnly")));
        if (Prop->PropertyFlags & CPF_Net)           FlagNames.Add(MakeShared<FJsonValueString>(TEXT("CPF_Net")));
        if (Prop->PropertyFlags & CPF_Transient)     FlagNames.Add(MakeShared<FJsonValueString>(TEXT("CPF_Transient")));
        Obj->SetArrayField(TEXT("flagNames"), FlagNames);
        return Obj;
    }
}
```

- [ ] **Step 1.8: 重写 `inspect_class` 分支**

Modify `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_EnvironmentHandlers.cpp:1721-1757`，替换整个 `else if (LowerSubAction.Equals(TEXT("inspect_class")))` 块：

```cpp
        else if (LowerSubAction.Equals(TEXT("inspect_class")))
        {
            FString ClassName;
            Payload->TryGetStringField(TEXT("className"), ClassName);
            if (ClassName.IsEmpty())
            {
                SendAutomationError(RequestingSocket, RequestId,
                                    TEXT("className is required for inspect_class"),
                                    TEXT("INVALID_ARGUMENT"));
                return true;
            }

            UClass* TargetClass = FindObject<UClass>(nullptr, *ClassName);
            if (!TargetClass && !ClassName.Contains(TEXT(".")))
            {
                TargetClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *ClassName));
            }
            if (!TargetClass)
            {
                SendAutomationError(RequestingSocket, RequestId,
                                    FString::Printf(TEXT("Class not found: %s"), *ClassName),
                                    TEXT("CLASS_NOT_FOUND"));
                return true;
            }

            Resp->SetStringField(TEXT("className"), TargetClass->GetName());
            Resp->SetStringField(TEXT("classPath"), TargetClass->GetPathName());
            Resp->SetStringField(TEXT("parentClass"), TargetClass->GetSuperClass() ? TargetClass->GetSuperClass()->GetName() : TEXT("None"));
            Resp->SetStringField(TEXT("parentClassPath"), TargetClass->GetSuperClass() ? TargetClass->GetSuperClass()->GetPathName() : TEXT(""));

            bool bDetailed = false;
            Payload->TryGetBoolField(TEXT("detailed"), bDetailed);
            if (bDetailed)
            {
                bool bIncludeInherited = false;
                Payload->TryGetBoolField(TEXT("includeInherited"), bIncludeInherited);
                const EFieldIteratorFlags::SuperClassFlags SuperFlag = bIncludeInherited
                    ? EFieldIteratorFlags::IncludeSuper
                    : EFieldIteratorFlags::ExcludeSuper;

                FString FuncFilter;
                Payload->TryGetStringField(TEXT("functionFilter"), FuncFilter);
                FString PropFilter;
                Payload->TryGetStringField(TEXT("propertyFilter"), PropFilter);

                TArray<FString> FlagFilter;
                const TArray<TSharedPtr<FJsonValue>>* FlagFilterJson = nullptr;
                if (Payload->TryGetArrayField(TEXT("functionFlagFilter"), FlagFilterJson) && FlagFilterJson)
                {
                    for (const TSharedPtr<FJsonValue>& V : *FlagFilterJson)
                    {
                        if (V.IsValid()) FlagFilter.Add(V->AsString());
                    }
                }

                TArray<TSharedPtr<FJsonValue>> FunctionsArr;
                for (TFieldIterator<UFunction> It(TargetClass, SuperFlag); It; ++It)
                {
                    UFunction* Func = *It;
                    if (!FuncFilter.IsEmpty() && !Func->GetName().Contains(FuncFilter)) continue;
                    if (FlagFilter.Num() > 0 && !McpInspectReflection::MatchesFlagFilter(Func->FunctionFlags, FlagFilter)) continue;
                    FunctionsArr.Add(MakeShared<FJsonValueObject>(McpInspectReflection::DescribeFunction(Func, TargetClass)));
                }
                Resp->SetArrayField(TEXT("functions"), FunctionsArr);

                TArray<TSharedPtr<FJsonValue>> PropsArr;
                for (TFieldIterator<FProperty> It(TargetClass, SuperFlag); It; ++It)
                {
                    FProperty* Prop = *It;
                    if (!PropFilter.IsEmpty() && !Prop->GetName().Contains(PropFilter)) continue;
                    PropsArr.Add(MakeShared<FJsonValueObject>(McpInspectReflection::DescribeProperty(Prop, TargetClass)));
                }
                Resp->SetArrayField(TEXT("properties"), PropsArr);

                TArray<TSharedPtr<FJsonValue>> InterfacesArr;
                for (const FImplementedInterface& Impl : TargetClass->Interfaces)
                {
                    if (Impl.Class)
                    {
                        InterfacesArr.Add(MakeShared<FJsonValueString>(Impl.Class->GetName()));
                    }
                }
                Resp->SetArrayField(TEXT("interfaces"), InterfacesArr);
            }

            Resp->SetBoolField(TEXT("success"), true);
            SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("Class inspected"), Resp, FString());
            return true;
        }
```

- [ ] **Step 1.9: Commit Task 1**

```bash
git add src/tools/handlers/inspect-handlers.ts src/tools/handlers/inspect-handlers.test.ts src/tools/consolidated-tool-definitions.ts plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Tools/McpTool_Inspect.cpp plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_EnvironmentHandlers.cpp
git commit -m "$(cat <<'EOF'
feat(inspect): inspect_class detailed=true returns functions/properties/interfaces with filters

- C++: TFieldIterator walk of UFunction/FProperty on TargetClass
- Default selfOnly (ExcludeSuper); includeInherited=true walks super chain
- functionFilter / functionFlagFilter / propertyFilter applied server-side
- Flags exposed as both mask + readable name array (FUNC_BlueprintEvent etc.)
- Enables diagnosis of Puerts AutoMode BIE override registration

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: 新 action `inspect_function`

**Files:**
- Modify: `src/tools/consolidated-tool-definitions.ts:889-899` (inspect action enum)
- Modify: `src/tools/handlers/inspect-handlers.ts` (新增 case)
- Modify: `src/tools/handlers/inspect-handlers.test.ts` (新增测试)
- Modify: `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Tools/McpTool_Inspect.cpp:31-64` (action enum)
- Modify: `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_EnvironmentHandlers.cpp` (新增分支)

- [ ] **Step 2.1: 扩展测试 — `inspect_function` 路由**

追加到 `src/tools/handlers/inspect-handlers.test.ts` 的最后 `});` 之前（新增一个 `describe` 块）：

```typescript
describe('inspect_function routing', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockClear();
    executeAutomationRequestMock.mockResolvedValue({
      success: true,
      function: { name: 'OnPaint', definedIn: 'PaperBackground_C' }
    });
  });

  it('forwards className + functionName to bridge', async () => {
    await handleInspectTools(
      'inspect_function',
      { action: 'inspect_function', className: 'PaperBackground_C', functionName: 'OnPaint' },
      {} as never
    );
    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'inspect',
      expect.objectContaining({
        action: 'inspect_function',
        className: 'PaperBackground_C',
        functionName: 'OnPaint'
      })
    );
  });

  it('throws when functionName missing', async () => {
    await expect(
      handleInspectTools(
        'inspect_function',
        { action: 'inspect_function', className: 'Foo' },
        {} as never
      )
    ).rejects.toThrow(/functionName/);
  });
});
```

- [ ] **Step 2.2: 跑测试确认失败**

Run: `npm run test:unit -- inspect-handlers`
Expected: FAIL — `inspect_function` 走 default 分支，不会抛 `functionName` 错误。

- [ ] **Step 2.3: TS handler 新增 `inspect_function` case**

Modify `src/tools/handlers/inspect-handlers.ts`，在 `case 'inspect_cdo': { ... }` 之后（约 697 行后）新增：

```typescript
    case 'inspect_function': {
      const params = normalizeArgs(args, [
        { key: 'className', aliases: ['classPath'], required: true },
        { key: 'functionName', aliases: ['memberName'], required: true }
      ]);
      const className = extractString(params, 'className');
      const functionName = extractString(params, 'functionName');

      const argsRecord = args as Record<string, unknown>;
      const res = await executeAutomationRequest(tools, 'inspect', {
        action: 'inspect_function',
        className,
        functionName,
        includeInherited: argsRecord.includeInherited as boolean | undefined
      }) as InspectResponse;
      return cleanObject(res);
    }
```

- [ ] **Step 2.4: 扩展 TS action 枚举 + schema**

Modify `src/tools/consolidated-tool-definitions.ts:889-899`，在枚举中增 `'inspect_function'`，例如（第 894 行前后）：

```typescript
            'inspect_class', 'inspect_function', 'inspect_cdo', 'list_objects',
```

在 inspect 的 `inputSchema.properties` 中（与 Task 1 的新字段同段）新增：

```typescript
        functionName: commonSchemas.stringProp,
```

在 `outputSchema.properties` 中新增：

```typescript
        function: commonSchemas.objectProp,
```

- [ ] **Step 2.5: 跑 TS 单元测试确认通过**

Run: `npm run test:unit -- inspect-handlers`
Expected: PASS — 所有测试通过。

Run: `npm run build:core`
Expected: PASS — TS 编译无错。

- [ ] **Step 2.6: 扩展 C++ action 枚举 + schema**

Modify `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Tools/McpTool_Inspect.cpp:44-45`，在 `TEXT("inspect_class"),` 与 `TEXT("inspect_cdo"),` 之间插入：

```cpp
					TEXT("inspect_class"),
					TEXT("inspect_function"),
					TEXT("inspect_cdo"),
```

在 Task 1.6 插入的 schema 字段块后追加：

```cpp
				.String(TEXT("functionName"), TEXT("inspect_function: UFunction name to introspect."))
```

- [ ] **Step 2.7: C++ 新增 `inspect_function` 分支**

Modify `McpAutomationBridge_EnvironmentHandlers.cpp`，在 Task 1.8 新写的 `inspect_class` 分支之后、`inspect_cdo` 分支之前，插入：

```cpp
        else if (LowerSubAction.Equals(TEXT("inspect_function")))
        {
            FString ClassName, FuncName;
            Payload->TryGetStringField(TEXT("className"), ClassName);
            Payload->TryGetStringField(TEXT("functionName"), FuncName);
            if (ClassName.IsEmpty() || FuncName.IsEmpty())
            {
                SendAutomationError(RequestingSocket, RequestId,
                                    TEXT("className and functionName are required for inspect_function"),
                                    TEXT("INVALID_ARGUMENT"));
                return true;
            }

            UClass* TargetClass = FindObject<UClass>(nullptr, *ClassName);
            if (!TargetClass && !ClassName.Contains(TEXT(".")))
            {
                TargetClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *ClassName));
            }
            if (!TargetClass)
            {
                SendAutomationError(RequestingSocket, RequestId,
                                    FString::Printf(TEXT("Class not found: %s"), *ClassName),
                                    TEXT("CLASS_NOT_FOUND"));
                return true;
            }

            bool bIncludeInherited = true;
            Payload->TryGetBoolField(TEXT("includeInherited"), bIncludeInherited);

            UFunction* Found = nullptr;
            const EFieldIteratorFlags::SuperClassFlags SuperFlag = bIncludeInherited
                ? EFieldIteratorFlags::IncludeSuper
                : EFieldIteratorFlags::ExcludeSuper;
            for (TFieldIterator<UFunction> It(TargetClass, SuperFlag); It; ++It)
            {
                if (It->GetName().Equals(FuncName, ESearchCase::IgnoreCase))
                {
                    Found = *It;
                    break;
                }
            }
            if (!Found)
            {
                Resp->SetBoolField(TEXT("success"), false);
                Resp->SetStringField(TEXT("error"), TEXT("FUNCTION_NOT_FOUND"));
                Resp->SetStringField(TEXT("message"),
                    FString::Printf(TEXT("Function '%s' not found on class '%s' (includeInherited=%s)"),
                                    *FuncName, *ClassName, bIncludeInherited ? TEXT("true") : TEXT("false")));
                SendAutomationResponse(RequestingSocket, RequestId, false,
                                       TEXT("Function lookup failed"), Resp, TEXT("FUNCTION_NOT_FOUND"));
                return true;
            }

            Resp->SetBoolField(TEXT("success"), true);
            Resp->SetObjectField(TEXT("function"), McpInspectReflection::DescribeFunction(Found, TargetClass));
            SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("Function inspected"), Resp, FString());
            return true;
        }
```

同时在本 handler 顶部（约 1436 行，global actions list）把 `inspect_function` 加进"全局 actions"列表：

```cpp
        LowerSubAction.Equals(TEXT("inspect_class")) ||
        LowerSubAction.Equals(TEXT("inspect_function")) ||
        LowerSubAction.Equals(TEXT("inspect_cdo"));
```

- [ ] **Step 2.8: Commit Task 2**

```bash
git add src/tools/handlers/inspect-handlers.ts src/tools/handlers/inspect-handlers.test.ts src/tools/consolidated-tool-definitions.ts plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Tools/McpTool_Inspect.cpp plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_EnvironmentHandlers.cpp
git commit -m "$(cat <<'EOF'
feat(inspect): add inspect_function action for single-UFunction introspection

- Returns definedIn / flags / parameters / returnType / script bytecode size
- Supports includeInherited (default true) for walking super chain
- Enables pinpoint diagnosis: "does PaperBackground_C.OnPaint exist, and from which class?"

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: 新 action `list_graph_nodes` 挂在 `manage_blueprint`

**Files:**
- Modify: `src/tools/handlers/blueprint-handlers.ts` (新增 case)
- Modify: `src/tools/handlers/inspect-handlers.test.ts` (复用现有测试文件，新增 describe)
- Modify: `src/tools/consolidated-tool-definitions.ts` (manage_blueprint action enum + schema)
- Modify: `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Tools/McpTool_ManageBlueprint.cpp` (action enum)
- Modify: `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_BlueprintGraphHandlers.cpp` (新增分支)

- [ ] **Step 3.1: 写失败的 TS 测试**

创建 `src/tools/handlers/blueprint-handlers.test.ts`（若不存在）：

```typescript
import { beforeEach, describe, expect, it, vi } from 'vitest';

const { executeAutomationRequestMock } = vi.hoisted(() => ({
  executeAutomationRequestMock: vi.fn(async () => ({ success: true, nodes: [] }))
}));

vi.mock('./common-handlers.js', () => ({
  executeAutomationRequest: executeAutomationRequestMock,
  requireNonEmptyString: (value: unknown, fieldName: string) => {
    if (typeof value !== 'string' || value.trim().length === 0) {
      throw new Error(`Missing required parameter: ${fieldName}`);
    }
  }
}));

import { handleBlueprintTools } from './blueprint-handlers.js';

describe('manage_blueprint list_graph_nodes', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockClear();
  });

  it('forwards blueprintPath + graphName to bridge', async () => {
    await handleBlueprintTools(
      'list_graph_nodes',
      {
        action: 'list_graph_nodes',
        blueprintPath: '/Game/UI/WBP_Foo',
        graphName: 'EventGraph'
      },
      {} as never
    );

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'manage_blueprint_graph',
      expect.objectContaining({
        subAction: 'list_nodes',
        assetPath: '/Game/UI/WBP_Foo',
        graphName: 'EventGraph'
      })
    );
  });
});
```

- [ ] **Step 3.2: 跑测试确认失败**

Run: `npm run test:unit -- blueprint-handlers`
Expected: FAIL — `list_graph_nodes` 走 default 分支。

- [ ] **Step 3.3: TS handler 新增 case**

Modify `src/tools/handlers/blueprint-handlers.ts`，在 `case 'add_node':` 之前（约第 294 行）新增：

```typescript
    case 'list_graph_nodes': {
      const res = await executeAutomationRequest(tools, 'manage_blueprint_graph', {
        subAction: 'list_nodes',
        assetPath: argsTyped.name || argsTyped.blueprintPath || (argsRecord.path as string) || '',
        graphName: argsTyped.graphName,
        nameFilter: argsRecord.nameFilter as string | undefined,
        classFilter: argsRecord.classFilter as string | undefined,
        timeoutMs: argsRecord.timeoutMs as number | undefined
      }) as Record<string, unknown>;
      return cleanObject(res);
    }
```

- [ ] **Step 3.4: 扩展 manage_blueprint TS schema**

Modify `src/tools/consolidated-tool-definitions.ts`，找 `manage_blueprint` 定义（`name: 'manage_blueprint'`），在 action enum 里加 `'list_graph_nodes'`（紧邻 `'add_node'`）；在 properties 里加：

```typescript
        nameFilter: commonSchemas.stringProp,
        classFilter: commonSchemas.stringProp,
```

在 outputSchema.properties 加：

```typescript
        nodes: commonSchemas.arrayOfObjects,
```

- [ ] **Step 3.5: 跑 TS 测试 + 构建**

Run: `npm run test:unit -- blueprint-handlers`
Expected: PASS.

Run: `npm run build:core`
Expected: PASS.

- [ ] **Step 3.6: 扩展 C++ action 枚举**

Modify `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Tools/McpTool_ManageBlueprint.cpp:50` 附近，在现有 `TEXT("add_node"),` 之前或之后新增：

```cpp
				TEXT("list_graph_nodes"),
```

- [ ] **Step 3.7: C++ 新增 `list_nodes` sub-action 分支**

在 `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_BlueprintGraphHandlers.cpp` 中找到现有 `create_node` 分支（用 grep `SubAction.Equals(TEXT("create_node"))` 定位），在其附近追加新分支：

```cpp
    else if (SubAction.Equals(TEXT("list_nodes"), ESearchCase::IgnoreCase))
    {
        FString AssetPath;
        Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
        FString GraphName;
        Payload->TryGetStringField(TEXT("graphName"), GraphName);
        FString NameFilter;
        Payload->TryGetStringField(TEXT("nameFilter"), NameFilter);
        FString ClassFilter;
        Payload->TryGetStringField(TEXT("classFilter"), ClassFilter);

        UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *AssetPath);
        if (!BP)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath),
                TEXT("BLUEPRINT_NOT_FOUND"));
            return true;
        }

        UEdGraph* TargetGraph = nullptr;
        if (GraphName.IsEmpty()) GraphName = TEXT("EventGraph");
        for (UEdGraph* Graph : BP->UbergraphPages)
        {
            if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
            {
                TargetGraph = Graph;
                break;
            }
        }
        if (!TargetGraph)
        {
            for (UEdGraph* Graph : BP->FunctionGraphs)
            {
                if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
                {
                    TargetGraph = Graph;
                    break;
                }
            }
        }
        if (!TargetGraph)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Graph '%s' not found on blueprint '%s'"), *GraphName, *AssetPath),
                TEXT("GRAPH_NOT_FOUND"));
            return true;
        }

        TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
        Resp->SetStringField(TEXT("blueprintPath"), BP->GetPathName());
        Resp->SetStringField(TEXT("graphName"), TargetGraph->GetName());

        TArray<TSharedPtr<FJsonValue>> NodesArr;
        for (UEdGraphNode* Node : TargetGraph->Nodes)
        {
            if (!Node) continue;
            const FString NodeName = Node->GetName();
            const FString NodeClass = Node->GetClass()->GetName();
            if (!NameFilter.IsEmpty() && !NodeName.Contains(NameFilter) && !Node->GetNodeTitle(ENodeTitleType::ListView).ToString().Contains(NameFilter)) continue;
            if (!ClassFilter.IsEmpty() && !NodeClass.Contains(ClassFilter)) continue;

            TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
            NodeObj->SetStringField(TEXT("nodeId"), Node->NodeGuid.ToString());
            NodeObj->SetStringField(TEXT("nodeClass"), NodeClass);
            NodeObj->SetStringField(TEXT("nodeName"), NodeName);
            NodeObj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
            NodeObj->SetNumberField(TEXT("posX"), Node->NodePosX);
            NodeObj->SetNumberField(TEXT("posY"), Node->NodePosY);

            if (UK2Node_CallFunction* CallFn = Cast<UK2Node_CallFunction>(Node))
            {
                NodeObj->SetStringField(TEXT("functionName"), CallFn->FunctionReference.GetMemberName().ToString());
                if (UClass* MemberParent = CallFn->FunctionReference.GetMemberParentClass())
                {
                    NodeObj->SetStringField(TEXT("functionParent"), MemberParent->GetName());
                }
            }
            else if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
            {
                NodeObj->SetStringField(TEXT("eventName"), EventNode->EventReference.GetMemberName().ToString());
                if (UClass* MemberParent = EventNode->EventReference.GetMemberParentClass())
                {
                    NodeObj->SetStringField(TEXT("eventParent"), MemberParent->GetName());
                }
                NodeObj->SetBoolField(TEXT("bOverrideFunction"), EventNode->bOverrideFunction);
            }

            NodesArr.Add(MakeShared<FJsonValueObject>(NodeObj));
        }
        Resp->SetArrayField(TEXT("nodes"), NodesArr);
        Resp->SetNumberField(TEXT("nodeCount"), NodesArr.Num());
        Resp->SetBoolField(TEXT("success"), true);

        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Graph nodes listed"), Resp, FString());
        return true;
    }
```

若该文件顶部未 include `K2Node_CallFunction.h` / `K2Node_Event.h`，需补：

```cpp
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
```

- [ ] **Step 3.8: Commit Task 3**

```bash
git add src/tools/handlers/blueprint-handlers.ts src/tools/handlers/blueprint-handlers.test.ts src/tools/consolidated-tool-definitions.ts plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Tools/McpTool_ManageBlueprint.cpp plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_BlueprintGraphHandlers.cpp
git commit -m "$(cat <<'EOF'
feat(manage_blueprint): add list_graph_nodes action for graph introspection

- Enumerates nodes in EventGraph/FunctionGraphs with nodeId / class / title / position
- Specializes K2Node_CallFunction (functionName + functionParent) and K2Node_Event (eventName + eventParent + bOverrideFunction) for call-chain diagnosis
- Supports nameFilter / classFilter for targeted queries
- Provides read-after-write symmetry vs existing add_node

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: `get_widget_info` 返回 tree 拓扑

**Files:**
- Modify: `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_WidgetAuthoringHandlers.cpp:1768-1822`
- Modify: `src/tools/consolidated-tool-definitions.ts` (widget_authoring outputSchema)

- [ ] **Step 4.1: 写失败的 TS 单元测试**（验证 TS 侧透传，不动 C++）

追加到 `src/tools/handlers/widget-authoring-handlers.test.ts`（若文件不存在则创建）：

```typescript
import { beforeEach, describe, expect, it, vi } from 'vitest';

const { executeAutomationRequestMock } = vi.hoisted(() => ({
  executeAutomationRequestMock: vi.fn(async () => ({
    success: true,
    widgetInfo: { tree: { name: 'Root', class: 'UCanvasPanel', children: [] } }
  }))
}));

vi.mock('./common-handlers.js', () => ({
  executeAutomationRequest: executeAutomationRequestMock,
  requireNonEmptyString: (value: unknown, fieldName: string) => {
    if (typeof value !== 'string' || value.trim().length === 0) {
      throw new Error(`Missing required parameter: ${fieldName}`);
    }
  }
}));

import { handleWidgetAuthoringTools } from './widget-authoring-handlers.js';

describe('widget_authoring get_widget_info tree response', () => {
  beforeEach(() => executeAutomationRequestMock.mockClear());

  it('passes through tree field in widgetInfo', async () => {
    const result = await handleWidgetAuthoringTools(
      'get_widget_info',
      { action: 'get_widget_info', widgetPath: '/Game/UI/WBP_Foo' },
      {} as never
    ) as { widgetInfo?: { tree?: unknown } };

    expect(result.widgetInfo?.tree).toBeDefined();
  });
});
```

- [ ] **Step 4.2: 跑测试确认通过（透传已成立）**

Run: `npm run test:unit -- widget-authoring`
Expected: PASS — `widgetInfo.tree` 已在 mock 返回中透传。该测试的主要作用是固化输出 shape 契约。

- [ ] **Step 4.3: C++ 替换 `get_widget_info` 实现**

Modify `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_WidgetAuthoringHandlers.cpp:1768-1822`，替换整个 `if (SubAction.Equals(TEXT("get_widget_info"), ESearchCase::IgnoreCase))` 块为：

```cpp
    if (SubAction.Equals(TEXT("get_widget_info"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        TSharedPtr<FJsonObject> WidgetInfo = McpHandlerUtils::CreateResultObject();
        WidgetInfo->SetStringField(TEXT("widgetClass"), WidgetBP->GetName());
        if (WidgetBP->ParentClass)
        {
            WidgetInfo->SetStringField(TEXT("parentClass"), WidgetBP->ParentClass->GetName());
        }

        TArray<TSharedPtr<FJsonValue>> SlotsArray;
        if (WidgetBP->WidgetTree)
        {
            WidgetBP->WidgetTree->ForEachWidget([&](UWidget* Widget) {
                SlotsArray.Add(MakeShared<FJsonValueString>(Widget->GetName()));
            });
        }
        WidgetInfo->SetArrayField(TEXT("slots"), SlotsArray);

        // Recursive tree builder
        TFunction<TSharedPtr<FJsonObject>(UWidget*)> BuildNode;
        BuildNode = [&BuildNode](UWidget* W) -> TSharedPtr<FJsonObject>
        {
            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            if (!W) { return Obj; }
            Obj->SetStringField(TEXT("name"), W->GetName());
            Obj->SetStringField(TEXT("class"), W->GetClass()->GetName());
            if (W->Slot)
            {
                Obj->SetStringField(TEXT("slotClass"), W->Slot->GetClass()->GetName());
            }

            TArray<TSharedPtr<FJsonValue>> Children;
            if (UPanelWidget* Panel = Cast<UPanelWidget>(W))
            {
                const int32 N = Panel->GetChildrenCount();
                for (int32 i = 0; i < N; ++i)
                {
                    if (UWidget* Child = Panel->GetChildAt(i))
                    {
                        Children.Add(MakeShared<FJsonValueObject>(BuildNode(Child)));
                    }
                }
            }
            else if (UNamedSlot* NamedSlot = Cast<UNamedSlot>(W))
            {
                if (UWidget* Content = NamedSlot->GetContent())
                {
                    Children.Add(MakeShared<FJsonValueObject>(BuildNode(Content)));
                }
            }
            Obj->SetArrayField(TEXT("children"), Children);
            return Obj;
        };

        if (WidgetBP->WidgetTree && WidgetBP->WidgetTree->RootWidget)
        {
            WidgetInfo->SetObjectField(TEXT("tree"), BuildNode(WidgetBP->WidgetTree->RootWidget));
        }
        else
        {
            WidgetInfo->SetObjectField(TEXT("tree"), MakeShared<FJsonObject>());
        }

        TArray<TSharedPtr<FJsonValue>> AnimsArray;
        for (UWidgetAnimation* Anim : WidgetBP->Animations)
        {
            if (Anim)
            {
                AnimsArray.Add(MakeShared<FJsonValueString>(Anim->GetName()));
            }
        }
        WidgetInfo->SetArrayField(TEXT("animations"), AnimsArray);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetObjectField(TEXT("widgetInfo"), WidgetInfo);

        McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Retrieved widget info"), ResultJson);
        return true;
    }
```

若文件未 include `UNamedSlot`，需在文件顶部补：

```cpp
#include "Components/NamedSlot.h"
#include "Components/PanelWidget.h"
```

- [ ] **Step 4.4: 更新 widget_authoring outputSchema（加 tree 字段文档）**

Modify `src/tools/consolidated-tool-definitions.ts`，找 `manage_widget_authoring` 的 `outputSchema.properties`，在 `widgetInfo` 对象描述处（或相关 slots 字段之后）补：

```typescript
        widgetInfo: commonSchemas.objectProp, // tree: {name, class, slotClass, children: [...]}
```

（若 `widgetInfo` 字段已存在则保持；该 comment 仅作 agent-facing 文档。）

- [ ] **Step 4.5: Commit Task 4**

```bash
git add plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_WidgetAuthoringHandlers.cpp src/tools/handlers/widget-authoring-handlers.test.ts src/tools/consolidated-tool-definitions.ts
git commit -m "$(cat <<'EOF'
feat(widget_authoring): get_widget_info returns hierarchical tree

- Adds `tree` field to widgetInfo with recursive {name, class, slotClass, children}
- Handles UPanelWidget.GetChildAt and UNamedSlot.GetContent
- Preserves flat `slots` array for backward compat

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: 集成验证（手动，需 Editor）

**Files:** 无代码改动，产出 `docs/superpowers/specs/` 下的验证记录（可选）

- [ ] **Step 5.1: 启动 Editor + Bridge + MCP server**

Run: （在 UE Editor 中启用 McpAutomationBridge 插件，启动 Play 或至少打开 Editor 使 Subsystem 激活）
Run: `npm run build:core && npm start`（若项目有 `start` script，否则按 README 启动 MCP server）

- [ ] **Step 5.2: 验证 `inspect_class detailed=true`**

通过 MCP client 或 `tests/test-runner.mjs` 脚本调用：

```json
{
  "tool": "inspect",
  "args": {
    "action": "inspect_class",
    "className": "/Script/UMG.UserWidget",
    "detailed": true,
    "functionFilter": "OnPaint"
  }
}
```

Expected: `success: true`，`functions` 数组含至少一个 `{name: "OnPaint", flagNames: [..."FUNC_BlueprintEvent"...], parameters: [{name: "Context", type: "FPaintContext", isOut: true, isRef: true}], ...}`。

- [ ] **Step 5.3: 验证 `inspect_function`**

```json
{"tool": "inspect", "args": {"action": "inspect_function", "className": "/Script/UMG.UserWidget", "functionName": "OnPaint"}}
```

Expected: `success: true, function: {name: "OnPaint", definedIn: "UserWidget", flagNames: [..."FUNC_BlueprintEvent"...], ...}`。

- [ ] **Step 5.4: 验证 `list_graph_nodes`**

先创建任意 WBP 子类（或用现有 WBP），graph 中确保至少一个 CallFunction 节点。调用：

```json
{"tool": "manage_blueprint", "args": {"action": "list_graph_nodes", "blueprintPath": "/Game/YourWBP", "graphName": "EventGraph"}}
```

Expected: `success: true, nodes: [{nodeId: "...", nodeClass: "K2Node_CallFunction", functionName: "...", ...}]`。

- [ ] **Step 5.5: 验证 `get_widget_info` tree**

```json
{"tool": "manage_widget_authoring", "args": {"action": "get_widget_info", "widgetPath": "/Game/YourWBP"}}
```

Expected: `widgetInfo.tree.name == RootWidget 名字`，`tree.children` 含嵌套子 widget。

- [ ] **Step 5.6: 需求方验收用例（Puerts 场景）**

按 `docs/superpowers/specs/2026-04-20-hex-map-editor-2d-ink-wash-design.md` T9.1 跑：

```
inspect_class className=/Game/Blueprints/TypeScript/.../PaperBackground.PaperBackground_C \
              detailed=true functionFilter=OnPaint includeInherited=true
```

根据返回结论判断 Puerts 根因（见原 issue "诊断分支决策树"），不在本 plan scope 内修复 Puerts 侧。

---

## Self-Review Checklist

已核对：
- [x] **Spec coverage**：原 issue 需求（revised v2）的 `inspect_class detailed` + `inspect_function` + `list_graph_nodes` + `get_widget_info tree` 四项全覆盖。
- [x] **No placeholders**：每个 Step 给出完整代码或明确命令。
- [x] **Type consistency**：`functions[] / function.flagNames / nodes[].nodeId / widgetInfo.tree` 字段名跨 Task 统一。
- [x] **Backward compat**：`inspect_class` 不传 `detailed` 保持原 3 字段响应；`get_widget_info.slots` 保留。
- [x] **Security / side effects**：全部只读反射，无 `UPackage::SavePackage`，无 asset 写入，符合 CLAUDE.md UE 5.7 安全约束。

---

## Out-of-scope（明确不做）

- `manage_widget_authoring` 对 `UTypeScriptGeneratedClass` 的 error message 改进（延后至 Phase 2）
- `K2Node_CallParentFunction.FunctionReference` 绑定（原 v1 issue P0，v2 撤回，待 Phase 1 诊断能力完工后再评估）
- PuertsEditor watcher 触发 / TS uasset 枚举（跨 scope，不在本仓库职责内）
- `inspect_function` 暴露 `nativeFuncPtrHex` 指针值（决策 3 B，已拒绝）
