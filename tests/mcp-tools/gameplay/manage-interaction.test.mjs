#!/usr/bin/env node
/**
 * manage_interaction Tool Integration Tests
 * Covers all 22 actions with real Blueprint paths and spawned actor state.
 */

import { runToolTests } from '../../test-runner.mjs';

const ts = Date.now();
const TEST_FOLDER = `/Game/MCPTest/GameplayInteraction_${ts}`;
const TEST_FOLDER_ALIAS = TEST_FOLDER.slice(1);
const TEST_ACTOR = `TestInteractionActor_${ts}`;
const BLUEPRINT_NAME = `BP_MCP_InteractionActor_${ts}`;
const INTERFACE_NAME = `BPI_MCP_Interactable_${ts}`;
const DOOR_NAME = `BP_MCP_Door_${ts}`;
const SWITCH_NAME = `BP_MCP_Switch_${ts}`;
const CHEST_NAME = `BP_MCP_Chest_${ts}`;
const LEVER_NAME = `BP_MCP_Lever_${ts}`;
const TRIGGER_NAME = `BP_MCP_Trigger_${ts}`;

const EXPECTED_BLUEPRINT_PATH = `${TEST_FOLDER}/${BLUEPRINT_NAME}`;
const EXPECTED_INTERFACE_PATH = `${TEST_FOLDER}/${INTERFACE_NAME}.${INTERFACE_NAME}`;
const EXPECTED_DOOR_PATH = `${TEST_FOLDER}/${DOOR_NAME}`;
const EXPECTED_SWITCH_PATH = `${TEST_FOLDER}/${SWITCH_NAME}.${SWITCH_NAME}`;
const EXPECTED_CHEST_PATH = `${TEST_FOLDER}/${CHEST_NAME}.${CHEST_NAME}`;
const EXPECTED_LEVER_PATH = `${TEST_FOLDER}/${LEVER_NAME}.${LEVER_NAME}`;
const EXPECTED_TRIGGER_PATH = `${TEST_FOLDER}/${TRIGGER_NAME}.${TRIGGER_NAME}`;
const TEST_WIDGET_CLASS = '/Script/UMG.UserWidget';
const TEST_LOOT_TABLE_PATH = `${TEST_FOLDER}/LT_MCP_Test`;

const blueprintPath = '${captured:blueprintPath}';
const doorPath = '${captured:doorPath}';
const switchPath = '${captured:switchPath}';
const chestPath = '${captured:chestPath}';
const triggerPath = '${captured:triggerPath}';

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Setup: spawn destructible target actor', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: TEST_ACTOR, location: { x: 0, y: 0, z: 100 } }, expected: 'success', assertions: [{ path: 'structuredContent.result.actorName', equals: TEST_ACTOR, label: 'destructible target actor spawned' }] },
  {
    scenario: 'Setup: create interaction actor blueprint',
    toolName: 'manage_blueprint',
    arguments: { action: 'create', name: BLUEPRINT_NAME, path: TEST_FOLDER, parentClass: 'Actor' },
    expected: 'success',
    captureResult: { key: 'blueprintPath', fromField: 'result.assetPath' },
    assertions: [{ path: 'structuredContent.result.assetPath', equals: EXPECTED_BLUEPRINT_PATH, label: 'interaction target blueprint path returned' }]
  },

  // === INTERACTION COMPONENT ===
  { scenario: 'CREATE: create_interaction_component', toolName: 'manage_interaction', arguments: { action: 'create_interaction_component', blueprintPath, componentName: 'InteractionComponent', traceDistance: 250 }, expected: 'success', assertions: [{ path: 'structuredContent.result.componentAdded', equals: true, label: 'interaction component added' }, { path: 'structuredContent.result.componentName', equals: 'InteractionComponent', label: 'interaction component name returned' }, { path: 'structuredContent.result.assetPath', equals: EXPECTED_BLUEPRINT_PATH, label: 'interaction component target verified' }] },
  { scenario: 'CONFIG: configure_interaction_trace', toolName: 'manage_interaction', arguments: { action: 'configure_interaction_trace', blueprintPath, traceType: 'sphere', traceDistance: 375, traceRadius: 80 }, expected: 'success', assertions: [{ path: 'structuredContent.result.traceType', equals: 'sphere', label: 'interaction trace type configured' }, { path: 'structuredContent.result.traceDistance', equals: 375, label: 'interaction trace distance configured' }, { path: 'structuredContent.result.traceRadius', equals: 80, label: 'interaction trace radius configured' }, { path: 'structuredContent.result.configured', equals: true, label: 'interaction trace configured flag returned' }] },
  { scenario: 'CONFIG: configure_interaction_widget', toolName: 'manage_interaction', arguments: { action: 'configure_interaction_widget', blueprintPath, widgetClass: TEST_WIDGET_CLASS, showOnHover: true, showPromptText: false, promptTextFormat: 'Use {Key}' }, expected: 'success', assertions: [{ path: 'structuredContent.result.widgetClass', equals: TEST_WIDGET_CLASS, label: 'interaction widget class returned' }, { path: 'structuredContent.result.showOnHover', equals: true, label: 'interaction widget hover enabled' }, { path: 'structuredContent.result.showPromptText', equals: false, label: 'interaction widget prompt flag configured' }, { path: 'structuredContent.result.promptTextFormat', equals: 'Use {Key}', label: 'interaction widget prompt format configured' }, { path: 'structuredContent.result.blueprintPath', equals: EXPECTED_BLUEPRINT_PATH, label: 'interaction widget target returned' }] },
  { scenario: 'ADD: add_interaction_events', toolName: 'manage_interaction', arguments: { action: 'add_interaction_events', blueprintPath }, expected: 'success', assertions: [{ path: 'structuredContent.result.eventsAdded', length: 4, label: 'interaction event dispatchers added' }, { path: 'structuredContent.result.eventCount', equals: 4, label: 'interaction event count returned' }, { path: 'structuredContent.result.blueprintPath', equals: EXPECTED_BLUEPRINT_PATH, label: 'interaction events target returned' }] },

  // === INTERACTABLES ===
  { scenario: 'CREATE: create_interactable_interface', toolName: 'manage_interaction', arguments: { action: 'create_interactable_interface', name: INTERFACE_NAME, folder: TEST_FOLDER_ALIAS }, expected: 'success', assertions: [{ path: 'structuredContent.result.interfacePath', equals: EXPECTED_INTERFACE_PATH, label: 'interactable interface path returned' }, { path: 'structuredContent.result.created', equals: true, label: 'interactable interface created' }, { path: 'structuredContent.result.functionsAdded', length: 3, label: 'interactable interface functions added' }] },
  {
    scenario: 'CREATE: create_door_actor',
    toolName: 'manage_interaction',
    arguments: { action: 'create_door_actor', name: DOOR_NAME, folder: TEST_FOLDER, openAngle: 110, openTime: 0.75, autoClose: true, autoCloseDelay: 2.5, requiresKey: true },
    expected: 'success',
    captureResult: { key: 'doorPath', fromField: 'result.assetPath' },
    assertions: [{ path: 'structuredContent.result.openAngle', equals: 110, label: 'door open angle set at creation' }, { path: 'structuredContent.result.openTime', equals: 0.75, label: 'door open time set at creation' }, { path: 'structuredContent.result.autoClose', equals: true, label: 'door auto-close set at creation' }, { path: 'structuredContent.result.requiresKey', equals: true, label: 'door key requirement set at creation' }, { path: 'structuredContent.result.assetPath', equals: EXPECTED_DOOR_PATH, label: 'door asset path returned' }]
  },
  { scenario: 'CONFIG: configure_door_properties', toolName: 'manage_interaction', arguments: { action: 'configure_door_properties', doorPath, openAngle: 95, openTime: 1.25, locked: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.doorPath', equals: EXPECTED_DOOR_PATH, label: 'door config target returned' }, { path: 'structuredContent.result.openAngle', equals: 95, label: 'door config open angle applied' }, { path: 'structuredContent.result.openTime', equals: 1.25, label: 'door config open time applied' }, { path: 'structuredContent.result.locked', equals: true, label: 'door locked flag applied' }] },
  {
    scenario: 'CREATE: create_switch_actor',
    toolName: 'manage_interaction',
    arguments: { action: 'create_switch_actor', name: SWITCH_NAME, folder: TEST_FOLDER, switchType: 'lever' },
    expected: 'success',
    captureResult: { key: 'switchPath', fromField: 'result.switchPath' },
    assertions: [{ path: 'structuredContent.result.switchPath', equals: EXPECTED_SWITCH_PATH, label: 'switch path returned' }, { path: 'structuredContent.result.switchType', equals: 'lever', label: 'switch type set at creation' }]
  },
  { scenario: 'CONFIG: configure_switch_properties', toolName: 'manage_interaction', arguments: { action: 'configure_switch_properties', switchPath, switchType: 'pressure_plate', canToggle: false, resetTime: 3.5 }, expected: 'success', assertions: [{ path: 'structuredContent.result.switchPath', equals: EXPECTED_SWITCH_PATH, label: 'switch config target returned' }, { path: 'structuredContent.result.switchType', equals: 'pressure_plate', label: 'switch config type applied' }, { path: 'structuredContent.result.canToggle', equals: false, label: 'switch toggle flag applied' }, { path: 'structuredContent.result.resetTime', equals: 3.5, label: 'switch reset time applied' }] },
  {
    scenario: 'CREATE: create_chest_actor',
    toolName: 'manage_interaction',
    arguments: { action: 'create_chest_actor', name: CHEST_NAME, folder: TEST_FOLDER, locked: true },
    expected: 'success',
    captureResult: { key: 'chestPath', fromField: 'result.chestPath' },
    assertions: [{ path: 'structuredContent.result.chestPath', equals: EXPECTED_CHEST_PATH, label: 'chest path returned' }, { path: 'structuredContent.result.locked', equals: true, label: 'chest locked at creation' }]
  },
  { scenario: 'CONFIG: configure_chest_properties', toolName: 'manage_interaction', arguments: { action: 'configure_chest_properties', chestPath, locked: false, openAngle: 75, openTime: 0.6, lootTablePath: TEST_LOOT_TABLE_PATH }, expected: 'success', assertions: [{ path: 'structuredContent.result.chestPath', equals: EXPECTED_CHEST_PATH, label: 'chest config target returned' }, { path: 'structuredContent.result.locked', equals: false, label: 'chest locked flag configured' }, { path: 'structuredContent.result.openAngle', equals: 75, label: 'chest open angle configured' }, { path: 'structuredContent.result.openTime', equals: 0.6, label: 'chest open time configured' }, { path: 'structuredContent.result.lootTablePath', equals: TEST_LOOT_TABLE_PATH, label: 'chest loot table path returned' }] },
  {
    scenario: 'CREATE: create_lever_actor',
    toolName: 'manage_interaction',
    arguments: { action: 'create_lever_actor', name: LEVER_NAME, folder: TEST_FOLDER },
    expected: 'success',
    captureResult: { key: 'leverPath', fromField: 'result.leverPath' },
    assertions: [{ path: 'structuredContent.result.leverPath', equals: EXPECTED_LEVER_PATH, label: 'lever path returned' }, { path: 'structuredContent.result.blueprintPath', equals: EXPECTED_LEVER_PATH, label: 'lever blueprint path returned' }]
  },

  // === DESTRUCTIBLES ===
  { scenario: 'ACTION: setup_destructible_mesh', toolName: 'manage_interaction', arguments: { action: 'setup_destructible_mesh', actorName: TEST_ACTOR }, expected: 'success', assertions: [{ path: 'structuredContent.result.actorName', equals: TEST_ACTOR, label: 'destructible mesh actor target returned' }, { path: 'structuredContent.result.configured', equals: true, label: 'destructible mesh configured flag returned' }, { path: 'structuredContent.result.tagAdded', equals: 'MCP_DestructibleMeshConfigured', label: 'destructible mesh actor tag added' }] },
  { scenario: 'CONFIG: configure_destruction_levels', toolName: 'manage_interaction', arguments: { action: 'configure_destruction_levels', actorName: TEST_ACTOR }, expected: 'success', assertions: [{ path: 'structuredContent.result.actorName', equals: TEST_ACTOR, label: 'destruction levels actor target returned' }, { path: 'structuredContent.result.configured', equals: true, label: 'destruction levels configured flag returned' }, { path: 'structuredContent.result.tagAdded', equals: 'MCP_DestructionLevelsConfigured', label: 'destruction levels actor tag added' }] },
  { scenario: 'CONFIG: configure_destruction_effects', toolName: 'manage_interaction', arguments: { action: 'configure_destruction_effects', actorName: TEST_ACTOR }, expected: 'success', assertions: [{ path: 'structuredContent.result.actorName', equals: TEST_ACTOR, label: 'destruction effects actor target returned' }, { path: 'structuredContent.result.configured', equals: true, label: 'destruction effects configured flag returned' }, { path: 'structuredContent.result.tagAdded', equals: 'MCP_DestructionEffectsConfigured', label: 'destruction effects actor tag added' }] },
  { scenario: 'CONFIG: configure_destruction_damage', toolName: 'manage_interaction', arguments: { action: 'configure_destruction_damage', actorName: TEST_ACTOR }, expected: 'success', assertions: [{ path: 'structuredContent.result.actorName', equals: TEST_ACTOR, label: 'destruction damage actor target returned' }, { path: 'structuredContent.result.configured', equals: true, label: 'destruction damage configured flag returned' }, { path: 'structuredContent.result.tagAdded', equals: 'MCP_DestructionDamageConfigured', label: 'destruction damage actor tag added' }] },
  { scenario: 'ADD: add_destruction_component', toolName: 'manage_interaction', arguments: { action: 'add_destruction_component', blueprintPath, componentName: 'DestructionComponent' }, expected: 'success', assertions: [{ path: 'structuredContent.result.componentAdded', equals: true, label: 'destruction component added' }, { path: 'structuredContent.result.componentName', equals: 'DestructionComponent', label: 'destruction component name returned' }, { path: 'structuredContent.result.variablesAdded', length: 4, label: 'destruction variables added' }] },

  // === TRIGGERS ===
  {
    scenario: 'CREATE: create_trigger_actor',
    toolName: 'manage_interaction',
    arguments: { action: 'create_trigger_actor', name: TRIGGER_NAME, folder: TEST_FOLDER, triggerShape: 'sphere' },
    expected: 'success',
    captureResult: { key: 'triggerPath', fromField: 'result.triggerPath' },
    assertions: [{ path: 'structuredContent.result.triggerPath', equals: EXPECTED_TRIGGER_PATH, label: 'trigger path returned' }, { path: 'structuredContent.result.triggerShape', equals: 'sphere', label: 'trigger shape set at creation' }]
  },
  { scenario: 'CONFIG: configure_trigger_events', toolName: 'manage_interaction', arguments: { action: 'configure_trigger_events', triggerPath }, expected: 'success', assertions: [{ path: 'structuredContent.result.configured', equals: true, label: 'trigger events configured flag returned' }, { path: 'structuredContent.result.eventCount', equals: 3, label: 'trigger event dispatchers added' }, { path: 'structuredContent.result.eventsAdded', length: 3, label: 'trigger event names returned' }] },
  { scenario: 'CONFIG: configure_trigger_filter', toolName: 'manage_interaction', arguments: { action: 'configure_trigger_filter', triggerPath }, expected: 'success', assertions: [{ path: 'structuredContent.result.triggerPath', equals: EXPECTED_TRIGGER_PATH, label: 'trigger filter target returned' }, { path: 'structuredContent.result.configured', equals: true, label: 'trigger filter configured flag returned' }, { path: 'structuredContent.result.variablesAdded', length: 2, label: 'trigger filter variables added' }] },
  { scenario: 'CONFIG: configure_trigger_response', toolName: 'manage_interaction', arguments: { action: 'configure_trigger_response', triggerPath }, expected: 'success', assertions: [{ path: 'structuredContent.result.triggerPath', equals: EXPECTED_TRIGGER_PATH, label: 'trigger response target returned' }, { path: 'structuredContent.result.configured', equals: true, label: 'trigger response configured flag returned' }, { path: 'structuredContent.result.variablesAdded', length: 2, label: 'trigger response variables added' }] },

  // === INFO ===
  { scenario: 'INFO: get_interaction_info blueprint', toolName: 'manage_interaction', arguments: { action: 'get_interaction_info', blueprintPath }, expected: 'success', assertions: [{ path: 'structuredContent.result.assetType', equals: 'Blueprint', label: 'interaction info reports blueprint asset type' }, { path: 'structuredContent.result.blueprintPath', equals: EXPECTED_BLUEPRINT_PATH, label: 'interaction info returns blueprint path' }, { path: 'structuredContent.result.blueprintName', equals: BLUEPRINT_NAME, label: 'interaction info returns blueprint name' }] },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete test actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: TEST_ACTOR }, expected: 'success|not found' },
  { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
];

runToolTests('manage-interaction', testCases);
