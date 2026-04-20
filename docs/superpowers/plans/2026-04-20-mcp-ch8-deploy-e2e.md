# Ch8 — Deployment + E2E Smoke Test

> **Parent plan:** `2026-04-20-mcp-tier123-expansion.md`
> **Spec:** §4 Ch8
> **Depends on:** Ch1-7 all merged
> **Estimated:** 0.5 day, 3-4 commits

**Goal:** Junction-link plugin into War project, run comprehensive E2E scenario, verify all new functionality works end-to-end against live UE 5.7 with War content.

---

## Task 1: Create `deploy-to-war.sh` deployment script

**Files:**
- Create: `scripts/deploy-to-war.sh`

- [ ] **Step 1: Write deployment script**

```bash
#!/usr/bin/env bash
# scripts/deploy-to-war.sh
# One-shot junction-link McpAutomationBridge plugin into /d/Unreal/Project/War/Plugins
# Safe to re-run: verifies existing link, backs up real dirs, skips if already correct.

set -euo pipefail

REPO_PLUGIN_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/plugins/McpAutomationBridge"
WAR_PLUGINS_DIR="/d/Unreal/Project/War/Plugins"
WAR_LINK="${WAR_PLUGINS_DIR}/McpAutomationBridge"
WAR_LINK_WIN='D:\Unreal\Project\War\Plugins\McpAutomationBridge'
REPO_PLUGIN_WIN='C:\Code\Unreal_mcp\plugins\McpAutomationBridge'

if [ ! -d "$REPO_PLUGIN_DIR" ]; then
    echo "ERROR: repo plugin dir not found: $REPO_PLUGIN_DIR"
    exit 1
fi
if [ ! -d "$WAR_PLUGINS_DIR" ]; then
    echo "ERROR: War Plugins dir not found: $WAR_PLUGINS_DIR"
    exit 1
fi

# If already a junction pointing to repo, do nothing
if [ -L "$WAR_LINK" ]; then
    TARGET=$(readlink "$WAR_LINK")
    if [ "$TARGET" = "$REPO_PLUGIN_DIR" ]; then
        echo "OK: junction already exists pointing to repo."
        exit 0
    fi
    echo "INFO: junction exists but points elsewhere ($TARGET) — removing."
    rm "$WAR_LINK"
fi

# If it's a real directory, back it up
if [ -d "$WAR_LINK" ]; then
    BACKUP="${WAR_LINK}.bak-$(date +%Y%m%d-%H%M%S)"
    echo "INFO: backing up existing directory to $BACKUP"
    mv "$WAR_LINK" "$BACKUP"
fi

# Create junction (requires Windows cmd.exe mklink)
echo "Creating junction: $WAR_LINK_WIN -> $REPO_PLUGIN_WIN"
cmd //c "mklink /J \"$WAR_LINK_WIN\" \"$REPO_PLUGIN_WIN\"" || {
    echo "ERROR: mklink failed. Run this script from an elevated shell."
    exit 1
}

echo "Deployed. Restart UE Editor to reload plugin."
```

- [ ] **Step 2: Make executable**

```bash
chmod +x scripts/deploy-to-war.sh
```

- [ ] **Step 3: Test run (will create junction if absent)**

```bash
./scripts/deploy-to-war.sh
```

Expected output:
```
Creating junction: D:\Unreal\Project\War\Plugins\McpAutomationBridge -> C:\Code\Unreal_mcp\plugins\McpAutomationBridge
Deployed. Restart UE Editor to reload plugin.
```

(Or "OK: junction already exists" on repeat.)

Verify:
```bash
ls -la /d/Unreal/Project/War/Plugins/McpAutomationBridge
# Expect: lrwxrwxrwx ... McpAutomationBridge -> C:/Code/Unreal_mcp/plugins/McpAutomationBridge
```

- [ ] **Step 4: Commit**
```bash
git add scripts/deploy-to-war.sh
git commit -m "chore(ch8): add deploy-to-war.sh junction setup script

Idempotent one-shot deployment for dev iteration against /d/Unreal/Project/War.
Backs up pre-existing real directories, verifies existing junctions."
```

---

## Task 2: Build verification against War project

**Files:** none (verification only)

- [ ] **Step 1: Full TypeScript build**

```bash
cd /c/Code/Unreal_mcp
npm run build:core
```

Expected: 0 errors, dist/ produced.

- [ ] **Step 2: Close any open UE Editor instance for War project**

Manually. The build step will fail if Editor has file locks.

- [ ] **Step 3: Build plugin via UBT**

```bash
"X:/Unreal_Engine/UE_5.7/Engine/Build/BatchFiles/Build.bat" \
  WarEditor Win64 Development \
  -Project="D:/Unreal/Project/War/War.uproject" \
  -WaitMutex -FromMsBuild
```

Expected: `Build successful` with 0 errors, 0 warnings introduced by our new code. Any new warnings must be investigated and fixed before proceeding.

- [ ] **Step 4: Launch UE Editor**

```bash
"X:/Unreal_Engine/UE_5.7/Engine/Binaries/Win64/UnrealEditor.exe" \
  "D:/Unreal/Project/War/War.uproject"
```

Wait for Editor to load, confirm `McpAutomationBridge` appears in Edit→Plugins.

- [ ] **Step 5: Smoke-check plugin state (manual, 2 min)**

In UE log / Output Log, grep for:
- `McpAutomationBridge: Tool registry initialized with N tools` — `N` should be `35 + 3 = 38` (3 new: `manage_data`, `manage_gameplay_tags`, `manage_curve`)
- No `LogMcp: Error` entries at startup

---

## Task 3: Run full integration regression

- [ ] **Step 1: Run existing integration tests**

```bash
cd /c/Code/Unreal_mcp
npm test
```

Expected: all existing scenarios pass OR existing-scenario failures are pre-existing (not introduced by Ch1-7).

Record baseline: pre-Ch1 pass count vs post-Ch7 pass count. New scenarios added in Ch1-7 should all PASS.

- [ ] **Step 2: Run unit tests**

```bash
npx vitest run
```

Expected: all pass (including new tests in `data-handlers.test.ts`, `gameplay-tags-handlers.test.ts`, `curve-handlers.test.ts`, and patches in existing test files).

- [ ] **Step 3: If regressions found, triage and fix per-chapter**

Revert the suspect chapter's commits if not fixable in one round; document in `docs/superpowers/plans/2026-04-20-mcp-ch8-regression-log.md` and re-run this plan's Ch on the impacted chapter.

- [ ] **Step 4: If all green, commit baseline snapshot (optional)**

```bash
git commit --allow-empty -m "test(ch8): integration + unit regression baseline green"
```

---

## Task 4: E2E War scenario (`tests/scenarios/war-e2e.mjs`)

**Files:**
- Create: `tests/scenarios/war-e2e.mjs`
- Modify: `tests/integration.mjs` (or run standalone — see Step 5)

- [ ] **Step 1: Write the scenario file**

```javascript
#!/usr/bin/env node
// tests/scenarios/war-e2e.mjs
// End-to-end: exercises ALL new Ch1-7 actions against War project
// in a single task-8-realistic flow.

import { runToolTests } from '../test-runner.mjs';

const ROOT = '/Game/War/Data/ModifierKeys';

const testCases = [
  // =============================================================
  // 1) Struct: create ST_ModifierKeyRow (uses manage_blueprint)
  // =============================================================
  { scenario: 'E2E.1: create struct ST_ModifierKeyRow', toolName: 'manage_blueprint',
    arguments: { action: 'create_struct', path: ROOT, name: 'ST_ModifierKeyRow' },
    expected: 'success|already exists' },

  // =============================================================
  // 2) Struct fields: Name:FName, Value:double, Description:FText
  // =============================================================
  { scenario: 'E2E.2a: add field Name', toolName: 'manage_blueprint',
    arguments: { action: 'modify_struct', path: `${ROOT}/ST_ModifierKeyRow`, op: 'add_variable', varName: 'Name', varType: 'name' },
    expected: 'success|already exists' },
  { scenario: 'E2E.2b: add field Value', toolName: 'manage_blueprint',
    arguments: { action: 'modify_struct', path: `${ROOT}/ST_ModifierKeyRow`, op: 'add_variable', varName: 'Value', varType: 'double' },
    expected: 'success|already exists' },
  { scenario: 'E2E.2c: add field Description', toolName: 'manage_blueprint',
    arguments: { action: 'modify_struct', path: `${ROOT}/ST_ModifierKeyRow`, op: 'add_variable', varName: 'Description', varType: 'text' },
    expected: 'success|already exists' },

  // =============================================================
  // 3) DataTable: DT_ModifierKeys referencing the struct
  // =============================================================
  { scenario: 'E2E.3: create DT_ModifierKeys', toolName: 'manage_data',
    arguments: { action: 'create_data_table', path: ROOT, name: 'DT_ModifierKeys',
                 rowStructPath: `${ROOT}/ST_ModifierKeyRow.ST_ModifierKeyRow` },
    expected: 'success|already exists' },

  // =============================================================
  // 4) Add 3 rows via DataTable API (exercises struct reflection)
  // =============================================================
  { scenario: 'E2E.4a: add row Rain', toolName: 'manage_data',
    arguments: { action: 'add_data_table_row', path: `${ROOT}/DT_ModifierKeys`, rowName: 'Rain',
                 fields: { Name: 'Rain', Value: 1.25, Description: 'Rain buff' } },
    expected: 'success|already exists' },
  { scenario: 'E2E.4b: add row Snow', toolName: 'manage_data',
    arguments: { action: 'add_data_table_row', path: `${ROOT}/DT_ModifierKeys`, rowName: 'Snow',
                 fields: { Name: 'Snow', Value: 0.8, Description: 'Snow penalty' } },
    expected: 'success|already exists' },
  { scenario: 'E2E.4c: add row Drought', toolName: 'manage_data',
    arguments: { action: 'add_data_table_row', path: `${ROOT}/DT_ModifierKeys`, rowName: 'Drought',
                 fields: { Name: 'Drought', Value: 0.5, Description: 'Drought penalty' } },
    expected: 'success|already exists' },

  // =============================================================
  // 5) DataAsset: create instance referencing struct values
  // =============================================================
  { scenario: 'E2E.5a: create DataAsset BP parent', toolName: 'manage_blueprint',
    arguments: { action: 'create', name: 'BP_WarModifierSet', path: ROOT, parentClass: '/Script/Engine.DataAsset' },
    expected: 'success|already exists' },
  { scenario: 'E2E.5b: add DT ref var', toolName: 'manage_blueprint',
    arguments: { action: 'add_variable', blueprintPath: `${ROOT}/BP_WarModifierSet`, name: 'ModifierTable', type: 'object /Script/Engine.DataTable' },
    expected: 'success|already exists' },
  { scenario: 'E2E.5c: create DataAsset instance', toolName: 'manage_data',
    arguments: { action: 'create_data_asset', path: ROOT, name: 'DA_Modifiers',
                 dataAssetClassPath: `${ROOT}/BP_WarModifierSet.BP_WarModifierSet_C` },
    expected: 'success|already exists' },
  { scenario: 'E2E.5d: set DA property', toolName: 'manage_data',
    arguments: { action: 'set_data_asset_property', path: `${ROOT}/DA_Modifiers`, propertyPath: 'ModifierTable',
                 value: `${ROOT}/DT_ModifierKeys.DT_ModifierKeys` },
    expected: 'success' },

  // =============================================================
  // 6) Curve: decay curve for modifiers
  // =============================================================
  { scenario: 'E2E.6a: create decay curve', toolName: 'manage_curve',
    arguments: { action: 'create_curve_float', path: ROOT, name: 'C_ModifierDecay' },
    expected: 'success|already exists' },
  { scenario: 'E2E.6b: set decay keys', toolName: 'manage_curve',
    arguments: { action: 'set_curve_keys', path: `${ROOT}/C_ModifierDecay`,
                 keys: [{ time: 0, value: 1, interpMode: 'Auto' }, { time: 5, value: 0.5, interpMode: 'Auto' }, { time: 10, value: 0, interpMode: 'Linear' }] },
    expected: 'success' },

  // =============================================================
  // 7) GameplayTag: Modifier.Weather.Rain
  // =============================================================
  { scenario: 'E2E.7: add gameplay tag', toolName: 'manage_gameplay_tags',
    arguments: { action: 'add_gameplay_tag', tag: 'Modifier.Weather.Rain', comment: 'Rain modifier' },
    expected: 'success|already exists' },

  // =============================================================
  // 8) Blueprint: reparent + add interface
  // =============================================================
  { scenario: 'E2E.8a: create test BP', toolName: 'manage_blueprint',
    arguments: { action: 'create', name: 'BP_WarE2E', path: ROOT, parentClass: 'Actor' },
    expected: 'success|already exists' },
  { scenario: 'E2E.8b: reparent to Pawn', toolName: 'manage_blueprint',
    arguments: { action: 'set_parent_class', blueprintPath: `${ROOT}/BP_WarE2E`, parentClass: '/Script/Engine.Pawn' },
    expected: 'success' },
  { scenario: 'E2E.8c: add interface', toolName: 'manage_blueprint',
    arguments: { action: 'add_interface', blueprintPath: `${ROOT}/BP_WarE2E`, interfacePath: '/Script/Engine.Interface_AssetUserData' },
    expected: 'success' },

  // =============================================================
  // 9) Verify final state
  // =============================================================
  { scenario: 'E2E.9a: verify DT rows (expect 3)', toolName: 'manage_data',
    arguments: { action: 'list_data_table_rows', path: `${ROOT}/DT_ModifierKeys` },
    expected: 'success' },
  { scenario: 'E2E.9b: verify curve keys (expect 3)', toolName: 'manage_curve',
    arguments: { action: 'inspect_curve', path: `${ROOT}/C_ModifierDecay` },
    expected: 'success' },
  { scenario: 'E2E.9c: verify tag present', toolName: 'manage_gameplay_tags',
    arguments: { action: 'list_gameplay_tags', prefix: 'Modifier.Weather' },
    expected: 'success' },
  { scenario: 'E2E.9d: verify BP interfaces (expect 1)', toolName: 'manage_blueprint',
    arguments: { action: 'list_interfaces', blueprintPath: `${ROOT}/BP_WarE2E` },
    expected: 'success' },
];

await runToolTests({
  testCases,
  label: 'War E2E integration',
  targetProject: 'D:/Unreal/Project/War/War.uproject'
});
```

(Adjust `runToolTests` call signature to match `tests/test-runner.mjs` actual API — check the file before running.)

- [ ] **Step 2: Run E2E (requires Editor running against War project)**

```bash
node tests/scenarios/war-e2e.mjs
```

Expected: 22 scenarios PASS. Any FAIL triggers bisect (revert suspect Ch chapter's commit, rerun).

- [ ] **Step 3: Manual verification in Editor (5 min)**

Open UE Editor for War project:
1. Content Browser → `/Game/War/Data/ModifierKeys` → verify `ST_ModifierKeyRow`, `DT_ModifierKeys`, `DA_Modifiers`, `C_ModifierDecay`, `BP_WarE2E` all present
2. Open `DT_ModifierKeys` → verify 3 rows with populated fields
3. Open `C_ModifierDecay` → verify 3 keyframes
4. Open `BP_WarE2E` → verify parent is Pawn, Interfaces tab shows Interface_AssetUserData
5. Project Settings → GameplayTags → verify `Modifier.Weather.Rain` visible in tag tree

- [ ] **Step 4: Commit E2E scenario**
```bash
git add tests/scenarios/war-e2e.mjs
git commit -m "test(ch8): add War E2E integration scenario

22-step end-to-end test exercising all Ch1-7 new actions against the
real War project content layout. Validates Task 8 (ModifierKeys)
authoring workflow unblocks."
```

---

## Task 5: Final documentation + close

**Files:**
- Modify: `README.md` (new tools section)
- Modify: `docs/handler-mapping.md` (if exists — add 3 new tools and extensions)
- Modify: `CHANGELOG.md` (new 0.5.19 or whatever next version)

- [ ] **Step 1: Update README tool count**

Search for "35 consolidated tools" / "36 consolidated tools" in README; bump to `38` (3 new added).

In the tool list (if present), add 3 new entries with one-line descriptions:
- `manage_data` — UDataTable + UDataAsset CRUD
- `manage_gameplay_tags` — GameplayTag registry ini operations
- `manage_curve` — UCurveFloat authoring

- [ ] **Step 2: Update `docs/handler-mapping.md`**

Add sections for the 3 new tools with their action → C++ handler mappings.

- [ ] **Step 3: Update `CHANGELOG.md`**

```markdown
## [0.6.0] - 2026-04-XX

### Added
- `manage_data` tool: 12 actions for UDataTable + UDataAsset CRUD
- `manage_gameplay_tags` tool: 4 actions for GameplayTag ini management
- `manage_curve` tool: 4 actions for UCurveFloat authoring
- `manage_blueprint`: `set_parent_class`, `add_interface`, `remove_interface`, `list_interfaces` actions
- `manage_widget_authoring`: `add_widget`, `remove_widget` generic actions
- `manage_ai`: StateTree schema completion (contextClass, stateType, taskProps, add_task, list_states, remove_state)
- Shared C++ helpers: `McpStructReflection`, `McpGenericAssetFactory`, `McpPropertyPath`

### Changed
- Tool count: 35 → 38

### Compatibility
- Target UE 5.7. Earlier versions (5.0-5.6) may work for all actions except StateTree (5.2+); not tested.
```

- [ ] **Step 4: Version bump**

```bash
# Version files per CLAUDE.md: package.json, server.json, src/index.ts
# (Optionally use .github/workflows/bump-version.yml if it supports manual bump.)
```

Bump each to `0.6.0` (major-feature bump).

- [ ] **Step 5: Commit + tag**

```bash
git add README.md docs/handler-mapping.md CHANGELOG.md package.json server.json src/index.ts
git commit -m "docs: Tier 1+2+3 expansion release (0.6.0)

Tool count 35→38. Adds manage_data / manage_gameplay_tags / manage_curve.
Extends manage_blueprint, manage_widget_authoring, manage_ai.
Shared C++ scaffolding: McpStructReflection, McpGenericAssetFactory, McpPropertyPath.
Target: UE 5.7. See CHANGELOG for full action list."

git tag -a v0.6.0 -m "Tier 1+2+3 MCP expansion"
# Do NOT git push --tags without user confirmation
```

---

## Acceptance Checklist (Ch8)

- [ ] `scripts/deploy-to-war.sh` idempotent and tested
- [ ] UBT build: 0 new errors/warnings
- [ ] UE Editor launches with plugin loaded, 38 tools registered
- [ ] All existing integration tests pass (no regressions)
- [ ] All unit tests pass
- [ ] `tests/scenarios/war-e2e.mjs` all 22 scenarios PASS
- [ ] Manual verification in Editor confirms assets created & valid
- [ ] Version bumped in 4 files, tag created locally (not pushed)
- [ ] README + CHANGELOG + handler-mapping updated

---

## Acceptance Checklist (Entire Expansion, across Ch1-8)

- [ ] ~30 new MCP actions available
- [ ] ~40 unit test cases green
- [ ] ~40 integration scenarios green
- [ ] 3 new tools registered (manage_data, manage_gameplay_tags, manage_curve)
- [ ] 3 tools extended (manage_blueprint, manage_widget_authoring, manage_ai)
- [ ] 3 shared C++ helpers landed (McpStructReflection, McpGenericAssetFactory, McpPropertyPath)
- [ ] War project's Task 8 unblocked (ST_ModifierKeyRow + DT_ModifierKeys creatable via MCP)
- [ ] Plugin deployed to War via junction, auto-updates on rebuild
- [ ] Zero `as any`, zero `UPackage::SavePackage`, zero `ANY_PACKAGE`
- [ ] Changelog documents the 0.6.0 release

---

## Notes for Subagent

- **Do NOT `git push --tags`** without explicit user approval; per CLAUDE.md tag pushes are shared-state and require confirmation.
- **Close UE Editor before UBT build** — file locks on compiled DLLs will block incremental builds. Editor can be launched AFTER build finishes.
- **E2E test scope** — the 22-step scenario in Task 4 relies on ALL Ch1-7 chapters being merged and working. If any chapter fails, fix it before running Task 4 rather than skipping steps in the E2E.
- **If `manage_blueprint/modify_struct` varType enum doesn't accept `'name'` / `'text'` strings** — check the actual enum in `consolidated-tool-definitions.ts`. Adjust the E2E scenario strings accordingly. This is a data-structure mismatch risk, not a handler bug.
