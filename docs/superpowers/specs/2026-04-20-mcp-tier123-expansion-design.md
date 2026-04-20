# MCP Tier 1+2+3 Expansion — Design Spec

**Date:** 2026-04-20
**Author:** Goni
**Status:** Draft (awaiting implementation)
**Target UE Version:** 5.7 (single-version focus)
**Target Test Project:** `D:\Unreal\Project\War`
**Estimated Effort:** ~5 工作日(8 章节,垂直切片推进)

---

## 1. 背景与目标

### 1.1 背景

本 MCP 项目(`unreal-engine`)当前提供 35 个 consolidated tools。War 项目在推进 P1-P7 路线图时,发现以下关键能力缺口,阻塞 Task 8 及后续阶段:

- 无法创建 / 操作 **UDataTable**(P1 Task 8 阻塞)
- 无法创建 / 操作 **UDataAsset 实例**(P2 Event 系统预期)
- 无法管理 **GameplayTag**(P3 Faction/Character 业务类)
- Blueprint 无法 **reparent** 或 **add/remove Interface**
- 无法创建 / 编辑 **Curve**(P4 战斗曲线)
- UMG 无通用 `add_widget(customClass)` 逃生舱
- StateTree 已在 `manage_ai` 部分覆盖但实现浅(缺 contextClass、stateType、taskProps、inspect、remove)

### 1.2 目标

**一次规划完整 + 分章节 ship**,补齐上述所有真实缺口(Tier 1+2+3)。不包括 Tier 4 polish(MPC / Niagara param / CSV import / GAS 扩展 — 已有覆盖)。

### 1.3 非目标

- **不**支持 UE 5.0-5.6(API 兼容编译守卫足矣,不跑跨版本集成测试)
- **不**做 CSV import/export、Curve Vector/LinearColor、CurveTable、Material Parameter Collection、Niagara parameter edit
- **不**改动现有 35 个 tool 的对外签名(只加 action 或扩 optional 字段)

---

## 2. 范围

### 2.1 工具变更总览

| 变更类型 | Tool | 新 Action 数 |
|---------|------|-------------|
| **新建** | `manage_data`(DataTable + DataAsset 合并) | 12 |
| **新建** | `manage_gameplay_tags` | 4 |
| **新建** | `manage_curve` | 4 |
| **扩展** | `manage_blueprint`(reparent/interface 补丁) | 4 |
| **扩展** | `manage_widget_authoring`(通用 add_widget) | 2 |
| **扩展** | `manage_ai`(StateTree 補齐) | 3-5(Ch7a audit 后定) |
| **合计** | | **~29-31 新 action** |

### 2.2 决策日志

| 决策点 | 选择 | 备选 | 拍板理由 |
|--------|------|------|---------|
| 规划范围 | Tier 1+2+3(B) | A(只 Tier 1+2)/ C(全 Tier) | 用户指定,一次规划完整,章节分 ship |
| Tool 组织 | X:3 新 + 3 扩 | Y:4 新 / Z:1 巨型新 tool | 同源合并 + 独立范式不混,不复刻 `manage_asset` 沼泽 |
| UE 版本 | 5.7 单版本(α) | β 双版本 / γ 全矩阵 | 跨版本 ROI 低,War 项目锁 5.7 |
| 章节组织 | 垂直切片(方式 3) | 按 tool / 按架构层 | 及早反馈 + 失败隔离 + 符合项目既有模式 |
| 测试部署 | 符号链接 `War/Plugins/McpAutomationBridge` → 本 repo | 拷贝 / 仅 repo 测 | 开发迭代快,避免双目录同步漂移 |
| Ch7 拆分 | 7a audit + 7b 执行 | 直接 Ch7 | StateTree 实现深度未知,先审计降风险 |
| CSV import/export | 不做 | 做 | Task 8 不需要,P4+ 才用 |
| `remove_gameplay_tag` | 做,走 GConfig 直接读写 ini | 不做 | 测试需清理;API 无官方 remove,hack 可接受 |

---

## 3. 架构与共享脚手架

### 3.1 新增共享 C++ 层

在 `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Helpers/` 下:

**`McpStructReflection.h/cpp`**
```cpp
namespace McpStructReflection {
    bool SetStructFieldFromJson(
        const UStruct* Struct,
        void* StructInstance,
        FName FieldName,
        const TSharedPtr<FJsonValue>& Value,
        FString& OutError);

    TSharedPtr<FJsonObject> StructInstanceToJson(
        const UStruct* Struct,
        const void* StructInstance);

    // UUserDefinedStruct 字段名有 GUID 后缀(DisplayName_2_ABCD1234)
    // 映射 logical name → internal name
    FName ResolveUserDefinedStructFieldName(
        const UUserDefinedStruct* Struct,
        const FString& LogicalName);
}
```

**处理范围**:
- 基础类型 `FString/FName/FText/int32/int64/float/double/bool`
- `TArray<T>` 递归
- `TMap<K,V>` 递归
- `UStruct` 嵌套递归
- `TSoftObjectPtr<T>` / `TSoftClassPtr<T>`
- `FGameplayTag` / `FGameplayTagContainer`(特殊序列化)
- `UUserDefinedStruct` 字段名 GUID 剥离

**`McpGenericAssetFactory.h/cpp`**
```cpp
namespace McpGenericAssetFactory {
    UObject* CreateAssetOfClass(
        UClass* AssetClass,
        const FString& PackagePath,
        const FString& AssetName,
        TFunction<void(UObject*)> Configurator,
        FString& OutError);
    // 内部:AssetToolsModule.CreateAsset + McpSafeAssetSave
    // 强制 game-thread 执行
}
```

### 3.2 章节标准结构(每章 4 commit)

1. **Commit A(TS)**:consolidated-tool-definitions.ts 加 action + schema;consolidated-tool-handlers.ts 注册;`src/tools/handlers/<domain>-handlers.ts` 新建或扩展;`*.test.ts`(mock)
2. **Commit B(C++)**:`plugins/.../Private/MCP/Tools/McpTool_<Domain>.cpp` 加 handler;`McpAutomationBridgeSubsystem::InitializeHandlers()` 注册
3. **Commit C(Integration)**:`tests/mcp-tools/<domain>/*.mjs` live test
4. **Commit D(Docs,可选)**:更新 README / handler-mapping.md

### 3.3 UE 5.7 硬约束

- 零 `UPackage::SavePackage` → 全部 `McpSafeAssetSave`
- 零 `ANY_PACKAGE` → `nullptr`
- 零 `as any`(TS 运行时)
- 零 `console.log` / `console.error`(runtime,仅 `logger.*`)
- 游戏线程 API 强制走 `AsyncTask(ENamedThreads::GameThread, ...)`

### 3.4 依赖图

```
Ch1 (BP 补丁)          ── 独立
Ch2 (DataTable)        ── 写 McpStructReflection
  ├── Ch3 (DataAsset)     ── 复用 McpStructReflection
  └── Ch5 (Curve)         ── 轻度复用(FRichCurve 非 UStruct,适配层)
Ch4 (GameplayTag)      ── 独立(ini 操作)
Ch6 (UMG add_widget)   ── 复用 McpStructReflection(slotProps)
Ch7 (StateTree 補齐)    ── 独立,风险最高
Ch8 (部署 + smoke)      ── 汇总
```

### 3.5 风险登记

| 风险 | 章节 | 缓解 |
|------|------|------|
| `UUserDefinedStruct` 字段名 GUID 后缀 | Ch2/3 | `McpStructReflection::ResolveUserDefinedStructFieldName` 做映射 |
| `FGameplayTag` 存 ini 而非 asset | Ch4 | `UGameplayTagsManager::AddNewGameplayTagToINI()`,删除走 `GConfig` |
| UE 5.7 StateTree API 与 5.5 有签名变更 | Ch7 | Ch7a 开始前 grep `X:\Unreal_Engine\UE_5.7\Engine\Plugins\Runtime\StateTree` 头文件 |
| CreateAsset 非游戏线程崩 | 全章 | `McpGenericAssetFactory` 强制 `AsyncTask(GameThread)` |
| Editor 修改 ini 后下拉菜单不刷新 | Ch4 | 调用 `EditorRefreshGameplayTagTree()` |
| `propertyPath` 嵌套解析(`Effects.[0].Value`) | Ch3 | 专用 parser,支持 `.` 分段 + `[N]` 索引 |

---

## 4. 章节详表

### Ch1 — `manage_blueprint` 补丁(~0.5 d)

#### Actions
| Action | 输入 | 输出 |
|--------|------|------|
| `set_parent_class` | `blueprintPath: string, parentClass: string` | `{success, oldParent, newParent}` |
| `add_interface` | `blueprintPath, interfacePath` | `{success, currentInterfaces: string[]}` |
| `remove_interface` | `blueprintPath, interfacePath` | `{success, currentInterfaces}` |
| `list_interfaces` | `blueprintPath` | `{interfaces: string[]}` |

#### Event Dispatcher
- Ch1 起始 audit 现有 `manage_blueprint/add_event` C++ 源码,确认是否支持 `delegateSignature` 参数
- 不支持:扩参数,**不加新 action**
- 支持:验证已实现,标注完成

#### 关键 API
- `FBlueprintEditorUtils::ReparentBlueprint()` + `CompileBlueprint`
- `FBlueprintEditorUtils::ImplementNewInterface()` / `RemoveInterface()`
- `UBlueprint::ImplementedInterfaces` TArray 枚举

#### 集成测试(`tests/mcp-tools/core/blueprint-patches.mjs`)
1. 创建 BP(parent = Actor)→ set_parent_class 到 Pawn → compile 成功 → 读 `Blueprint.ParentClass` = Pawn
2. add_interface MyInterface → list_interfaces 含 MyInterface → remove_interface → list 不含
3. Reparent 期间 SCS 组件保留不丢

---

### Ch2 — `manage_data` DataTable 部分(~1 d)

#### Actions(新 tool `manage_data`,DataTable 子集 8)
| Action | 输入 | 输出 |
|--------|------|------|
| `create_data_table` | `path, name, rowStructPath` | `{success, assetPath}` |
| `add_data_table_row` | `path, rowName, fields?: Record<string,unknown>` | `{success, rowName}` |
| `set_data_table_row` | `path, rowName, fields: Record<string,unknown>` | `{success}` |
| `update_data_table_row` | `path, rowName, fields: Partial<...>` | `{success, updatedFields: string[]}` |
| `remove_data_table_row` | `path, rowName` | `{success}` |
| `get_data_table_rows` | `path, rowNames?: string[]` | `{rows: Record<rowName, fields>}` |
| `list_data_table_rows` | `path` | `{rowNames: string[]}` |
| `set_data_table_row_struct` | `path, newRowStructPath` | `{success, rowsMigrated: number}` |

#### 核心脚手架(Ch2 首次写,Ch3/5/6 复用)
`McpStructReflection::SetStructFieldFromJson` 实现清单参见 §3.1。

#### 关键 API
- `UDataTable::AddRow()` / `RemoveRow()` / `GetRowMap()`
- `UDataTable::GetRow<FTableRowBase>(rowName)` 读
- `UDataTable::CleanBeforeStructChange()` + `OnDataTableChanged.Broadcast()`(迁移 row struct 时用)

#### 集成测试(`tests/mcp-tools/authoring/data-table.mjs`)
1. `manage_blueprint/create_struct(ST_TestRow)` → 加 3 字段
2. `create_data_table(DT_Test, ST_TestRow)`
3. `add_data_table_row("Row1", {Name:"A", Value:1.5, Desc:"hello"})`
4. `get_data_table_rows(["Row1"])` → 字段全对齐
5. `update_data_table_row("Row1", {Value: 2.0})` → get 验证仅 Value 变
6. `list_data_table_rows` → `["Row1"]`
7. `remove_data_table_row("Row1")` → list = `[]`

---

### Ch3 — `manage_data` DataAsset 部分(~0.5 d)

#### Actions(扩 `manage_data`,DataAsset 子集 4)
| Action | 输入 | 输出 |
|--------|------|------|
| `create_data_asset` | `path, name, dataAssetClassPath`(BP 或 native 都支持) | `{success, assetPath}` |
| `set_data_asset_property` | `path, propertyPath, value` | `{success}` |
| `get_data_asset_property` | `path, propertyPath` | `{value}` |
| `list_data_assets_of_class` | `classPath, searchPaths?: string[]` | `{assets: string[]}` |

#### `propertyPath` 语法
- `"Name"` — 顶层字段
- `"Stats.Health"` — 嵌套 struct 字段
- `"Effects.[0].Value"` — TArray 元素 + 子字段
- `"Tags.[\"Combat.Attack\"]"` — TMap key lookup

实现:`McpPropertyPathParser::Parse()` → `TArray<FPropertyPathSegment>`,逐段 walk `FProperty::ContainerPtrToValuePtr`。

#### 集成测试
1. 创建 struct → 创建 DataAsset BP(parent=UDataAsset)→ 加 struct 字段
2. `create_data_asset` 实例
3. `set_data_asset_property("Stats.Health", 100)` + `get` 验证
4. `list_data_assets_of_class` 命中创建的实例

---

### Ch4 — `manage_gameplay_tags`(~0.5 d)

#### Actions(新 tool,4)
| Action | 输入 | 输出 |
|--------|------|------|
| `add_gameplay_tag` | `tag, comment?, sourceIni?` | `{success, tag, sourceIni}` |
| `remove_gameplay_tag` | `tag` | `{success}` |
| `list_gameplay_tags` | `prefix?` | `{tags: string[]}` |
| `add_gameplay_tag_source` | `iniRelativePath` | `{success}` |

#### 关键 API
- 写:`UGameplayTagsManager::AddNewGameplayTagToINI(TagName, Comment, RestrictedSourceName)`
- 读:`UGameplayTagsManager::Get().RequestAllGameplayTags(TagContainer, false)`
- 删:`GConfig->RemoveKey("/Script/GameplayTags.GameplayTagsList", ...)` + `UGameplayTagsManager::Get().EditorRefreshGameplayTagTree()`
- 默认 ini:`Config/DefaultGameplayTags.ini`

#### 集成测试
1. `add_gameplay_tag("Modifier.Weather.Rain", "Rain modifier")`
2. 检查 `Config/DefaultGameplayTags.ini` 含 `GameplayTagList=(Tag="Modifier.Weather.Rain",DevComment="Rain modifier")`
3. `list_gameplay_tags("Modifier.Weather")` → 命中
4. `remove_gameplay_tag("Modifier.Weather.Rain")` → list 不含,ini 不含

---

### Ch5 — `manage_curve`(~0.5 d)

#### Actions(新 tool,4)
| Action | 输入 | 输出 |
|--------|------|------|
| `create_curve_float` | `path, name` | `{success, assetPath}` |
| `set_curve_keys` | `path, keys: [{time, value, interpMode?}]` | `{success, keyCount}` |
| `get_curve_keys` | `path` | `{keys: [{time, value, interpMode}]}` |
| `inspect_curve` | `path` | `{keyCount, minTime, maxTime, keys}` |

#### `interpMode` 映射
```
"Auto"        → ERichCurveInterpMode::RCIM_Cubic + TangentMode=Auto
"Linear"      → RCIM_Linear
"Constant"    → RCIM_Constant
"CubicBreak"  → RCIM_Cubic + TangentMode=Break
```

#### 关键 API
- `UCurveFloat::FloatCurve.AddKey(Time, Value)`
- `FRichCurve::SetKeyInterpMode(Handle, Mode)`
- `FRichCurve::GetKeys()` / `GetNumKeys()`

#### 集成测试
1. `create_curve_float(C_Test)`
2. `set_curve_keys([{0, 0, "Linear"}, {1, 1, "Auto"}, {2, 0, "Constant"}])`
3. `get_curve_keys` → 3 keys,interpMode 对齐
4. `inspect_curve` → `{keyCount:3, minTime:0, maxTime:2}`

---

### Ch6 — `manage_widget_authoring/add_widget` 扩展(~0.25 d)

#### Actions(扩现有 tool,2)
| Action | 输入 | 输出 |
|--------|------|------|
| `add_widget` | `widgetBlueprintPath, parentWidgetName, widgetClass, widgetName, slotProps?` | `{success, widgetName}` |
| `remove_widget` | `widgetBlueprintPath, widgetName` | `{success}` |

#### `widgetClass` 解析
- Native:`"UUserWidget"` / `"UTextBlock"` → `FindObject<UClass>(nullptr, ...)`
- BP:`"/Game/UI/WBP_HealthBar.WBP_HealthBar_C"` → `LoadObject<UClass>()`

#### `slotProps` 处理
复用 Ch2 的 `McpStructReflection::SetStructFieldFromJson` 写入 `UPanelSlot` 子类属性。

#### 集成测试
1. 已有 WBP_Parent → `add_widget(WBP_Parent, "RootCanvas", "/Game/UI/WBP_HealthBar.WBP_HealthBar_C", "HealthBarInstance", {Anchor: {Minimum: [0,0], Maximum: [0.3, 0.1]}})`
2. `manage_widget_authoring/get_widget_info(WBP_Parent)` 含 HealthBarInstance
3. `remove_widget(WBP_Parent, "HealthBarInstance")` → 消失

---

### Ch7 — `manage_ai` StateTree 補齐(~1 d)

#### Ch7a / 实现深度审计(2 h)
- 读 `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_AIHandlers.cpp` 的 4 个 StateTree handler 源码
- 读 `X:\Unreal_Engine\UE_5.7\Engine\Plugins\Runtime\StateTree\Source\StateTreeEditorModule\Public\StateTreeEditorData.h` 和相关头,确认 5.7 API 签名
- 产出:audit report 决定 Ch7b 补齐清单(本 spec 预估范围,实际以 audit 为准)

#### Ch7b / 补齐执行(预估)
| 动作 | 类型 |
|------|------|
| 扩 `create_state_tree`:加 `contextClass`(必填) | schema + handler |
| 扩 `add_state_tree_state`:加 `stateType: "State"\|"Subtree"\|"Linked", parentState?` | schema + handler |
| 新 `add_state_tree_task`(如 `configure_state_tree_task` 只改不新增) | 新 action |
| 扩 `configure_state_tree_task`:加 `taskProps: object` | schema + handler |
| 新 `list_state_tree_states(path)` → 树形 | 新 action |
| 新 `remove_state_tree_state(path, stateName)` | 新 action |

#### 关键 API(5.7)
- `UStateTreeEditorData::AddSubTree(FStateTreeStateHandle Parent, FName Name)` 类签名(待 audit 确认)
- `UStateTreeEditorData::Schema` 指向 `UStateTreeSchema` context class
- `FStateTreeEditorNode` 承载 task/evaluator 实例

#### 集成测试
1. `create_state_tree("ST_Test", "/Script/StateTreeModule.StateTreeComponentSchema")`
2. `add_state_tree_state("Root", "State")` → `add_state_tree_state("Combat", "Subtree", parent="Root")`
3. `add_state_tree_task("Combat", "BTTask_MoveTo")` + `configure_state_tree_task("Combat", "BTTask_MoveTo", {Distance: 500})`
4. `add_state_tree_transition("Root", "Combat", "OnDetectEnemy")`
5. `list_state_tree_states` → `{Root: {children: {Combat: {...}}}}`
6. `remove_state_tree_state("Combat")` → list 不含

---

### Ch8 — 部署 + smoke test(~0.5 d)

#### 8.1 构建

```bash
npm run build:core

# C++ plugin(选一):
#   A. UE Editor 里开 War 项目,Editor 自动 rebuild plugin
#   B. 命令行 UBT:
#     "X:\Unreal_Engine\UE_5.7\Engine\Build\BatchFiles\Build.bat" ^
#       WarEditor Win64 Development -Project="D:\Unreal\Project\War\War.uproject"
```

#### 8.2 符号链接(一次性)

```bash
# 备份既有
if [ -e /d/Unreal/Project/War/Plugins/McpAutomationBridge ] && [ ! -L /d/Unreal/Project/War/Plugins/McpAutomationBridge ]; then
  mv /d/Unreal/Project/War/Plugins/McpAutomationBridge /d/Unreal/Project/War/Plugins/McpAutomationBridge.bak
fi

# 管理员 cmd 创建 junction
cmd //c "mklink /J D:\Unreal\Project\War\Plugins\McpAutomationBridge C:\Code\Unreal_mcp\plugins\McpAutomationBridge"
```

提供脚本化版本 `scripts/deploy-to-war.sh`(一次性维护)。

#### 8.3 E2E 综合场景(`tests/mcp-tools/e2e/war-integration.mjs`)

复现 War 项目 Task 8 真实需求 + 覆盖所有章节新 action:
1. `manage_blueprint/create_struct("/Game/War/Data/ModifierKeys/ST_ModifierKeyRow")`
2. `manage_blueprint/modify_struct` 加字段 `Name: FName, Value: double, Description: FText`
3. `manage_data/create_data_table("/Game/War/Data/ModifierKeys/DT_ModifierKeys", ST_ModifierKeyRow)`
4. `manage_data/add_data_table_row` × 3
5. `manage_data/create_data_asset` + `set_data_asset_property` 嵌套
6. `manage_curve/create_curve_float` + `set_curve_keys`
7. `manage_gameplay_tags/add_gameplay_tag("Modifier.Weather.Rain")`
8. `manage_blueprint/create` BP → `set_parent_class` → `add_interface`
9. 所有资产 compile + save pass
10. 每步断言输出对齐 expected schema

#### 8.4 验收 checklist
- [ ] War 项目在 UE 5.7 Editor 启动不崩
- [ ] MCP Client 连上 UE bridge
- [ ] `tests/mcp-tools/e2e/war-integration.mjs` 全绿
- [ ] 全部 vitest unit test 绿
- [ ] 现有 integration test 无回归
- [ ] `git log` 每章一个清晰的分组 commit

---

## 5. 测试策略

### 5.1 四层

| 层 | 工具 | 范围 | 何时跑 |
|---|---|---|---|
| Unit(mock) | vitest,`*.test.ts` 同目录 | TS handler 参数验证、payload 构造、错误路径 | 每 commit,CI |
| Integration(live) | `tests/test-runner.mjs` + UE Editor | 每 action 端到端 | 每章节末(本地) |
| E2E(live) | `war-integration.mjs` | Ch8 综合 10 步 | Ch8 |
| 回归 | 既有 `tests/mcp-tools/**/*.mjs` | 保护未改 tool | Ch8 |

### 5.2 验收矩阵

| Ch | 工期 | 新 action | TS test | C++ live | E2E |
|----|------|----------|---------|----------|-----|
| 1 | 0.5d | 4 | 6 | 3 | reparent + interface |
| 2 | 1d | 8 | 10 | 6 | DT + struct 反射 |
| 3 | 0.5d | 4 | 6 | 4 | nested propertyPath |
| 4 | 0.5d | 4 | 6 | 4 | ini + refresh |
| 5 | 0.5d | 4 | 6 | 4 | curve keys |
| 6 | 0.25d | 2 | 3 | 2 | 自定义 widget class |
| 7 | 1d | 3-5 新 + 3 扩 | 6 | 5 | StateTree 完整树 |
| 8 | 0.5d | 0 | — | 全回归 | War 10 步 |
| **合计** | **~5d** | **~29-31** | **~43** | **~28** | **10+** |

---

## 6. 错误处理

### 6.1 C++ 层分类

```cpp
enum class EMcpErrorCategory {
    InvalidParams,    // 参数格式错 / 缺必填
    NotFound,         // 资产 / 字段 / state 不存在
    ConflictState,    // 名字已占用 / 字段已存在
    EngineAPIError,   // UE 内部失败
    Unsupported,      // 当前 UE 版本不支持
};
```

每个 handler 失败走 `RespondWithError(RequestId, Category, Message)`。

### 6.2 TS 层契约

所有新 action 的 outputSchema 强制含:
```ts
{
    success: boolean,
    error?: string,
    errorCategory?: 'InvalidParams' | 'NotFound' | 'ConflictState' | 'EngineAPIError' | 'Unsupported'
}
```

TS handler 禁止 silent swallow,catch 必须 `logger.error` + 返回结构化错误。

### 6.3 回滚

- 每章 3-4 个独立 commit,失败 `git revert` 该章范围
- **不**跨章节原子性(UE Editor 不支持跨重启事务)

---

## 7. 实施顺序(终版)

```
Day 1:  Ch1 (BP 补丁, 0.5d) + Ch2.A/B (DataTable, 前半 0.5d)
Day 2:  Ch2.C/D (DataTable, 后半) + Ch3 (DataAsset)
Day 3:  Ch4 (GameplayTag) + Ch5 (Curve)
Day 4:  Ch6 (UMG add_widget) + Ch7a (StateTree audit) + Ch7b 开始
Day 5:  Ch7b 完成 + Ch8 (部署 + E2E)
```

---

## 8. 附录

### 8.1 相关文件索引

| 文件 | 作用 |
|------|------|
| `src/tools/consolidated-tool-definitions.ts` | 所有 action enum + JSON Schema |
| `src/tools/consolidated-tool-handlers.ts` | Tool 路由注册 |
| `src/tools/handlers/*.ts` | 域 handler(TS) |
| `plugins/.../Private/MCP/Tools/McpTool_*.cpp` | 域 handler(C++) |
| `plugins/.../Private/MCP/Helpers/McpSafeAssetSave.h` | 5.7 安全保存 |
| `plugins/.../Private/MCP/Helpers/McpStructReflection.h/cpp` | **(新)**通用反射 |
| `plugins/.../Private/MCP/Helpers/McpGenericAssetFactory.h/cpp` | **(新)**通用资产工厂 |
| `tests/test-runner.mjs` | 集成测试 runner |
| `tests/mcp-tools/e2e/war-integration.mjs` | **(新)**Ch8 E2E 场景 |

### 8.2 相关参考

- Engine 5.7 源码:`X:\Unreal_Engine\UE_5.7\Engine`
- 现有 handler 模板:`plugins/.../Private/MCP/Tools/McpTool_ManageBlueprint.cpp`(结构最全)
- 测试示例:`tests/mcp-tools/core/*.mjs`

---

**End of Design Spec.**
