# MCP Tier 1+2+3 Expansion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add ~30 new MCP actions covering UDataTable / UDataAsset / GameplayTag / Curve / UMG generic add / Blueprint reparent+interface / StateTree completion — all targeting UE 5.7 in single chapter-by-chapter ships.

**Architecture:** Vertical-slice chapters (Ch1-8), each chapter self-contained (TS schema + TS handler + C++ handler + tests + commit). Two shared C++ helpers (`McpStructReflection`, `McpGenericAssetFactory`) written once in Task 0 and reused across chapters.

**Tech Stack:** TypeScript (NodeNext ESM, strict mode), Unreal Engine 5.7 C++ (UE API), Vitest (unit), custom `tests/test-runner.mjs` (live integration).

**Spec reference:** `docs/superpowers/specs/2026-04-20-mcp-tier123-expansion-design.md`

---

## Plan Layout

This plan is split across **9 files** for subagent-friendly execution. Each chapter file is self-contained and corresponds to one subagent dispatch:

| File | Purpose |
|------|---------|
| `2026-04-20-mcp-tier123-expansion.md` (this file) | Master index, file structure, Task 0 (shared scaffolding), execution order |
| `2026-04-20-mcp-ch1-blueprint-patches.md` | Ch1: BP reparent / interface / event dispatcher |
| `2026-04-20-mcp-ch2-data-table.md` | Ch2: `manage_data` DataTable (8 actions) |
| `2026-04-20-mcp-ch3-data-asset.md` | Ch3: `manage_data` DataAsset (4 actions) |
| `2026-04-20-mcp-ch4-gameplay-tags.md` | Ch4: `manage_gameplay_tags` (4 actions) |
| `2026-04-20-mcp-ch5-curve.md` | Ch5: `manage_curve` (4 actions) |
| `2026-04-20-mcp-ch6-umg-add-widget.md` | Ch6: `manage_widget_authoring/add_widget` (2 actions) |
| `2026-04-20-mcp-ch7-state-tree.md` | Ch7: `manage_ai` StateTree audit + completion |
| `2026-04-20-mcp-ch8-deploy-e2e.md` | Ch8: War project deployment + E2E smoke test |

---

## File Structure Map

### TypeScript layer (existing files touched)

| File | Action |
|------|--------|
| `src/tools/consolidated-tool-definitions.ts` | Add action enums + JSON Schema for 3 new tools, extend 3 existing tools |
| `src/tools/consolidated-tool-handlers.ts` | Wire in 3 new handler imports + switch cases |

### TypeScript layer (new files)

| File | Responsibility |
|------|---------------|
| `src/tools/handlers/data-handlers.ts` | Routes `manage_data` actions (DataTable + DataAsset) |
| `src/tools/handlers/data-handlers.test.ts` | Vitest for `data-handlers.ts` |
| `src/tools/handlers/gameplay-tags-handlers.ts` | Routes `manage_gameplay_tags` actions |
| `src/tools/handlers/gameplay-tags-handlers.test.ts` | Vitest |
| `src/tools/handlers/curve-handlers.ts` | Routes `manage_curve` actions |
| `src/tools/handlers/curve-handlers.test.ts` | Vitest |

### TypeScript layer (files extended)

| File | What changes |
|------|--------------|
| `src/tools/handlers/blueprint-handlers.ts` | +4 action cases: `set_parent_class`, `add_interface`, `remove_interface`, `list_interfaces`; audit-then-patch `add_event` |
| `src/tools/handlers/ai-handlers.ts` | Extend StateTree 4 actions schema handling; add 3-5 new StateTree actions |
| `src/tools/handlers/widget-authoring-handlers.ts` | +2 actions: `add_widget`, `remove_widget` |

### C++ layer (new helpers)

| File | Responsibility |
|------|---------------|
| `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Helpers/McpStructReflection.h` | Reflection-based struct-field get/set (UStruct + UUserDefinedStruct) |
| `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Helpers/McpStructReflection.cpp` | Impl |
| `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Helpers/McpGenericAssetFactory.h` | Game-thread-safe asset factory wrapper over `FAssetToolsModule::CreateAsset` + `McpSafeAssetSave` |
| `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Helpers/McpGenericAssetFactory.cpp` | Impl |
| `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Helpers/McpPropertyPath.h` | `"Effects.[0].Value"` path parser + walker |
| `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Helpers/McpPropertyPath.cpp` | Impl |

### C++ layer (new tool dispatchers)

| File | Tool |
|------|------|
| `plugins/.../Private/MCP/Tools/McpTool_ManageData.cpp` | `manage_data` routing (DataTable + DataAsset) |
| `plugins/.../Private/MCP/Tools/McpTool_ManageGameplayTags.cpp` | `manage_gameplay_tags` |
| `plugins/.../Private/MCP/Tools/McpTool_ManageCurve.cpp` | `manage_curve` |

### C++ layer (new handlers)

| File | Handlers |
|------|---------|
| `plugins/.../Private/McpAutomationBridge_DataHandlers.cpp` | DataTable + DataAsset handler bodies |
| `plugins/.../Private/McpAutomationBridge_GameplayTagsHandlers.cpp` | GameplayTag handler bodies |
| `plugins/.../Private/McpAutomationBridge_CurveHandlers.cpp` | Curve handler bodies |

### C++ layer (files extended)

| File | Extension |
|------|-----------|
| `plugins/.../Private/McpAutomationBridge_BlueprintHandlers.cpp` | +4 handlers: reparent, interfaces |
| `plugins/.../Private/McpAutomationBridge_AIHandlers.cpp` | StateTree audit + extend/add handlers |
| `plugins/.../Private/McpAutomationBridge_WidgetAuthoringHandlers.cpp` | +2 handlers: add_widget, remove_widget |
| `plugins/.../McpAutomationBridge.Build.cs` | Add `GameplayTagsEditor`, `StateTreeEditorModule` to `PrivateDependencyModuleNames` if not already |
| `plugins/.../Private/McpAutomationBridgeSubsystem.cpp` (InitializeHandlers) | Register all new handlers in dispatcher map |

### Integration tests (new files)

| File | Purpose |
|------|---------|
| `tests/scenarios/blueprint-patches.mjs` | Ch1 integration cases |
| `tests/scenarios/data-table.mjs` | Ch2 integration |
| `tests/scenarios/data-asset.mjs` | Ch3 integration |
| `tests/scenarios/gameplay-tags.mjs` | Ch4 integration |
| `tests/scenarios/curve.mjs` | Ch5 integration |
| `tests/scenarios/umg-add-widget.mjs` | Ch6 integration |
| `tests/scenarios/state-tree.mjs` | Ch7 integration |
| `tests/scenarios/war-e2e.mjs` | Ch8 E2E |
| `scripts/deploy-to-war.sh` | Ch8 one-shot deploy-to-War junction setup |

---

## Task 0: Shared C++ Scaffolding

Write once, use across Ch2/3/5/6. Must merge before starting Ch2.

**Files:**
- Create: `plugins/.../Private/MCP/Helpers/McpStructReflection.h`
- Create: `plugins/.../Private/MCP/Helpers/McpStructReflection.cpp`
- Create: `plugins/.../Private/MCP/Helpers/McpGenericAssetFactory.h`
- Create: `plugins/.../Private/MCP/Helpers/McpGenericAssetFactory.cpp`
- Create: `plugins/.../Private/MCP/Helpers/McpPropertyPath.h`
- Create: `plugins/.../Private/MCP/Helpers/McpPropertyPath.cpp`

- [ ] **Step 1: Write `McpStructReflection.h`**

```cpp
// McpStructReflection.h
#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"

namespace McpStructReflection
{
    /** Resolve a logical field name to the internal name (UUserDefinedStruct has GUID suffixes). */
    FName ResolveFieldName(const UStruct* Struct, const FString& LogicalName);

    /** Write a JSON value into a struct field by FName lookup. Returns false + OutError on failure. */
    bool SetStructFieldFromJson(
        const UStruct* Struct,
        void* StructInstance,
        FName FieldName,
        const TSharedPtr<FJsonValue>& Value,
        FString& OutError);

    /** Write all fields from a JSON object into a struct instance. Returns false + OutError on first failure. */
    bool SetStructFieldsFromJsonObject(
        const UStruct* Struct,
        void* StructInstance,
        const TSharedPtr<FJsonObject>& Fields,
        FString& OutError);

    /** Read a struct instance back to a JSON object (all fields). */
    TSharedPtr<FJsonObject> StructInstanceToJson(const UStruct* Struct, const void* StructInstance);

    /** Read a single field by name. Returns null on missing field. */
    TSharedPtr<FJsonValue> GetStructFieldAsJson(
        const UStruct* Struct,
        const void* StructInstance,
        FName FieldName);
}
```

- [ ] **Step 2: Write `McpStructReflection.cpp`**

```cpp
// McpStructReflection.cpp
#include "McpStructReflection.h"
#include "UObject/UnrealType.h"
#include "UObject/PropertyPortFlags.h"
#include "Engine/UserDefinedStruct.h"
#include "JsonObjectConverter.h"

namespace McpStructReflection
{
    FName ResolveFieldName(const UStruct* Struct, const FString& LogicalName)
    {
        if (!Struct) return NAME_None;

        // UUserDefinedStruct stores field names as "LogicalName_IDX_GUID"
        const UUserDefinedStruct* UDS = Cast<UUserDefinedStruct>(Struct);
        if (UDS)
        {
            for (TFieldIterator<FProperty> It(UDS); It; ++It)
            {
                const FProperty* Prop = *It;
                const FString PropName = Prop->GetName();
                // Match by prefix up to the "_N_" pattern
                int32 UnderscoreIdx = INDEX_NONE;
                if (PropName.FindChar(TEXT('_'), UnderscoreIdx))
                {
                    const FString Logical = PropName.Left(UnderscoreIdx);
                    if (Logical.Equals(LogicalName, ESearchCase::IgnoreCase))
                    {
                        return Prop->GetFName();
                    }
                }
                // Fallback: exact match (for non-UDS or early-mangled)
                if (PropName.Equals(LogicalName, ESearchCase::IgnoreCase))
                {
                    return Prop->GetFName();
                }
            }
            return NAME_None;
        }

        // Native UStruct: direct FName lookup
        FProperty* Prop = Struct->FindPropertyByName(FName(*LogicalName));
        return Prop ? Prop->GetFName() : NAME_None;
    }

    bool SetStructFieldFromJson(
        const UStruct* Struct,
        void* StructInstance,
        FName FieldName,
        const TSharedPtr<FJsonValue>& Value,
        FString& OutError)
    {
        if (!Struct || !StructInstance) { OutError = TEXT("Null struct"); return false; }
        FProperty* Prop = Struct->FindPropertyByName(FieldName);
        if (!Prop)
        {
            OutError = FString::Printf(TEXT("Field not found: %s"), *FieldName.ToString());
            return false;
        }
        void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(StructInstance);
        if (!FJsonObjectConverter::JsonValueToUProperty(Value, Prop, ValuePtr, 0, CPF_Transient))
        {
            OutError = FString::Printf(TEXT("Failed to convert JSON to field %s"), *FieldName.ToString());
            return false;
        }
        return true;
    }

    bool SetStructFieldsFromJsonObject(
        const UStruct* Struct,
        void* StructInstance,
        const TSharedPtr<FJsonObject>& Fields,
        FString& OutError)
    {
        if (!Fields.IsValid()) { OutError = TEXT("Null fields object"); return false; }
        for (const auto& Pair : Fields->Values)
        {
            const FName ResolvedName = ResolveFieldName(Struct, Pair.Key);
            if (ResolvedName == NAME_None)
            {
                OutError = FString::Printf(TEXT("Unknown field: %s"), *Pair.Key);
                return false;
            }
            if (!SetStructFieldFromJson(Struct, StructInstance, ResolvedName, Pair.Value, OutError))
            {
                return false;
            }
        }
        return true;
    }

    TSharedPtr<FJsonObject> StructInstanceToJson(const UStruct* Struct, const void* StructInstance)
    {
        TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
        if (!Struct || !StructInstance) return Out;
        for (TFieldIterator<FProperty> It(Struct); It; ++It)
        {
            const FProperty* Prop = *It;
            const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(StructInstance);
            TSharedPtr<FJsonValue> JsonVal = FJsonObjectConverter::UPropertyToJsonValue(Prop, ValuePtr, 0, CPF_Transient);

            // Strip UUserDefinedStruct GUID suffix for logical names
            FString Key = Prop->GetName();
            int32 UnderscoreIdx = INDEX_NONE;
            if (Struct->IsA<UUserDefinedStruct>() && Key.FindChar(TEXT('_'), UnderscoreIdx))
            {
                Key = Key.Left(UnderscoreIdx);
            }
            Out->SetField(Key, JsonVal);
        }
        return Out;
    }

    TSharedPtr<FJsonValue> GetStructFieldAsJson(
        const UStruct* Struct,
        const void* StructInstance,
        FName FieldName)
    {
        if (!Struct || !StructInstance) return nullptr;
        FProperty* Prop = Struct->FindPropertyByName(FieldName);
        if (!Prop) return nullptr;
        const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(StructInstance);
        return FJsonObjectConverter::UPropertyToJsonValue(Prop, ValuePtr, 0, CPF_Transient);
    }
}
```

- [ ] **Step 3: Write `McpGenericAssetFactory.h`**

```cpp
// McpGenericAssetFactory.h
#pragma once
#include "CoreMinimal.h"

namespace McpGenericAssetFactory
{
    /**
     * Create a UObject asset of the given class at PackagePath/AssetName.
     * MUST be called from Game Thread (wrap at callsite with AsyncTask if needed).
     * After CreateAsset, Configurator is called with the new object so caller can set defaults.
     * Saves via McpSafeAssetSave. Returns nullptr + OutError on failure.
     */
    UObject* CreateAssetOfClass(
        UClass* AssetClass,
        const FString& PackagePath,
        const FString& AssetName,
        TFunction<void(UObject*)> Configurator,
        FString& OutError);
}
```

- [ ] **Step 4: Write `McpGenericAssetFactory.cpp`**

```cpp
// McpGenericAssetFactory.cpp
#include "McpGenericAssetFactory.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "HAL/IConsoleManager.h"
#include "McpAutomationBridgeHelpers.h" // for McpSafeAssetSave

namespace McpGenericAssetFactory
{
    UObject* CreateAssetOfClass(
        UClass* AssetClass,
        const FString& PackagePath,
        const FString& AssetName,
        TFunction<void(UObject*)> Configurator,
        FString& OutError)
    {
        check(IsInGameThread());
        if (!AssetClass) { OutError = TEXT("Null AssetClass"); return nullptr; }
        if (AssetName.IsEmpty()) { OutError = TEXT("Empty AssetName"); return nullptr; }
        if (PackagePath.IsEmpty()) { OutError = TEXT("Empty PackagePath"); return nullptr; }

        FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
        UObject* NewAsset = AssetToolsModule.Get().CreateAsset(AssetName, PackagePath, AssetClass, nullptr);
        if (!NewAsset)
        {
            OutError = FString::Printf(TEXT("CreateAsset failed for %s/%s"), *PackagePath, *AssetName);
            return nullptr;
        }

        if (Configurator)
        {
            Configurator(NewAsset);
        }

        NewAsset->MarkPackageDirty();
        if (!McpSafeAssetSave(NewAsset))
        {
            OutError = FString::Printf(TEXT("McpSafeAssetSave failed for %s/%s"), *PackagePath, *AssetName);
            // Do not delete; asset is valid in memory even if save fails
        }
        return NewAsset;
    }
}
```

- [ ] **Step 5: Write `McpPropertyPath.h`**

```cpp
// McpPropertyPath.h
#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonValue.h"

namespace McpPropertyPath
{
    /** Walk a dotted/indexed path like "Stats.Health" or "Effects.[0].Value" and SET the target JSON value. */
    bool SetValueAtPath(
        UObject* RootObject,
        const FString& PropertyPath,
        const TSharedPtr<FJsonValue>& Value,
        FString& OutError);

    /** Walk a path and READ the target as a JSON value. Returns null on miss. */
    TSharedPtr<FJsonValue> GetValueAtPath(
        UObject* RootObject,
        const FString& PropertyPath,
        FString& OutError);
}
```

- [ ] **Step 6: Write `McpPropertyPath.cpp`**

```cpp
// McpPropertyPath.cpp
#include "McpPropertyPath.h"
#include "UObject/UnrealType.h"
#include "JsonObjectConverter.h"

namespace McpPropertyPath
{
    struct FSegment
    {
        FString Name;      // field name, empty if this is a pure array index
        int32 ArrayIndex;  // -1 if not an array index
    };

    static bool ParsePath(const FString& Path, TArray<FSegment>& OutSegments, FString& OutError)
    {
        TArray<FString> Parts;
        Path.ParseIntoArray(Parts, TEXT("."), true);
        for (const FString& P : Parts)
        {
            FSegment Seg;
            Seg.ArrayIndex = -1;
            if (P.StartsWith(TEXT("[")) && P.EndsWith(TEXT("]")))
            {
                const FString IndexStr = P.Mid(1, P.Len() - 2);
                if (!IndexStr.IsNumeric())
                {
                    OutError = FString::Printf(TEXT("Non-numeric array index: %s"), *P);
                    return false;
                }
                Seg.ArrayIndex = FCString::Atoi(*IndexStr);
            }
            else
            {
                Seg.Name = P;
            }
            OutSegments.Add(Seg);
        }
        return true;
    }

    static bool WalkToContainer(
        UObject* Root,
        const TArray<FSegment>& Segments,
        FProperty*& OutFinalProp,
        void*& OutFinalContainer,
        FString& OutError)
    {
        if (!Root) { OutError = TEXT("Null root"); return false; }
        void* CurrentContainer = Root;
        UStruct* CurrentStruct = Root->GetClass();
        FProperty* CurrentProp = nullptr;

        for (int32 i = 0; i < Segments.Num(); ++i)
        {
            const FSegment& Seg = Segments[i];
            const bool bIsLast = (i == Segments.Num() - 1);

            if (Seg.ArrayIndex >= 0)
            {
                // Previous prop must be FArrayProperty
                FArrayProperty* ArrayProp = CastField<FArrayProperty>(CurrentProp);
                if (!ArrayProp) { OutError = TEXT("Indexed segment on non-array"); return false; }
                FScriptArrayHelper Helper(ArrayProp, CurrentContainer);
                if (!Helper.IsValidIndex(Seg.ArrayIndex)) { OutError = FString::Printf(TEXT("Array index OOB: %d"), Seg.ArrayIndex); return false; }
                CurrentContainer = Helper.GetRawPtr(Seg.ArrayIndex);
                CurrentProp = ArrayProp->Inner;
                if (FStructProperty* InnerStruct = CastField<FStructProperty>(CurrentProp))
                {
                    CurrentStruct = InnerStruct->Struct;
                }
                if (bIsLast)
                {
                    OutFinalProp = CurrentProp;
                    OutFinalContainer = CurrentContainer;
                    return true;
                }
                continue;
            }

            // Name segment
            FProperty* Prop = CurrentStruct->FindPropertyByName(FName(*Seg.Name));
            if (!Prop) { OutError = FString::Printf(TEXT("Field not found: %s"), *Seg.Name); return false; }

            if (bIsLast)
            {
                OutFinalProp = Prop;
                OutFinalContainer = CurrentContainer;
                return true;
            }

            // Descend
            if (FStructProperty* SP = CastField<FStructProperty>(Prop))
            {
                CurrentContainer = SP->ContainerPtrToValuePtr<void>(CurrentContainer);
                CurrentStruct = SP->Struct;
                CurrentProp = Prop;
            }
            else if (FArrayProperty* AP = CastField<FArrayProperty>(Prop))
            {
                // Next segment must be [N]
                CurrentContainer = AP->ContainerPtrToValuePtr<void>(CurrentContainer);
                CurrentProp = Prop;
            }
            else
            {
                OutError = FString::Printf(TEXT("Cannot descend into non-struct/non-array field: %s"), *Seg.Name);
                return false;
            }
        }
        OutError = TEXT("Empty path");
        return false;
    }

    bool SetValueAtPath(UObject* Root, const FString& Path, const TSharedPtr<FJsonValue>& Value, FString& OutError)
    {
        TArray<FSegment> Segments;
        if (!ParsePath(Path, Segments, OutError)) return false;
        FProperty* FinalProp = nullptr;
        void* FinalContainer = nullptr;
        if (!WalkToContainer(Root, Segments, FinalProp, FinalContainer, OutError)) return false;
        void* ValuePtr = FinalProp->ContainerPtrToValuePtr<void>(FinalContainer);
        // For array element write, FinalContainer IS the raw element ptr (see WalkToContainer); skip Container offset
        if (Segments.Last().ArrayIndex >= 0) ValuePtr = FinalContainer;

        if (!FJsonObjectConverter::JsonValueToUProperty(Value, FinalProp, ValuePtr, 0, CPF_Transient))
        {
            OutError = TEXT("JsonValueToUProperty failed");
            return false;
        }
        Root->MarkPackageDirty();
        return true;
    }

    TSharedPtr<FJsonValue> GetValueAtPath(UObject* Root, const FString& Path, FString& OutError)
    {
        TArray<FSegment> Segments;
        if (!ParsePath(Path, Segments, OutError)) return nullptr;
        FProperty* FinalProp = nullptr;
        void* FinalContainer = nullptr;
        if (!WalkToContainer(Root, Segments, FinalProp, FinalContainer, OutError)) return nullptr;
        void* ValuePtr = FinalProp->ContainerPtrToValuePtr<void>(FinalContainer);
        if (Segments.Last().ArrayIndex >= 0) ValuePtr = FinalContainer;
        return FJsonObjectConverter::UPropertyToJsonValue(FinalProp, ValuePtr, 0, CPF_Transient);
    }
}
```

- [ ] **Step 7: Register all 3 helper .cpp files in Build.cs (if using explicit list)**

`plugins/McpAutomationBridge/Source/McpAutomationBridge/McpAutomationBridge.Build.cs` — verify `JsonUtilities` and `Json` modules are in `PrivateDependencyModuleNames`; if not:

```csharp
PrivateDependencyModuleNames.AddRange(new string[] {
    // ... existing ...
    "JsonUtilities", "Json"
});
```

(Unreal's default Build.cs scans folder recursively; no explicit file list needed.)

- [ ] **Step 8: Compile plugin (manual verification)**

Open UE 5.7 Editor with the project OR run:
```
"X:\Unreal_Engine\UE_5.7\Engine\Build\BatchFiles\Build.bat" McpAutomationBridgeEditor Win64 Development -Plugin="C:\Code\Unreal_mcp\plugins\McpAutomationBridge\McpAutomationBridge.uplugin"
```
Expected: build succeeds with 0 errors. No handlers wired yet; nothing to test.

- [ ] **Step 9: Commit Task 0**

```bash
git add plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Helpers/
git commit -m "feat(mcp-helpers): add StructReflection / GenericAssetFactory / PropertyPath scaffolding

Shared C++ helpers for upcoming DataTable / DataAsset / Curve / UMG actions.
No handler wired yet; pure scaffolding commit."
```

---

## Execution Order

1. **Task 0** (this file) — shared scaffolding, merge first
2. **Ch1** (`2026-04-20-mcp-ch1-blueprint-patches.md`) — warmup, no new scaffolding
3. **Ch2** (`2026-04-20-mcp-ch2-data-table.md`) — first consumer of `McpStructReflection`
4. **Ch3** (`2026-04-20-mcp-ch3-data-asset.md`) — first consumer of `McpPropertyPath`
5. **Ch4** (`2026-04-20-mcp-ch4-gameplay-tags.md`) — independent (ini ops)
6. **Ch5** (`2026-04-20-mcp-ch5-curve.md`) — independent
7. **Ch6** (`2026-04-20-mcp-ch6-umg-add-widget.md`) — reuses `McpStructReflection`
8. **Ch7** (`2026-04-20-mcp-ch7-state-tree.md`) — audit first, then extend
9. **Ch8** (`2026-04-20-mcp-ch8-deploy-e2e.md`) — deployment + E2E smoke

Each chapter file is self-contained; a subagent can be dispatched one chapter at a time.

---

## Rules Applied to All Chapters

- **TDD order per action**: Unit test (mock, failing) → TS handler (pass unit test) → C++ handler → integration test → commit
- **One commit per action** for TS+C+++test (merged); chapters may span 4-10 commits
- **Zero `as any`** in TS runtime code; use `unknown` / domain interfaces
- **Zero `UPackage::SavePackage`** — always `McpSafeAssetSave`
- **Game Thread API** — all UE Editor API calls must be on Game Thread (use `AsyncTask(ENamedThreads::GameThread, ...)` or `FFunctionGraphTask` in handler)
- **Error shape** — every response: `{ success: boolean, error?: string, errorCategory?: string }`
- **Path normalization** — use `normalizeBlueprintPath` / `/Game/` prefix enforcement from `src/utils/normalize.ts`
- **Integration test registration** — add scenarios to `tests/integration.mjs` (NOT a new subdir; existing flat layout preferred)

### Workflow corrections (post-Task 0 lessons)

- **UE Engine path is `D:\Unreal\UE_5.7`** (NOT `X:\Unreal_Engine\UE_5.7` — CLAUDE.md line was wrong). UBT batch file: `D:\Unreal\UE_5.7\Engine\Build\BatchFiles\Build.bat`.
- **Compile command** (use this, not anything referencing `McpAutomationBridgeEditor` target):
  ```
  "D:\Unreal\UE_5.7\Engine\Build\BatchFiles\Build.bat" UnrealEditor Win64 Development -Project="D:\Unreal\Project\War\War.uproject"
  ```
- **Kill UE Editor before every UBT compile** — otherwise plugin DLL is locked and UBT silently reuses the stale DLL (gives false "Succeeded"). Process name: `UnrealEditor-Win64-DebugGame.exe` or `UnrealEditor.exe`.
- **War project plugin is a COPY, not a symlink** — after each successful compile, sync via:
  ```
  node scripts/sync-mcp-plugin.js --project D:/Unreal/Project/War/Plugins
  ```

### UE 5.5+ API migrations (discovered during Task 0)

- `#include "Engine/UserDefinedStruct.h"` is a deprecated shim → use `#include "StructUtils/UserDefinedStruct.h"`
- `UStruct::IsA<T>()` template doesn't resolve → use `Cast<T>(Struct) != nullptr`

### `McpGenericAssetFactory::CreateAssetOfClass` signature (Task 0 final)

```cpp
UObject* CreateAssetOfClass(
    UClass* AssetClass,
    const FString& PackagePath,
    const FString& AssetName,
    TFunction<void(UObject*)> Configurator,
    FString& OutError,
    bool& bOutSaved);         // NEW: distinguishes create-success from save-success
```

Call sites pass `bool bSaved = false;` as the last argument and optionally report a warning when `bSaved == false`.

---

## Execution Handoff

Plan complete — **9 files** in `docs/superpowers/plans/`:
- `2026-04-20-mcp-tier123-expansion.md` (this master)
- `2026-04-20-mcp-ch1-blueprint-patches.md`
- `2026-04-20-mcp-ch2-data-table.md`
- `2026-04-20-mcp-ch3-data-asset.md`
- `2026-04-20-mcp-ch4-gameplay-tags.md`
- `2026-04-20-mcp-ch5-curve.md`
- `2026-04-20-mcp-ch6-umg-add-widget.md`
- `2026-04-20-mcp-ch7-state-tree.md`
- `2026-04-20-mcp-ch8-deploy-e2e.md`

**Execution options:**

1. **Subagent-Driven (recommended)** — I dispatch a fresh subagent per chapter file, review between chapters, fast iteration. Each subagent gets the one chapter plan + spec reference.

2. **Inline Execution** — Execute tasks in this session using `executing-plans` skill, batch with checkpoints.

**Which approach?**
