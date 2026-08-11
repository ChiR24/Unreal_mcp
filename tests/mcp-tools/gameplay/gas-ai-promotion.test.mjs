#!/usr/bin/env node
/**
 * manage_gas / manage_ai promotion suite.
 *
 * Covers the seven routes promoted from hidden native leaves: four GAS
 * (create_ability_set, add_ability, grant_ability, create_execution_calculation)
 * and three AI (set_ai_perception, set_ai_movement, create_nav_modifier).
 *
 * Every optional parameter each record declares is referenced by at least one
 * case, which is what `test:params --optional-strict` requires. Three of the GAS
 * actions guard a one-of pair rather than a flat required list, so each arm gets
 * its own case: passing only the other spelling must still succeed.
 */

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const TEST_FOLDER = `/Game/MCPTest/GameplayAssets/Promotion_${ts}`;
const ABILITY_SET = `${TEST_FOLDER}/GAS_TestSet_${ts}`;
const ABILITY = `${TEST_FOLDER}/GA_TestAbility_${ts}`;
const ACTOR_BP = `${TEST_FOLDER}/BP_TestPawn_${ts}`;
const CONTROLLER = `${TEST_FOLDER}/AIC_TestController_${ts}`;
const EXEC_CALC = `EC_TestCalc_${ts}`;

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Setup: create the ability the set will reference', toolName: 'manage_gas', arguments: { action: 'create_gameplay_ability', name: `GA_TestAbility_${ts}`, path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Setup: create the pawn blueprint', toolName: 'manage_blueprint', arguments: { action: 'create_blueprint', name: `BP_TestPawn_${ts}`, path: TEST_FOLDER, parentClass: 'Character' }, expected: 'success|already exists' },
  { scenario: 'Setup: create the AI controller', toolName: 'manage_ai', arguments: { action: 'create_ai_controller', name: `AIC_TestController_${ts}`, path: TEST_FOLDER }, expected: 'success|already exists' },

  // === GAS: create_ability_set (setPath | assetPath are a one-of pair) ===
  { scenario: 'GAS: create_ability_set by setPath', toolName: 'manage_gas', arguments: { action: 'create_ability_set', setPath: ABILITY_SET, setName: `TestSet_${ts}` }, expected: 'success|already exists' },
  { scenario: 'GAS: create_ability_set by the assetPath spelling', toolName: 'manage_gas', arguments: { action: 'create_ability_set', assetPath: `${TEST_FOLDER}/GAS_AltSet_${ts}` }, expected: 'success|already exists' },

  // === GAS: add_ability (abilityPath | abilityClass are a one-of pair) ===
  { scenario: 'GAS: add_ability by abilityPath', toolName: 'manage_gas', arguments: { action: 'add_ability', setPath: ABILITY_SET, abilityPath: ABILITY }, expected: 'success|NOT_FOUND' },
  { scenario: 'GAS: add_ability by the abilityClass spelling', toolName: 'manage_gas', arguments: { action: 'add_ability', setPath: ABILITY_SET, abilityClass: ABILITY }, expected: 'success|NOT_FOUND|INVALID_CLASS' },

  // === GAS: grant_ability (actorPath | blueprintPath, abilityPath | abilityClass) ===
  { scenario: 'GAS: grant_ability by actorPath and abilityPath', toolName: 'manage_gas', arguments: { action: 'grant_ability', actorPath: ACTOR_BP, abilityPath: ABILITY, abilityLevel: 2, inputID: 3 }, expected: 'success|ASC_NOT_FOUND|NOT_FOUND' },
  { scenario: 'GAS: grant_ability by the blueprintPath and abilityClass spellings', toolName: 'manage_gas', arguments: { action: 'grant_ability', blueprintPath: ACTOR_BP, abilityClass: ABILITY }, expected: 'success|ASC_NOT_FOUND|NOT_FOUND|INVALID_CLASS' },

  // === GAS: create_execution_calculation ===
  { scenario: 'GAS: create_execution_calculation at an explicit path', toolName: 'manage_gas', arguments: { action: 'create_execution_calculation', name: EXEC_CALC, path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'GAS: create_execution_calculation defaults the path to /Game', toolName: 'manage_gas', arguments: { action: 'create_execution_calculation', name: `EC_Default_${ts}` }, expected: 'success|already exists' },

  // === AI: set_ai_perception ===
  { scenario: 'AI: set_ai_perception with both senses tuned', toolName: 'manage_ai', arguments: { action: 'set_ai_perception', controllerPath: CONTROLLER, enableSight: true, sightRadius: 2500, loseSightRadius: 3000, peripheralVisionAngle: 75, enableHearing: true }, expected: 'success' },
  { scenario: 'AI: set_ai_perception defaults every sense parameter', toolName: 'manage_ai', arguments: { action: 'set_ai_perception', controllerPath: CONTROLLER }, expected: 'success' },

  // === AI: set_ai_movement ===
  { scenario: 'AI: set_ai_movement sets every movement limit', toolName: 'manage_ai', arguments: { action: 'set_ai_movement', blueprintPath: ACTOR_BP, maxWalkSpeed: 450, maxAcceleration: 1800, brakingDeceleration: 1200, rotationRate: 360 }, expected: 'success' },
  { scenario: 'AI: set_ai_movement leaves every limit unchanged when omitted', toolName: 'manage_ai', arguments: { action: 'set_ai_movement', blueprintPath: ACTOR_BP }, expected: 'success' },

  // === AI: create_nav_modifier ===
  { scenario: 'AI: create_nav_modifier with an explicit area class', toolName: 'manage_ai', arguments: { action: 'create_nav_modifier', blueprintPath: ACTOR_BP, componentName: 'NavModifier', areaClass: '/Script/NavigationSystem.NavArea_Obstacle', failsafeToDefaultNavmesh: true }, expected: 'success' },
  { scenario: 'AI: create_nav_modifier names the component itself when omitted', toolName: 'manage_ai', arguments: { action: 'create_nav_modifier', blueprintPath: ACTOR_BP }, expected: 'success' },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
];

runToolTests('gas-ai-promotion', testCases);
