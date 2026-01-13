# Plan: Gameplay Ability System (GAS) Tool Implementation

## Objective
Implement 4 new consolidated tools to provide full GAS coverage in the Unreal MCP server:
1.  `manage_gameplay_abilities` (Authoring)
2.  `manage_attribute_sets` (Authoring)
3.  `manage_gameplay_cues` (Authoring)
4.  `test_gameplay_abilities` (Runtime Testing)

## 1. Schema Definition
**File**: `src/tools/consolidated-tool-definitions.ts`

Add the following tool definitions:

### `manage_gameplay_abilities`
*   **Category**: `gameplay`
*   **Description**: Create and configure Gameplay Abilities, Effects, and Tasks.
*   **Actions**:
    *   `create_gameplay_ability`
    *   `set_ability_tags` (AbilityTags, Cancel/Block tags)
    *   `set_ability_costs` (Cost GE)
    *   `set_ability_cooldown` (Cooldown GE)
    *   `set_ability_targeting` (Targeting type/range)
    *   `add_ability_task` (Wait, Montage, etc.)
    *   `set_activation_policy` (NetExecutionPolicy)
    *   `set_instancing_policy` (InstancingPolicy)
    *   `create_gameplay_effect`
    *   `set_effect_duration`
    *   `add_effect_modifier`
    *   `set_modifier_magnitude`
    *   `add_effect_execution_calculation`
    *   `add_effect_cue` (Effect-side cue)
    *   `set_effect_stacking`
    *   `set_effect_tags`

### `manage_attribute_sets`
*   **Category**: `gameplay`
*   **Description**: Create Blueprint AttributeSets and manage attributes.
*   **Actions**:
    *   `add_ability_system_component` (to Actor BP)
    *   `configure_asc` (Replication Mode)
    *   `create_attribute_set`
    *   `add_attribute` (FGameplayAttributeData)
    *   `set_attribute_base_value` (CDO reflection)
    *   `set_attribute_clamping` (Min/Max variables)

### `manage_gameplay_cues`
*   **Category**: `gameplay`
*   **Description**: Create and configure Gameplay Cue Notifies (Static/Actor).
*   **Actions**:
    *   `create_gameplay_cue_notify`
    *   `configure_cue_trigger` (OnExecute, WhileActive, etc.)
    *   `set_cue_effects` (Particle, Sound, CameraShake)
    *   `add_tag_to_asset` (Universal tag adder)

### `test_gameplay_abilities`
*   **Category**: `gameplay`
*   **Description**: Runtime GAS testing (Activate Ability, Apply Effect, Query Attributes).
*   **Actions**:
    *   `test_activate_ability` (by Class or Tag)
    *   `test_apply_effect` (Apply GE to Self/Target)
    *   `test_get_attribute` (Query current value)
    *   `test_add_tag` (Runtime ASC tag)
    *   `test_remove_tag` (Runtime ASC tag)

## 2. TypeScript Implementation
**File**: `src/tools/handlers/gas-handlers.ts`
**File**: `src/tools/consolidated-tool-handlers.ts`

1.  **Refactor `handleGASTools`**:
    *   Ensure it can handle actions from all 4 new tools.
    *   Map all TS actions to the single C++ action `manage_gas`.
    *   Pass the TS action name as the `subAction` in the JSON payload.
2.  **Registration**:
    *   Register all 4 new tools in `consolidated-tool-handlers.ts` to point to `handleGASTools`.

## 3. C++ Implementation
**File**: `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/McpAutomationBridge_GASHandlers.cpp`

The C++ handler already implements Authoring actions (13.1, 13.2, 13.3). We must add **13.4 RUNTIME TESTING**.

### New Runtime Actions
*   `test_activate_ability`:
    *   **Inputs**: `actorPath` (or label), `abilityClass` (or tag).
    *   **Logic**: Find Actor -> Get ASC -> `TryActivateAbilityByClass` / `TryActivateAbilitiesByTag`.
*   `test_apply_effect`:
    *   **Inputs**: `actorPath`, `effectClass`.
    *   **Logic**: Get ASC -> `ApplyGameplayEffectToSelf`.
*   `test_get_attribute`:
    *   **Inputs**: `actorPath`, `attributeSetClass`, `attributeName`.
    *   **Logic**: Get ASC -> Get AttributeSet -> Reflection read of Attribute value (CurrentValue).
*   `test_add_tag` / `test_remove_tag`:
    *   **Inputs**: `actorPath`, `tagName`.
    *   **Logic**: Get ASC -> `AddLooseGameplayTag` / `RemoveLooseGameplayTag`.

### Safety & Constraints
*   **Module Check**: Ensure `GameplayAbilities` module is loaded.
*   **PIE Support**: Use `GetActiveWorld()` to find runtime actors.
*   **Error Handling**: Graceful failure if ASC is missing.

## 4. Execution Steps

1.  **Update C++ Handlers**:
    *   Edit `McpAutomationBridge_GASHandlers.cpp` to add the runtime testing block.
    *   Verify compilation (via `npm run build:core` and visual check).
2.  **Update TS Definitions**:
    *   Edit `src/tools/consolidated-tool-definitions.ts` to add the 4 schemas.
3.  **Update TS Handlers**:
    *   Edit `src/tools/handlers/gas-handlers.ts` to support the new `test_*` actions.
    *   Update `src/tools/consolidated-tool-handlers.ts` registration.
4.  **Verification**:
    *   Run `npm run build:core`.
    *   Run `npm run lint`.
    *   (Optional) Create a test script in `tests/integration.mjs` to verify `manage_gameplay_abilities` (Authoring) works.
