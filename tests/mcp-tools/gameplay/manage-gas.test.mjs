#!/usr/bin/env node
/**
 * manage_gas Tool Integration Tests
 * Covers all 27 actions with real captured asset paths.
 */

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const TEST_FOLDER = `/Game/MCPTest/GameplayAssets/ManageGAS_${ts}`;
const blueprintName = `BP_TestGAS_${ts}`;
const attributeSetName = `BP_TestAttributes_${ts}`;
const abilityName = `BP_TestAbility_${ts}`;
const effectName = `BP_TestEffect_${ts}`;
const cueName = `BP_TestCue_${ts}`;
const attributeName = `Health_${ts}`;
const abilityTag = `Ability.Test.${ts}`;
const aliasTag = `Ability.TestAlias.${ts}`;
const effectTag = `Effect.Test.${ts}`;
const cueTag = `GameplayCue.Test.${ts}`;

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success' },
  {
    scenario: 'Setup: create test blueprint',
    toolName: 'manage_blueprint',
    arguments: { action: 'create', name: blueprintName, path: TEST_FOLDER, parentClass: 'Actor' },
    expected: 'success',
    captureResult: { key: 'blueprintPath', fromField: 'result.assetPath' }
  },

  // === COMPONENTS / ATTRIBUTES ===
  { scenario: 'ADD: add_ability_system_component', toolName: 'manage_gas', arguments: { action: 'add_ability_system_component', blueprintPath: '${captured:blueprintPath}', componentName: 'AbilitySystemComponent' }, expected: 'success' },
  { scenario: 'CONFIG: configure_asc', toolName: 'manage_gas', arguments: { action: 'configure_asc', blueprintPath: '${captured:blueprintPath}', componentName: 'AbilitySystemComponent', replicationMode: 'Mixed' }, expected: 'success' },
  {
    scenario: 'CREATE: create_attribute_set',
    toolName: 'manage_gas',
    arguments: { action: 'create_attribute_set', name: attributeSetName, path: TEST_FOLDER },
    expected: 'success',
    captureResult: { key: 'attributeSetPath', fromField: 'result.assetPath' }
  },
  { scenario: 'ADD: add_attribute', toolName: 'manage_gas', arguments: { action: 'add_attribute', attributeSetPath: '${captured:attributeSetPath}', attributeName, attributeType: 'Custom', defaultValue: 100 }, expected: 'success' },
  { scenario: 'CONFIG: set_attribute_base_value', toolName: 'manage_gas', arguments: { action: 'set_attribute_base_value', attributeSetPath: '${captured:attributeSetPath}', attributeName, baseValue: 125 }, expected: 'success' },
  { scenario: 'CONFIG: set_attribute_clamping', toolName: 'manage_gas', arguments: { action: 'set_attribute_clamping', attributeSetPath: '${captured:attributeSetPath}', attributeName, minValue: 0, maxValue: 200, clampMode: 'MinMax' }, expected: 'success' },

  // === GAMEPLAY ABILITY ===
  {
    scenario: 'CREATE: create_gameplay_ability',
    toolName: 'manage_gas',
    arguments: { action: 'create_gameplay_ability', name: abilityName, path: TEST_FOLDER },
    expected: 'success',
    captureResult: { key: 'abilityPath', fromField: 'result.assetPath' }
  },
  // set_ability_tags now VALIDATES tags against the project's GameplayTags registry BEFORE writing
  // anything: an unregistered tag is refused (GAMEPLAY_TAG_NOT_REGISTERED) with zero side effects.
  // Previously the same call answered success while silently writing nothing (the tag strings below
  // are Date.now()-suffixed, so they are never registered) -- the old expectation was asserting the
  // silent-failure bug. This case now asserts the refusal contract instead.
  { scenario: 'CONFIG: set_ability_tags refuses unregistered tags before writing', toolName: 'manage_gas', arguments: { action: 'set_ability_tags', abilityPath: '${captured:abilityPath}', abilityTags: [abilityTag], cancelAbilitiesWithTag: [`Ability.Cancel.${ts}`], blockAbilitiesWithTag: [`Ability.Block.${ts}`], activationRequiredTags: [`Ability.Required.${ts}`], activationBlockedTags: [`Ability.Blocked.${ts}`] }, expected: 'error', assertions: [{ path: 'structuredContent.error', includes: 'GAMEPLAY_TAG_NOT_REGISTERED', label: 'unregistered tags refused, nothing written' }] },
  { scenario: 'CONFIG: set_ability_targeting', toolName: 'manage_gas', arguments: { action: 'set_ability_targeting', abilityPath: '${captured:abilityPath}', targetingMode: 'AOE', targetRange: 1200, aoeRadius: 350 }, expected: 'success' },
  { scenario: 'ADD: add_ability_task', toolName: 'manage_gas', arguments: { action: 'add_ability_task', abilityPath: '${captured:abilityPath}', taskType: 'WaitDelay' }, expected: 'success' },
  { scenario: 'CONFIG: set_activation_policy', toolName: 'manage_gas', arguments: { action: 'set_activation_policy', abilityPath: '${captured:abilityPath}', activationPolicy: 'OnInputPressed' }, expected: 'success' },
  { scenario: 'CONFIG: set_instancing_policy', toolName: 'manage_gas', arguments: { action: 'set_instancing_policy', abilityPath: '${captured:abilityPath}', instancingPolicy: 'InstancedPerExecution' }, expected: 'success' },

  // === GAMEPLAY EFFECT ===
  {
    scenario: 'CREATE: create_gameplay_effect',
    toolName: 'manage_gas',
    arguments: { action: 'create_gameplay_effect', name: effectName, path: TEST_FOLDER, durationType: 'Instant' },
    expected: 'success',
    captureResult: { key: 'effectPath', fromField: 'result.assetPath' },
    assertions: [{ path: 'structuredContent.result.durationType', equals: 'instant', label: 'effect duration type preserved at creation' }]
  },
  { scenario: 'CONFIG: set_ability_costs', toolName: 'manage_gas', arguments: { action: 'set_ability_costs', abilityPath: '${captured:abilityPath}', costEffectPath: '${captured:effectPath}' }, expected: 'success', assertions: [{ path: 'structuredContent.result.costEffectAssigned', equals: true, label: 'cost effect class assigned' }] },
  { scenario: 'CONFIG: set_ability_cooldown', toolName: 'manage_gas', arguments: { action: 'set_ability_cooldown', abilityPath: '${captured:abilityPath}', cooldownEffectPath: '${captured:effectPath}' }, expected: 'success', assertions: [{ path: 'structuredContent.result.cooldownEffectAssigned', equals: true, label: 'cooldown effect class assigned' }] },
  { scenario: 'CONFIG: set_effect_duration', toolName: 'manage_gas', arguments: { action: 'set_effect_duration', effectPath: '${captured:effectPath}', durationType: 'HasDuration', duration: 5, period: 1 }, expected: 'success' },
  { scenario: 'ADD: add_effect_modifier', toolName: 'manage_gas', arguments: { action: 'add_effect_modifier', effectPath: '${captured:effectPath}', attributeName, modifierOperation: 'Add', modifierMagnitude: 25, targetAttribute: attributeName }, expected: 'success' },
  { scenario: 'CONFIG: set_modifier_magnitude', toolName: 'manage_gas', arguments: { action: 'set_modifier_magnitude', effectPath: '${captured:effectPath}', modifierIndex: 0, magnitudeCalculationType: 'SetByCaller', modifierMagnitude: 50, setByCallerTag: `Data.Damage.${ts}` }, expected: 'success' },
  { scenario: 'ADD: add_effect_execution_calculation', toolName: 'manage_gas', arguments: { action: 'add_effect_execution_calculation', effectPath: '${captured:effectPath}', calculationClass: '/Script/GameplayAbilities.GameplayEffectExecutionCalculation' }, expected: 'success' },
  { scenario: 'ADD: add_effect_cue', toolName: 'manage_gas', arguments: { action: 'add_effect_cue', effectPath: '${captured:effectPath}', cueTag }, expected: 'success' },
  { scenario: 'CONFIG: set_effect_stacking', toolName: 'manage_gas', arguments: { action: 'set_effect_stacking', effectPath: '${captured:effectPath}', stackingType: 'AggregateByTarget', stackLimitCount: 3, stackDurationRefreshPolicy: 'RefreshOnSuccessfulApplication', stackPeriodResetPolicy: 'ResetOnSuccessfulApplication', stackExpirationPolicy: 'RemoveSingleStackAndRefreshDuration' }, expected: 'success' },
  { scenario: 'CONFIG: set_effect_tags', toolName: 'manage_gas', arguments: { action: 'set_effect_tags', effectPath: '${captured:effectPath}', grantedTags: [effectTag], applicationRequiredTags: [`Effect.Required.${ts}`], removalTags: [`Effect.Remove.${ts}`], immunityTags: [`Effect.Immune.${ts}`] }, expected: 'success' },

  // === GAMEPLAY CUES / UTILITY ===
  {
    scenario: 'CREATE: create_gameplay_cue_notify',
    toolName: 'manage_gas',
    arguments: { action: 'create_gameplay_cue_notify', name: cueName, path: TEST_FOLDER, cueType: 'Static', cueTag },
    expected: 'success',
    captureResult: { key: 'cuePath', fromField: 'result.assetPath' }
  },
  { scenario: 'CONFIG: configure_cue_trigger', toolName: 'manage_gas', arguments: { action: 'configure_cue_trigger', cuePath: '${captured:cuePath}', triggerType: 'Executed' }, expected: 'success' },
  { scenario: 'CONFIG: set_cue_effects', toolName: 'manage_gas', arguments: { action: 'set_cue_effects', cuePath: '${captured:cuePath}', particleSystemPath: '/Engine/EngineResources/DefaultTexture', soundPath: '/Engine/EngineSounds/Notifications/CompileSuccess_Cue', cameraShakePath: '/Script/Engine.CameraShakeBase', decalPath: '/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial' }, expected: 'success', assertions: [{ path: 'structuredContent.result.decalPath', equals: '/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial', label: 'decal path applied' }, { path: 'structuredContent.result.variableCount', equals: 12, label: 'decal variables added' }] },
  { scenario: 'ADD: add_tag_to_asset', toolName: 'manage_gas', arguments: { action: 'add_tag_to_asset', assetPath: '${captured:abilityPath}', tagName: abilityTag }, expected: 'success' },
  // `tag` is the handler-level fallback for `tagName` (gas-special-actions.ts#handleAddTagToAsset),
  // so it needs its own case: the tagName spelling above never exercises that branch.
  { scenario: 'ADD: add_tag_to_asset via tag alias', toolName: 'manage_gas', arguments: { action: 'add_tag_to_asset', assetPath: '${captured:abilityPath}', tag: aliasTag }, expected: 'success' },
  { scenario: 'INFO: get_gas_info', toolName: 'manage_gas', arguments: { action: 'get_gas_info', assetPath: '${captured:abilityPath}' }, expected: 'success' },
  // params envelope: clients that cannot send arbitrary top-level fields nest them
  // under `params`, which is merged with top-level arguments before routing.
  // get_gas_info requires assetPath, so a success proves the merge reached the handler.
  { scenario: 'INFO: get_gas_info via params envelope', toolName: 'manage_gas', arguments: { action: 'get_gas_info', params: { assetPath: '${captured:abilityPath}' } }, expected: 'success' },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' }
];

runToolTests('manage-gas', testCases);
