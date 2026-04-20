# Ch4 — `manage_gameplay_tags` (4 actions)

> **Parent plan:** `2026-04-20-mcp-tier123-expansion.md`
> **Spec:** §4 Ch4
> **Depends on:** Task 0 (trivially — no shared helpers consumed)
> **Estimated:** 0.5 day, 5 commits

**Goal:** New `manage_gameplay_tags` tool for GameplayTag ini CRUD. Independent from other chapters (ini operations, not asset ops).

---

## Task 1: Bootstrap `manage_gameplay_tags` tool

**Files:**
- Modify: `src/tools/consolidated-tool-definitions.ts` (append tool)
- Create: `src/tools/handlers/gameplay-tags-handlers.ts`
- Create: `src/tools/handlers/gameplay-tags-handlers.test.ts`
- Modify: `src/tools/consolidated-tool-handlers.ts` (import + route)
- Create: `plugins/.../Private/MCP/Tools/McpTool_ManageGameplayTags.cpp`
- Create: `plugins/.../Private/McpAutomationBridge_GameplayTagsHandlers.cpp`
- Modify: `plugins/.../McpAutomationBridge.Build.cs` (add GameplayTags module if not present)

- [ ] **Step 1: Append tool definition**

In `consolidated-tool-definitions.ts`:

```typescript
{
  name: 'manage_gameplay_tags',
  category: 'gameplay',
  description: 'Manage GameplayTag registrations in project config (ini-based tags).',
  inputSchema: {
    type: 'object',
    properties: {
      action: {
        type: 'string',
        enum: ['add_gameplay_tag', 'remove_gameplay_tag', 'list_gameplay_tags', 'add_gameplay_tag_source'],
      },
      tag: { type: 'string', description: 'Tag name, e.g. "Modifier.Weather.Rain".' },
      comment: { type: 'string', description: 'Dev comment for the tag.' },
      sourceIni: { type: 'string', description: 'Ini file (relative to Config/). Default: DefaultGameplayTags.ini' },
      prefix: { type: 'string', description: 'Optional prefix filter for list_gameplay_tags.' },
      iniRelativePath: { type: 'string', description: 'Path for add_gameplay_tag_source.' }
    },
    required: ['action']
  },
  outputSchema: {
    type: 'object',
    properties: {
      success: { type: 'boolean' },
      error: { type: 'string' },
      errorCategory: { type: 'string' },
      tag: { type: 'string' },
      sourceIni: { type: 'string' },
      tags: { type: 'array', items: { type: 'string' } }
    }
  }
}
```

- [ ] **Step 2: Handler skeleton**

```typescript
// src/tools/handlers/gameplay-tags-handlers.ts
import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';

export async function handleGameplayTagsTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const argsRecord = args as Record<string, unknown>;
  switch (action) {
    default:
      throw new Error(`Unsupported manage_gameplay_tags action: ${action}`);
  }
}
```

- [ ] **Step 3: Wire route**

In `consolidated-tool-handlers.ts`:
```typescript
import { handleGameplayTagsTools } from './handlers/gameplay-tags-handlers.js';
// ...
case 'manage_gameplay_tags':
  return await handleGameplayTagsTools(action, args, tools);
```

- [ ] **Step 4: C++ tool dispatcher**

`plugins/.../Private/MCP/Tools/McpTool_ManageGameplayTags.cpp`:
```cpp
#include "McpToolDefinition.h"
#include "McpToolRegistry.h"

namespace McpTool_ManageGameplayTags
{
    static FMcpToolDefinition MakeDefinition()
    {
        FMcpToolDefinition Def;
        Def.Name = TEXT("manage_gameplay_tags");
        Def.Category = TEXT("gameplay");
        Def.Description = TEXT("GameplayTag ini CRUD.");
        Def.ActionMapping = {
            {TEXT("add_gameplay_tag"), TEXT("gt_add")},
            {TEXT("remove_gameplay_tag"), TEXT("gt_remove")},
            {TEXT("list_gameplay_tags"), TEXT("gt_list")},
            {TEXT("add_gameplay_tag_source"), TEXT("gt_add_source")}
        };
        return Def;
    }
    static struct FRegister { FRegister() { FMcpToolRegistry::Get().Register(MakeDefinition()); } } GRegister;
}
```

- [ ] **Step 5: Handlers file skeleton**

`plugins/.../Private/McpAutomationBridge_GameplayTagsHandlers.cpp`:
```cpp
#include "McpAutomationBridgeSubsystem.h"
#include "GameplayTagsManager.h"
#include "GameplayTagsModule.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

// Handler impls added per task below.
```

- [ ] **Step 6: Build.cs dependencies**

In `plugins/.../McpAutomationBridge.Build.cs`, ensure `PrivateDependencyModuleNames` contains:
```csharp
"GameplayTags",
"GameplayTagsEditor",
```

- [ ] **Step 7: Compile smoke**
```bash
npm run build:core
# UE plugin rebuild
```
Expected: build clean; all 4 actions throw "Unsupported" until handlers land.

- [ ] **Step 8: Commit**
```bash
git add src/tools/consolidated-tool-definitions.ts src/tools/handlers/gameplay-tags-handlers.ts src/tools/handlers/gameplay-tags-handlers.test.ts src/tools/consolidated-tool-handlers.ts plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Tools/McpTool_ManageGameplayTags.cpp plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_GameplayTagsHandlers.cpp plugins/McpAutomationBridge/Source/McpAutomationBridge/McpAutomationBridge.Build.cs
git commit -m "feat(manage_gameplay_tags): bootstrap tool skeleton

4-action enum registered; handlers throw unsupported. Adds GameplayTags
+ GameplayTagsEditor module deps."
```

---

## Task 2: `add_gameplay_tag` action

- [ ] **Step 1: Unit test**

```typescript
import { describe, it, expect, vi } from 'vitest';
import { handleGameplayTagsTools } from './gameplay-tags-handlers.js';

describe('manage_gameplay_tags: add_gameplay_tag', () => {
  it('forwards with tag, comment, sourceIni defaults', async () => {
    const mockTools = { executeAutomation: vi.fn().mockResolvedValue({
      success: true, tag: 'Modifier.Weather.Rain', sourceIni: 'DefaultGameplayTags.ini'
    }) };
    const res = await handleGameplayTagsTools(
      'add_gameplay_tag',
      { tag: 'Modifier.Weather.Rain', comment: 'Rain' } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(res.success).toBe(true);
    expect(res.tag).toBe('Modifier.Weather.Rain');
  });
});
```
Run: FAIL.

- [ ] **Step 2: TS handler**

```typescript
case 'add_gameplay_tag': {
  const tag = argsRecord.tag as string | undefined;
  const comment = (argsRecord.comment as string | undefined) ?? '';
  const sourceIni = (argsRecord.sourceIni as string | undefined) ?? 'DefaultGameplayTags.ini';
  if (!tag) throw new Error('Missing required parameter: tag');
  const res = await executeAutomationRequest(tools, 'gt_add', { tag, comment, sourceIni });
  return cleanObject(res as Record<string, unknown>) as Record<string, unknown>;
}
```
Run: PASS.

- [ ] **Step 3: C++ handler**

```cpp
TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleGtAdd(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    FString TagStr, Comment, SourceIni;
    if (!Params->TryGetStringField(TEXT("tag"), TagStr))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Missing tag"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }
    Params->TryGetStringField(TEXT("comment"), Comment);
    Params->TryGetStringField(TEXT("sourceIni"), SourceIni);

    UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
    const FString RestrictedSourceName = SourceIni.IsEmpty() ? FString() : SourceIni;
    const bool bAdded = Manager.AddNewGameplayTagToINI(TagStr, Comment, FName(*RestrictedSourceName));
    if (!bAdded)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("AddNewGameplayTagToINI failed for %s"), *TagStr));
        Response->SetStringField(TEXT("errorCategory"), TEXT("EngineAPIError"));
        return Response;
    }

    Manager.EditorRefreshGameplayTagTree();

    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("tag"), TagStr);
    Response->SetStringField(TEXT("sourceIni"), SourceIni.IsEmpty() ? TEXT("DefaultGameplayTags.ini") : SourceIni);
    return Response;
}
```

Register: `gt_add`.

- [ ] **Step 4: Integration**
```javascript
{ scenario: 'GT: add Modifier.Weather.Rain', toolName: 'manage_gameplay_tags',
  arguments: { action: 'add_gameplay_tag', tag: 'Modifier.Weather.Rain', comment: 'Rain modifier' },
  expected: 'success|already exists' },
```

- [ ] **Step 5: Compile + run**. Expected PASS.

- [ ] **Step 6: Commit**
```bash
git add -u
git commit -m "feat(manage_gameplay_tags): add add_gameplay_tag action

UGameplayTagsManager::AddNewGameplayTagToINI + EditorRefreshGameplayTagTree.
Default sourceIni is DefaultGameplayTags.ini."
```

---

## Task 3: `list_gameplay_tags` action

- [ ] **Step 1: Unit test**

```typescript
describe('manage_gameplay_tags: list_gameplay_tags', () => {
  it('returns filtered tags by prefix', async () => {
    const mockTools = { executeAutomation: vi.fn().mockResolvedValue({
      success: true, tags: ['Modifier.Weather.Rain', 'Modifier.Weather.Snow']
    }) };
    const res = await handleGameplayTagsTools(
      'list_gameplay_tags',
      { prefix: 'Modifier.Weather' } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(res.tags).toHaveLength(2);
  });
});
```
Run: FAIL.

- [ ] **Step 2: TS handler**

```typescript
case 'list_gameplay_tags': {
  const prefix = argsRecord.prefix as string | undefined;
  const res = await executeAutomationRequest(tools, 'gt_list', { prefix });
  return cleanObject(res as Record<string, unknown>) as Record<string, unknown>;
}
```
Run: PASS.

- [ ] **Step 3: C++ handler**

```cpp
TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleGtList(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    FString Prefix;
    Params->TryGetStringField(TEXT("prefix"), Prefix);

    FGameplayTagContainer All;
    UGameplayTagsManager::Get().RequestAllGameplayTags(All, /*OnlyIncludeDictTagsInContainer=*/false);

    TArray<TSharedPtr<FJsonValue>> Tags;
    for (const FGameplayTag& T : All)
    {
        const FString S = T.ToString();
        if (Prefix.IsEmpty() || S.StartsWith(Prefix))
        {
            Tags.Add(MakeShared<FJsonValueString>(S));
        }
    }
    Response->SetBoolField(TEXT("success"), true);
    Response->SetArrayField(TEXT("tags"), Tags);
    return Response;
}
```

Register: `gt_list`.

- [ ] **Step 4: Integration**
```javascript
{ scenario: 'GT: list with prefix', toolName: 'manage_gameplay_tags',
  arguments: { action: 'list_gameplay_tags', prefix: 'Modifier.Weather' },
  expected: 'success' },
```

- [ ] **Step 5: Compile + run**. Expected PASS with `tags` containing `Modifier.Weather.Rain`.

- [ ] **Step 6: Commit**
```bash
git add -u
git commit -m "feat(manage_gameplay_tags): add list_gameplay_tags action

Uses RequestAllGameplayTags (includes parent tags auto-generated from
dotted leaf paths). Optional prefix filter."
```

---

## Task 4: `remove_gameplay_tag` action

- [ ] **Step 1: Unit test**

```typescript
describe('manage_gameplay_tags: remove_gameplay_tag', () => {
  it('forwards tag', async () => {
    const mockTools = { executeAutomation: vi.fn().mockResolvedValue({ success: true }) };
    await handleGameplayTagsTools(
      'remove_gameplay_tag',
      { tag: 'Modifier.Weather.Rain' } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(mockTools.executeAutomation).toHaveBeenCalledWith('gt_remove', expect.objectContaining({ tag: 'Modifier.Weather.Rain' }));
  });
});
```
Run: FAIL.

- [ ] **Step 2: TS handler**

```typescript
case 'remove_gameplay_tag': {
  const tag = argsRecord.tag as string | undefined;
  if (!tag) throw new Error('Missing required parameter: tag');
  const res = await executeAutomationRequest(tools, 'gt_remove', { tag });
  return cleanObject(res as Record<string, unknown>) as Record<string, unknown>;
}
```
Run: PASS.

- [ ] **Step 3: C++ handler**

```cpp
TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleGtRemove(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    FString TagStr;
    if (!Params->TryGetStringField(TEXT("tag"), TagStr))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Missing tag"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }

    // UGameplayTagsManager has no public remove API; edit DefaultGameplayTags.ini directly.
    const FString IniFilePath = FPaths::ProjectConfigDir() / TEXT("DefaultGameplayTags.ini");
    const FString Section = TEXT("/Script/GameplayTags.GameplayTagsList");

    TArray<FString> Lines;
    if (GConfig->GetArray(*Section, TEXT("GameplayTagList"), Lines, IniFilePath) > 0)
    {
        int32 Removed = Lines.RemoveAll([&TagStr](const FString& Entry) {
            return Entry.Contains(FString::Printf(TEXT("Tag=\"%s\""), *TagStr));
        });
        if (Removed > 0)
        {
            GConfig->SetArray(*Section, TEXT("GameplayTagList"), Lines, IniFilePath);
            GConfig->Flush(false, IniFilePath);
        }
    }

    // Reload manager to reflect ini change
    UGameplayTagsManager::Get().EditorRefreshGameplayTagTree();

    Response->SetBoolField(TEXT("success"), true);
    return Response;
}
```

Register: `gt_remove`.

- [ ] **Step 4: Integration**
```javascript
{ scenario: 'GT: remove tag', toolName: 'manage_gameplay_tags',
  arguments: { action: 'remove_gameplay_tag', tag: 'Modifier.Weather.Rain' },
  expected: 'success' },
{ scenario: 'GT: list after remove', toolName: 'manage_gameplay_tags',
  arguments: { action: 'list_gameplay_tags', prefix: 'Modifier.Weather.Rain' },
  expected: 'success' },
```

- [ ] **Step 5: Compile + run**. Expected PASS; second scenario should return empty `tags`.

- [ ] **Step 6: Commit**
```bash
git add -u
git commit -m "feat(manage_gameplay_tags): add remove_gameplay_tag action

Direct GConfig edit of DefaultGameplayTags.ini (no public API exists).
Flushes + refreshes tag tree so Editor UI updates immediately."
```

---

## Task 5: `add_gameplay_tag_source` action

- [ ] **Step 1: Unit test**

```typescript
describe('manage_gameplay_tags: add_gameplay_tag_source', () => {
  it('forwards iniRelativePath', async () => {
    const mockTools = { executeAutomation: vi.fn().mockResolvedValue({ success: true }) };
    await handleGameplayTagsTools(
      'add_gameplay_tag_source',
      { iniRelativePath: 'Tags/CombatTags.ini' } as unknown as Record<string, unknown>,
      mockTools as never
    );
    expect(mockTools.executeAutomation).toHaveBeenCalledWith('gt_add_source', expect.objectContaining({ iniRelativePath: 'Tags/CombatTags.ini' }));
  });
});
```
Run: FAIL.

- [ ] **Step 2: TS handler**

```typescript
case 'add_gameplay_tag_source': {
  const iniRelativePath = argsRecord.iniRelativePath as string | undefined;
  if (!iniRelativePath) throw new Error('Missing required parameter: iniRelativePath');
  const res = await executeAutomationRequest(tools, 'gt_add_source', { iniRelativePath });
  return cleanObject(res as Record<string, unknown>) as Record<string, unknown>;
}
```
Run: PASS.

- [ ] **Step 3: C++ handler**

```cpp
TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleGtAddSource(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    FString RelPath;
    if (!Params->TryGetStringField(TEXT("iniRelativePath"), RelPath))
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Missing iniRelativePath"));
        Response->SetStringField(TEXT("errorCategory"), TEXT("InvalidParams"));
        return Response;
    }
    const FString FullPath = FPaths::ProjectConfigDir() / RelPath;
    if (!FPaths::FileExists(FullPath))
    {
        // Create empty file with minimal header
        const FString Header = TEXT("[/Script/GameplayTags.GameplayTagsList]\n");
        FFileHelper::SaveStringToFile(Header, *FullPath);
    }

    // Register as an additional source on UGameplayTagsManager
    UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
    // Add to project settings' ExtraTagIniFiles list
    UGameplayTagsSettings* Settings = GetMutableDefault<UGameplayTagsSettings>();
    if (Settings)
    {
        FConfigFile ExtraConfig;
        FGameplayTagSource Source;
        Source.SourceName = FName(*RelPath);
        Source.SourceType = EGameplayTagSourceType::TagList;
        // Ensure the Settings file has the ref; persist to DefaultGameplayTags.ini
        Manager.EditorRefreshGameplayTagTree();
    }

    Response->SetBoolField(TEXT("success"), true);
    return Response;
}
```

Register: `gt_add_source`.

NOTE: UE `UGameplayTagsSettings::ExtraTagIniFiles` API shape varies across versions. Verify against `X:\Unreal_Engine\UE_5.7\Engine\Source\Runtime\GameplayTags\Classes\GameplayTagsSettings.h` before committing. If the API lookup indicates a different property name, adapt.

- [ ] **Step 4: Integration**
```javascript
{ scenario: 'GT: add tag source ini', toolName: 'manage_gameplay_tags',
  arguments: { action: 'add_gameplay_tag_source', iniRelativePath: 'Tags/CombatTags.ini' },
  expected: 'success' },
```

- [ ] **Step 5: Compile + run**. Expected PASS. Verify `War/Config/Tags/CombatTags.ini` file exists after.

- [ ] **Step 6: Commit**
```bash
git add -u
git commit -m "feat(manage_gameplay_tags): add add_gameplay_tag_source action

Creates an additional tag-list ini under Config/. Subsequent
add_gameplay_tag calls with sourceIni=<relativePath> write here."
```

---

## Acceptance Checklist (Ch4)

- [ ] 5 commits
- [ ] Unit tests pass
- [ ] 5 new integration scenarios pass
- [ ] `War/Config/DefaultGameplayTags.ini` updated in-place (add → list → remove round-trip)
- [ ] EditorRefreshGameplayTagTree causes Editor tag dropdown to reflect changes immediately (manual check once)

---

## Notes for Subagent

- **`EditorRefreshGameplayTagTree` is editor-only** — wrap in `#if WITH_EDITOR` if the handler is compiled for runtime (should not be; these handlers are editor-only by default).
- **ini comment preservation**: `GConfig->SetArray` rewrites the whole section, losing `;`-comments. For War's main config this is acceptable (SCM tracks the file); mention in commit if any user-authored ini comments might be destroyed.
- **Task 5's `UGameplayTagsSettings::ExtraTagIniFiles` API must be verified** against 5.7 headers before finalizing the handler — this is the single biggest risk in Ch4.
