# Ch6 — `manage_widget_authoring/add_widget` Extension (2 actions)

> **Parent plan:** `2026-04-20-mcp-tier123-expansion.md`
> **Spec:** §4 Ch6
> **Depends on:** Task 0 (uses `McpStructReflection` for slot props)
> **Estimated:** 0.25 day, 2 commits

**Goal:** Extend existing `manage_widget_authoring` with generic `add_widget(widgetClass)` / `remove_widget` — escape hatch for adding custom `UUserWidget` subclasses as children.

---

## Task 1: `add_widget` action

**Files:**
- Modify: `src/tools/consolidated-tool-definitions.ts` (manage_widget_authoring action enum + schema)
- Modify: `src/tools/handlers/widget-authoring-handlers.ts`
- Modify: `plugins/.../Private/McpAutomationBridge_WidgetAuthoringHandlers.cpp`
- Modify: `plugins/.../Private/McpAutomationBridgeSubsystem.cpp`
- Modify: `tests/integration.mjs`

- [ ] **Step 1: Extend action enum + schema**

In `consolidated-tool-definitions.ts`, locate `manage_widget_authoring` action enum; append:
```typescript
'add_widget', 'remove_widget'
```

In input schema, add (near existing widget-related props):
```typescript
widgetBlueprintPath: { type: 'string', description: 'Widget blueprint containing the parent widget.' },
parentWidgetName: { type: 'string', description: 'Name of the parent panel widget in the WBP.' },
widgetClass: { type: 'string', description: 'Class path for child widget (native like "UUserWidget" or /Game/UI/WBP_Foo.WBP_Foo_C).' },
widgetName: { type: 'string', description: 'Name for the new child widget instance.' },
slotProps: { type: 'object', additionalProperties: true, description: 'Optional UPanelSlot property overrides.' }
```

- [ ] **Step 2: Unit test**

Extend `src/tools/handlers/widget-authoring-handlers.test.ts` (create if absent):

```typescript
import { describe, it, expect, vi } from 'vitest';
import { handleWidgetAuthoringTools } from './widget-authoring-handlers.js';

describe('widget-authoring: add_widget', () => {
  it('forwards blueprint + parent + class + name + slot', async () => {
    const mockTools = { executeAutomation: vi.fn().mockResolvedValue({ success: true, widgetName: 'HealthBarInstance' }) };
    const res = await handleWidgetAuthoringTools(
      'add_widget',
      {
        widgetBlueprintPath: '/Game/UI/WBP_Parent',
        parentWidgetName: 'RootCanvas',
        widgetClass: '/Game/UI/WBP_HealthBar.WBP_HealthBar_C',
        widgetName: 'HealthBarInstance',
        slotProps: { Anchor: { Minimum: [0, 0] } }
      } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(res.success).toBe(true);
    expect(res.widgetName).toBe('HealthBarInstance');
  });
});
```
Run: FAIL.

- [ ] **Step 3: TS handler case**

Add to `widget-authoring-handlers.ts` switch:

```typescript
case 'add_widget': {
  const widgetBlueprintPath = argsRecord.widgetBlueprintPath as string | undefined;
  const parentWidgetName = argsRecord.parentWidgetName as string | undefined;
  const widgetClass = argsRecord.widgetClass as string | undefined;
  const widgetName = argsRecord.widgetName as string | undefined;
  const slotProps = argsRecord.slotProps as Record<string, unknown> | undefined;
  if (!widgetBlueprintPath) throw new Error('Missing required parameter: widgetBlueprintPath');
  if (!parentWidgetName) throw new Error('Missing required parameter: parentWidgetName');
  if (!widgetClass) throw new Error('Missing required parameter: widgetClass');
  if (!widgetName) throw new Error('Missing required parameter: widgetName');
  const res = await executeAutomationRequest(tools, 'widget_add_widget', {
    widgetBlueprintPath, parentWidgetName, widgetClass, widgetName, slotProps: slotProps ?? {}
  });
  return cleanObject(res as Record<string, unknown>) as Record<string, unknown>;
}
```
Run: PASS.

- [ ] **Step 4: C++ handler**

```cpp
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "WidgetBlueprint.h"
#include "Components/PanelWidget.h"
#include "Components/PanelSlot.h"
#include "Blueprint/UserWidget.h"
#include "MCP/Helpers/McpStructReflection.h"

TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleWidgetAddWidget(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    FString WBPPath, ParentName, WidgetClassPath, NewName;
    if (!Params->TryGetStringField(TEXT("widgetBlueprintPath"), WBPPath) ||
        !Params->TryGetStringField(TEXT("parentWidgetName"), ParentName) ||
        !Params->TryGetStringField(TEXT("widgetClass"), WidgetClassPath) ||
        !Params->TryGetStringField(TEXT("widgetName"), NewName))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Missing required param"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }

    UWidgetBlueprint* WBP = LoadObject<UWidgetBlueprint>(nullptr, *WBPPath);
    if (!WBP)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("WBP not found: %s"), *WBPPath));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }

    UClass* ChildClass = LoadObject<UClass>(nullptr, *WidgetClassPath);
    if (!ChildClass)
    {
        FString Normalized = WidgetClassPath;
        if (!Normalized.Contains(TEXT(".")))
        {
            int32 SlashIdx = INDEX_NONE;
            if (Normalized.FindLastChar(TEXT('/'), SlashIdx))
            {
                Normalized = Normalized + TEXT(".") + Normalized.Mid(SlashIdx + 1) + TEXT("_C");
            }
        }
        ChildClass = LoadObject<UClass>(nullptr, *Normalized);
    }
    if (!ChildClass || !ChildClass->IsChildOf(UWidget::StaticClass()))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Not a UWidget subclass: %s"), *WidgetClassPath));
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }

    UWidgetTree* Tree = WBP->WidgetTree;
    UPanelWidget* ParentPanel = nullptr;
    Tree->ForEachWidget([&](UWidget* W) {
        if (W && W->GetName() == ParentName)
        {
            ParentPanel = Cast<UPanelWidget>(W);
        }
    });
    if (!ParentPanel)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Parent panel not found: %s"), *ParentName));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }

    UWidget* NewWidget = Tree->ConstructWidget<UWidget>(ChildClass, FName(*NewName));
    UPanelSlot* NewSlot = ParentPanel->AddChild(NewWidget);
    if (!NewSlot)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("AddChild failed"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("EngineAPIError"));
        return Response;
    }

    // Apply slotProps via reflection
    const TSharedPtr<FJsonObject>* SlotPropsObj = nullptr;
    if (Params->TryGetObjectField(TEXT("slotProps"), SlotPropsObj) && SlotPropsObj && (*SlotPropsObj).IsValid())
    {
        for (const auto& Pair : (*SlotPropsObj)->Values)
        {
            const FName FieldName = McpStructReflection::ResolveFieldName(NewSlot->GetClass(), Pair.Key);
            FString SetError;
            if (FieldName != NAME_None)
            {
                McpStructReflection::SetStructFieldFromJson(NewSlot->GetClass(), NewSlot, FieldName, Pair.Value, SetError);
            }
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(WBP);
    FKismetEditorUtilities::CompileBlueprint(WBP);
    McpSafeAssetSave(WBP);

    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("widgetName"), NewName);
    return Response;
}
```

Register: `widget_add_widget`.

- [ ] **Step 5: Integration**

```javascript
{ scenario: 'UMG prep: WBP parent', toolName: 'manage_widget_authoring',
  arguments: { action: 'create_widget_blueprint', name: 'WBP_Ch6Parent', path: '/Game/DataTest' },
  expected: 'success|already exists' },
{ scenario: 'UMG prep: add canvas panel root', toolName: 'manage_widget_authoring',
  arguments: { action: 'add_canvas_panel', widgetBlueprintPath: '/Game/DataTest/WBP_Ch6Parent', parentWidgetName: 'RootPanel', widgetName: 'RootCanvas' },
  expected: 'success|already exists' },
{ scenario: 'UMG prep: custom user widget class', toolName: 'manage_widget_authoring',
  arguments: { action: 'create_widget_blueprint', name: 'WBP_Ch6Child', path: '/Game/DataTest' },
  expected: 'success|already exists' },
{ scenario: 'UMG: add_widget custom class', toolName: 'manage_widget_authoring',
  arguments: { action: 'add_widget',
    widgetBlueprintPath: '/Game/DataTest/WBP_Ch6Parent',
    parentWidgetName: 'RootCanvas',
    widgetClass: '/Game/DataTest/WBP_Ch6Child.WBP_Ch6Child_C',
    widgetName: 'ChildInstance' },
  expected: 'success' },
```

(Verify `add_canvas_panel` schema in the existing enum — the arg name may be different. Check `consolidated-tool-definitions.ts` before committing.)

- [ ] **Step 6: Compile + run**. Expected PASS.

- [ ] **Step 7: Commit**
```bash
git add -u
git commit -m "feat(manage_widget_authoring): add generic add_widget action

Escape hatch for adding any UUserWidget subclass (native or BP) as
a child of a panel widget. Supports optional slotProps via reflection."
```

---

## Task 2: `remove_widget` action

- [ ] **Step 1: Unit test**

```typescript
describe('widget-authoring: remove_widget', () => {
  it('forwards path + widgetName', async () => {
    const mockTools = { executeAutomation: vi.fn().mockResolvedValue({ success: true }) };
    await handleWidgetAuthoringTools(
      'remove_widget',
      { widgetBlueprintPath: '/Game/UI/WBP_P', widgetName: 'ChildInstance' } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(mockTools.executeAutomation).toHaveBeenCalledWith('widget_remove_widget', expect.objectContaining({ widgetName: 'ChildInstance' }));
  });
});
```
Run: FAIL.

- [ ] **Step 2: TS handler**

```typescript
case 'remove_widget': {
  const widgetBlueprintPath = argsRecord.widgetBlueprintPath as string | undefined;
  const widgetName = argsRecord.widgetName as string | undefined;
  if (!widgetBlueprintPath) throw new Error('Missing required parameter: widgetBlueprintPath');
  if (!widgetName) throw new Error('Missing required parameter: widgetName');
  const res = await executeAutomationRequest(tools, 'widget_remove_widget', { widgetBlueprintPath, widgetName });
  return cleanObject(res as Record<string, unknown>) as Record<string, unknown>;
}
```
Run: PASS.

- [ ] **Step 3: C++ handler**

```cpp
TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleWidgetRemoveWidget(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    FString WBPPath, WName;
    if (!Params->TryGetStringField(TEXT("widgetBlueprintPath"), WBPPath) ||
        !Params->TryGetStringField(TEXT("widgetName"), WName))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Missing required param"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }
    UWidgetBlueprint* WBP = LoadObject<UWidgetBlueprint>(nullptr, *WBPPath);
    if (!WBP)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("WBP not found: %s"), *WBPPath));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }

    UWidget* Target = nullptr;
    WBP->WidgetTree->ForEachWidget([&](UWidget* W) {
        if (W && W->GetName() == WName) Target = W;
    });
    if (!Target)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Widget not found: %s"), *WName));
        Response->SetStringField(TEXT("errorCategory"), TEXT("NotFound"));
        return Response;
    }
    WBP->WidgetTree->RemoveWidget(Target);

    FBlueprintEditorUtils::MarkBlueprintAsModified(WBP);
    FKismetEditorUtilities::CompileBlueprint(WBP);
    McpSafeAssetSave(WBP);

    Response->SetBoolField(TEXT("success"), true);
    return Response;
}
```

Register: `widget_remove_widget`.

- [ ] **Step 4: Integration**
```javascript
{ scenario: 'UMG: remove_widget', toolName: 'manage_widget_authoring',
  arguments: { action: 'remove_widget', widgetBlueprintPath: '/Game/DataTest/WBP_Ch6Parent', widgetName: 'ChildInstance' },
  expected: 'success' },
```

- [ ] **Step 5: Compile + run**. Expected PASS.

- [ ] **Step 6: Commit**
```bash
git add -u
git commit -m "feat(manage_widget_authoring): add remove_widget action"
```

---

## Acceptance Checklist (Ch6)

- [ ] 2 commits
- [ ] Unit tests pass
- [ ] 4+ integration scenarios pass (prep + add + remove)
- [ ] `WBP_Ch6Parent` round-trips add→remove with BP recompile succeeding

---

## Notes for Subagent

- **Panel widget identification** — the `parentWidgetName` argument refers to the widget's in-tree name (the variable name shown in Designer), not its class or a path.
- **Slot class varies by parent** — `UCanvasPanel` creates `UCanvasPanelSlot`; `UVerticalBox` creates `UVerticalBoxSlot`; each has different fields. `McpStructReflection::ResolveFieldName` tolerates unknown field names by returning NAME_None; we silently skip those to avoid failing the whole call over a typo. If you want stricter behavior, collect and return unknown-field warnings.
- **CompileBlueprint after structure change** — UE requires the WBP to recompile to resolve the new child in Designer; without it, the child will appear in the tree but not render in PIE.
