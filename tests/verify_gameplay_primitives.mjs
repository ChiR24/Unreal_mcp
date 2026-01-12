#!/usr/bin/env node
/**
 * Standalone Verification Script for Phase 38: Gameplay Primitives
 *
 * This script runs only the test cases for the Gameplay Primitives tool
 * to verify its implementation end-to-end.
 *
 * Usage:
 *   node tests/verify_gameplay_primitives.mjs
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  // Setup: Create test actor for primitives
  { scenario: 'GameplayPrimitives: Spawn test actor', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Cube', actorName: 'GP_TestActor', location: { x: 0, y: 0, z: 100 } }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Spawn second test actor', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Sphere', actorName: 'GP_TestActor2', location: { x: 200, y: 0, z: 100 } }, expected: 'success' },
  
  // Value Tracker (8 actions)
  { scenario: 'GameplayPrimitives: Create value tracker', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_value_tracker', actorName: 'GP_TestActor', trackerKey: 'Health', initialValue: 100, minValue: 0, maxValue: 100 }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Get value', toolName: 'manage_gameplay_primitives', arguments: { action: 'get_value', actorName: 'GP_TestActor', trackerKey: 'Health' }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Set value', toolName: 'manage_gameplay_primitives', arguments: { action: 'set_value', actorName: 'GP_TestActor', trackerKey: 'Health', value: 75 }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Modify value', toolName: 'manage_gameplay_primitives', arguments: { action: 'modify_value', actorName: 'GP_TestActor', trackerKey: 'Health', delta: -25 }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Add value threshold', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_value_threshold', actorName: 'GP_TestActor', trackerKey: 'Health', thresholdValue: 25, direction: 'falling' }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Configure value decay', toolName: 'manage_gameplay_primitives', arguments: { action: 'configure_value_decay', actorName: 'GP_TestActor', trackerKey: 'Health', decayRate: 1.0, decayInterval: 1.0 }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Configure value regen', toolName: 'manage_gameplay_primitives', arguments: { action: 'configure_value_regen', actorName: 'GP_TestActor', trackerKey: 'Health', regenRate: 2.0, regenInterval: 0.5 }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Pause value changes', toolName: 'manage_gameplay_primitives', arguments: { action: 'pause_value_changes', actorName: 'GP_TestActor', trackerKey: 'Health', paused: true }, expected: 'success' },
  
  // State Machine (6 actions)
  { scenario: 'GameplayPrimitives: Create actor state machine', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_actor_state_machine', actorName: 'GP_TestActor' }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Add actor state', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_actor_state', actorName: 'GP_TestActor', stateName: 'Idle' }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Add second state', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_actor_state', actorName: 'GP_TestActor', stateName: 'Walking' }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Add state transition', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_actor_state_transition', actorName: 'GP_TestActor', fromState: 'Idle', toState: 'Walking' }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Set actor state', toolName: 'manage_gameplay_primitives', arguments: { action: 'set_actor_state', actorName: 'GP_TestActor', stateName: 'Idle' }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Get actor state', toolName: 'manage_gameplay_primitives', arguments: { action: 'get_actor_state', actorName: 'GP_TestActor' }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Configure state timer', toolName: 'manage_gameplay_primitives', arguments: { action: 'configure_state_timer', actorName: 'GP_TestActor', stateName: 'Idle', duration: 5.0, autoTransition: true, targetState: 'Walking' }, expected: 'success' },
  
  // World Time (7 actions)
  { scenario: 'GameplayPrimitives: Create world time', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_world_time', initialHour: 12, initialMinute: 0, dayLengthSeconds: 1200 }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Get world time', toolName: 'manage_gameplay_primitives', arguments: { action: 'get_world_time' }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Set world time', toolName: 'manage_gameplay_primitives', arguments: { action: 'set_world_time', hour: 18, minute: 30 }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Set time scale', toolName: 'manage_gameplay_primitives', arguments: { action: 'set_time_scale', timeScale: 2.0 }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Pause world time', toolName: 'manage_gameplay_primitives', arguments: { action: 'pause_world_time', paused: true }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Add time event', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_time_event', eventId: 'Sunset', hour: 20, minute: 0 }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Get time period', toolName: 'manage_gameplay_primitives', arguments: { action: 'get_time_period' }, expected: 'success' },
  
  // Zone (6 actions)
  { scenario: 'GameplayPrimitives: Create zone', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_zone', zoneId: 'Downtown', zoneName: 'Downtown District', location: { x: 0, y: 0, z: 0 }, extent: { x: 1000, y: 1000, z: 500 } }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Set zone property', toolName: 'manage_gameplay_primitives', arguments: { action: 'set_zone_property', zoneId: 'Downtown', propertyKey: 'dangerLevel', propertyValue: '3' }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Get zone property', toolName: 'manage_gameplay_primitives', arguments: { action: 'get_zone_property', zoneId: 'Downtown', propertyKey: 'dangerLevel' }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Get actor zone', toolName: 'manage_gameplay_primitives', arguments: { action: 'get_actor_zone', actorName: 'GP_TestActor' }, expected: 'success|no zone' },
  { scenario: 'GameplayPrimitives: Add zone enter event', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_zone_enter_event', zoneId: 'Downtown', eventId: 'EnterDowntown' }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Add zone exit event', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_zone_exit_event', zoneId: 'Downtown', eventId: 'ExitDowntown' }, expected: 'success' },
  
  // Faction (8 actions)
  { scenario: 'GameplayPrimitives: Create faction', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_faction', factionId: 'Police', displayName: 'Police Department', color: { r: 0, g: 0, b: 1, a: 1 } }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Create second faction', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_faction', factionId: 'Criminals', displayName: 'Criminal Gang', color: { r: 1, g: 0, b: 0, a: 1 } }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Set faction relationship', toolName: 'manage_gameplay_primitives', arguments: { action: 'set_faction_relationship', factionA: 'Police', factionB: 'Criminals', relationship: -2 }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Assign to faction', toolName: 'manage_gameplay_primitives', arguments: { action: 'assign_to_faction', actorName: 'GP_TestActor', factionId: 'Police' }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Get faction', toolName: 'manage_gameplay_primitives', arguments: { action: 'get_faction', actorName: 'GP_TestActor' }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Modify reputation', toolName: 'manage_gameplay_primitives', arguments: { action: 'modify_reputation', actorName: 'GP_TestActor', factionId: 'Criminals', delta: -50 }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Get reputation', toolName: 'manage_gameplay_primitives', arguments: { action: 'get_reputation', actorName: 'GP_TestActor', factionId: 'Criminals' }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Add reputation threshold', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_reputation_threshold', factionId: 'Criminals', thresholdValue: -100, direction: 'falling', eventId: 'BecomeEnemy' }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Check faction relationship', toolName: 'manage_gameplay_primitives', arguments: { action: 'check_faction_relationship', actorA: 'GP_TestActor', actorB: 'GP_TestActor2' }, expected: 'success|no faction' },
  
  // Condition (4 actions)
  { scenario: 'GameplayPrimitives: Create condition', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_condition', conditionId: 'IsLowHealth', conditionType: 'compare', leftOperand: { type: 'value_tracker', actorName: 'GP_TestActor', trackerKey: 'Health' }, operator: 'less_than', rightOperand: { type: 'constant', value: 25 } }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Create compound condition', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_compound_condition', conditionId: 'IsCritical', compoundType: 'all', childConditions: ['IsLowHealth'] }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Evaluate condition', toolName: 'manage_gameplay_primitives', arguments: { action: 'evaluate_condition', conditionId: 'IsLowHealth' }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Add condition listener', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_condition_listener', conditionId: 'IsLowHealth', eventId: 'OnLowHealth', triggerOnce: false }, expected: 'success' },
  
  // Interaction (6 actions)
  { scenario: 'GameplayPrimitives: Add interactable component', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_interactable_component', actorName: 'GP_TestActor' }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Configure interaction', toolName: 'manage_gameplay_primitives', arguments: { action: 'configure_interaction', actorName: 'GP_TestActor', interactionType: 'Use', interactionPrompt: 'Press E to interact', interactionRange: 200 }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Set interaction enabled', toolName: 'manage_gameplay_primitives', arguments: { action: 'set_interaction_enabled', actorName: 'GP_TestActor', enabled: true }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Get nearby interactables', toolName: 'manage_gameplay_primitives', arguments: { action: 'get_nearby_interactables', location: { x: 0, y: 0, z: 100 }, radius: 500 }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Focus interaction', toolName: 'manage_gameplay_primitives', arguments: { action: 'focus_interaction', actorName: 'GP_TestActor', focusingActorName: 'GP_TestActor2' }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Execute interaction', toolName: 'manage_gameplay_primitives', arguments: { action: 'execute_interaction', actorName: 'GP_TestActor', interactingActorName: 'GP_TestActor2' }, expected: 'success' },
  
  // Schedule (5 actions)
  { scenario: 'GameplayPrimitives: Create schedule', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_schedule', actorName: 'GP_TestActor', scheduleId: 'DailyRoutine' }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Add schedule entry', toolName: 'manage_gameplay_primitives', arguments: { action: 'add_schedule_entry', actorName: 'GP_TestActor', scheduleId: 'DailyRoutine', entryId: 'MorningPatrol', startHour: 8, endHour: 12, action: 'Patrol' }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Set schedule active', toolName: 'manage_gameplay_primitives', arguments: { action: 'set_schedule_active', actorName: 'GP_TestActor', scheduleId: 'DailyRoutine', active: true }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Get current schedule entry', toolName: 'manage_gameplay_primitives', arguments: { action: 'get_current_schedule_entry', actorName: 'GP_TestActor', scheduleId: 'DailyRoutine' }, expected: 'success|no entry' },
  { scenario: 'GameplayPrimitives: Skip to schedule entry', toolName: 'manage_gameplay_primitives', arguments: { action: 'skip_to_schedule_entry', actorName: 'GP_TestActor', scheduleId: 'DailyRoutine', entryId: 'MorningPatrol' }, expected: 'success' },
  
  // Spawner (6 actions)
  { scenario: 'GameplayPrimitives: Create spawner', toolName: 'manage_gameplay_primitives', arguments: { action: 'create_spawner', actorName: 'GP_TestActor', spawnClassPath: '/Engine/BasicShapes/Cone' }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Configure spawner', toolName: 'manage_gameplay_primitives', arguments: { action: 'configure_spawner', actorName: 'GP_TestActor', maxCount: 5, interval: 2.0, radius: 300 }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Set spawner enabled', toolName: 'manage_gameplay_primitives', arguments: { action: 'set_spawner_enabled', actorName: 'GP_TestActor', enabled: true }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Configure spawn conditions', toolName: 'manage_gameplay_primitives', arguments: { action: 'configure_spawn_conditions', actorName: 'GP_TestActor', conditionId: 'IsLowHealth' }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Get spawned count', toolName: 'manage_gameplay_primitives', arguments: { action: 'get_spawned_count', actorName: 'GP_TestActor' }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Despawn managed actors', toolName: 'manage_gameplay_primitives', arguments: { action: 'despawn_managed_actors', actorName: 'GP_TestActor' }, expected: 'success' },
  
  // Attachment (6 actions)
  { scenario: 'GameplayPrimitives: Attach to socket', toolName: 'manage_gameplay_primitives', arguments: { action: 'attach_to_socket', actorName: 'GP_TestActor2', parentActorName: 'GP_TestActor', socketName: '' }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Configure attachment rules', toolName: 'manage_gameplay_primitives', arguments: { action: 'configure_attachment_rules', actorName: 'GP_TestActor2', locationRule: 'KeepRelative', rotationRule: 'KeepRelative', scaleRule: 'KeepWorld' }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Get attached actors', toolName: 'manage_gameplay_primitives', arguments: { action: 'get_attached_actors', actorName: 'GP_TestActor' }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Get attachment parent', toolName: 'manage_gameplay_primitives', arguments: { action: 'get_attachment_parent', actorName: 'GP_TestActor2' }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Transfer control', toolName: 'manage_gameplay_primitives', arguments: { action: 'transfer_control', actorName: 'GP_TestActor2', newParentActorName: 'GP_TestActor' }, expected: 'success' },
  { scenario: 'GameplayPrimitives: Detach from parent', toolName: 'manage_gameplay_primitives', arguments: { action: 'detach_from_parent', actorName: 'GP_TestActor2' }, expected: 'success' },
  
  // Cleanup test actors
  { scenario: 'GameplayPrimitives: Cleanup test actor 1', toolName: 'control_actor', arguments: { action: 'delete', actorName: 'GP_TestActor' }, expected: 'success|not found' },
  { scenario: 'GameplayPrimitives: Cleanup test actor 2', toolName: 'control_actor', arguments: { action: 'delete', actorName: 'GP_TestActor2' }, expected: 'success|not found' },
];

async function main() {
  await runToolTests('manage_gameplay_primitives', testCases);
}

main().catch((err) => {
  console.error('Fatal test error:', err);
  process.exit(1);
});
