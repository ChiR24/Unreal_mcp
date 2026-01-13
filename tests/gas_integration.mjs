#!/usr/bin/env node
import { runToolTests } from './test-runner.mjs';

const TEST_FOLDER = '/Game/IntegrationTest/GAS';

const testCases = [
  // Setup
  { scenario: 'GAS: Create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
  
  // 1. Create Ability System Component Blueprint (Actor)
  { scenario: 'GAS: Create Actor with ASC', toolName: 'manage_blueprint', arguments: { action: 'create', name: 'BP_TestGASActor', path: TEST_FOLDER, parentClass: 'Actor' }, expected: 'success|already exists' },
  { scenario: 'GAS: Add ASC to Actor', toolName: 'manage_attribute_sets', arguments: { action: 'add_ability_system_component', blueprintPath: `${TEST_FOLDER}/BP_TestGASActor` }, expected: 'success' },
  
  // 2. Create AttributeSet
  { scenario: 'GAS: Create AttributeSet', toolName: 'manage_attribute_sets', arguments: { action: 'create_attribute_set', name: 'AS_TestAttributes', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'GAS: Add Health Attribute', toolName: 'manage_attribute_sets', arguments: { action: 'add_attribute', blueprintPath: `${TEST_FOLDER}/AS_TestAttributes`, attributeName: 'Health', defaultValue: 100 }, expected: 'success' },
  
  // 3. Create Gameplay Ability
  { scenario: 'GAS: Create Gameplay Ability', toolName: 'manage_gameplay_abilities', arguments: { action: 'create_gameplay_ability', name: 'GA_TestAbility', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'GAS: Add Tag to Ability', toolName: 'manage_gameplay_abilities', arguments: { action: 'add_tag_to_asset', assetPath: `${TEST_FOLDER}/GA_TestAbility`, tagName: 'Ability.Test' }, expected: 'success' },
  
  // 4. Create Gameplay Effect
  { scenario: 'GAS: Create Gameplay Effect', toolName: 'manage_gameplay_abilities', arguments: { action: 'create_gameplay_effect', name: 'GE_TestDamage', path: TEST_FOLDER, durationType: 'instant' }, expected: 'success|already exists' },
  { scenario: 'GAS: Add Damage Modifier', toolName: 'manage_gameplay_abilities', arguments: { action: 'add_effect_modifier', effectPath: `${TEST_FOLDER}/GE_TestDamage`, attributeName: 'Health', operation: 'add', magnitude: -10 }, expected: 'success' },
  
  // 5. Spawn Actor for Runtime Testing
  { scenario: 'GAS: Spawn Test Actor', toolName: 'control_actor', arguments: { action: 'spawn', blueprintPath: `${TEST_FOLDER}/BP_TestGASActor`, actorName: 'GAS_TestActor', location: { x: 0, y: 0, z: 100 } }, expected: 'success' },
  
  // 6. Runtime Testing
  { scenario: 'GAS: Test Get Tags', toolName: 'test_gameplay_abilities', arguments: { action: 'test_get_gameplay_tags', actorName: 'GAS_TestActor' }, expected: 'success' },
  
  // Expect success (tool execution works), but capability might fail if ability not granted
  { scenario: 'GAS: Test Activate Ability', toolName: 'test_gameplay_abilities', arguments: { action: 'test_activate_ability', actorName: 'GAS_TestActor', abilityClass: `${TEST_FOLDER}/GA_TestAbility` }, expected: 'success' },
  
  // Apply Effect
  { scenario: 'GAS: Test Apply Effect', toolName: 'test_gameplay_abilities', arguments: { action: 'test_apply_effect', actorName: 'GAS_TestActor', effectClass: `${TEST_FOLDER}/GE_TestDamage` }, expected: 'success' },
  
  // Get Attribute (Might fail to find if not initialized, but tool should handle it)
  { scenario: 'GAS: Test Get Attribute', toolName: 'test_gameplay_abilities', arguments: { action: 'test_get_attribute', actorName: 'GAS_TestActor', attributeName: 'Health' }, expected: 'success|not found' },
  
  // Cleanup
  { scenario: 'GAS: Cleanup Actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: 'GAS_TestActor' }, expected: 'success' }
];

runToolTests(testCases).catch(error => {
  console.error('Test suite failed:', error);
  process.exit(1);
});
