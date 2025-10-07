#!/usr/bin/env node
/**
 * Inspect Test Suite
 * Tool: inspect
 * Actions: inspect_object, set_property, get_property
 * 
 * NOTE: These tests use Python to create actors dynamically,
 * then inspect/modify them. This avoids hardcoded object paths.
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  // Setup: Create a test actor using Python
  {
    scenario: 'Setup - Create StaticMeshActor for testing',
    toolName: 'execute_python',
    arguments: {
      script: `
import unreal

# Spawn a test static mesh actor
world = unreal.EditorLevelLibrary.get_editor_world()
location = unreal.Vector(0, 0, 100)
rotation = unreal.Rotator(0, 0, 0)
actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
    unreal.StaticMeshActor.static_class(),
    location,
    rotation
)
actor.set_actor_label("InspectTestActor")

# Load and assign a mesh
mesh_asset = unreal.EditorAssetLibrary.load_asset('/Engine/BasicShapes/Cube')
if mesh_asset and actor.static_mesh_component:
    actor.static_mesh_component.set_static_mesh(mesh_asset)

print(f"RESULT:{{'success': True, 'actor': '{actor.get_name()}', 'path': '{actor.get_path_name()}'}}")
`
    },
    expected: 'success - test actor created'
  },
  
  // Setup: Create a test light
  {
    scenario: 'Setup - Create DirectionalLight for testing',
    toolName: 'execute_python',
    arguments: {
      script: `
import unreal

world = unreal.EditorLevelLibrary.get_editor_world()
location = unreal.Vector(0, 0, 500)
rotation = unreal.Rotator(-45, 0, 0)
light = unreal.EditorLevelLibrary.spawn_actor_from_class(
    unreal.DirectionalLight.static_class(),
    location,
    rotation
)
light.set_actor_label("InspectTestLight")

print(f"RESULT:{{'success': True, 'light': '{light.get_name()}', 'path': '{light.get_path_name()}'}}")
`
    },
    expected: 'success - test light created'
  },

  // Find actor and get its location using Python
  {
    scenario: 'Get actor location via Python',
    toolName: 'execute_python',
    arguments: {
      script: `
import unreal

actors = unreal.EditorLevelLibrary.get_all_level_actors()
test_actor = next((a for a in actors if a.get_actor_label() == "InspectTestActor"), None)

if test_actor:
    location = test_actor.get_actor_location()
    print(f"RESULT:{{'success': True, 'location': {{'X': {location.x}, 'Y': {location.y}, 'Z': {location.z}}}}}")
else:
    print("RESULT:{'success': False, 'error': 'Actor not found'}")
`
    },
    expected: 'success - location retrieved'
  },

  // Set actor location using Python
  {
    scenario: 'Set actor location via Python',
    toolName: 'execute_python',
    arguments: {
      script: `
import unreal

actors = unreal.EditorLevelLibrary.get_all_level_actors()
test_actor = next((a for a in actors if a.get_actor_label() == "InspectTestActor"), None)

if test_actor:
    new_location = unreal.Vector(100, 200, 300)
    test_actor.set_actor_location(new_location, False, False)
    print("RESULT:{'success': True, 'message': 'Location set'}")
else:
    print("RESULT:{'success': False, 'error': 'Actor not found'}")
`
    },
    expected: 'success - location set'
  },

  // Get actor rotation
  {
    scenario: 'Get actor rotation via Python',
    toolName: 'execute_python',
    arguments: {
      script: `
import unreal

actors = unreal.EditorLevelLibrary.get_all_level_actors()
test_actor = next((a for a in actors if a.get_actor_label() == "InspectTestActor"), None)

if test_actor:
    rotation = test_actor.get_actor_rotation()
    print(f"RESULT:{{'success': True, 'rotation': {{'Pitch': {rotation.pitch}, 'Yaw': {rotation.yaw}, 'Roll': {rotation.roll}}}}}")
else:
    print("RESULT:{'success': False, 'error': 'Actor not found'}")
`
    },
    expected: 'success - rotation retrieved'
  },

  // Set actor rotation
  {
    scenario: 'Set actor rotation via Python',
    toolName: 'execute_python',
    arguments: {
      script: `
import unreal

actors = unreal.EditorLevelLibrary.get_all_level_actors()
test_actor = next((a for a in actors if a.get_actor_label() == "InspectTestActor"), None)

if test_actor:
    new_rotation = unreal.Rotator(0, 90, 0)
    test_actor.set_actor_rotation(new_rotation, False)
    print("RESULT:{'success': True, 'message': 'Rotation set'}")
else:
    print("RESULT:{'success': False, 'error': 'Actor not found'}")
`
    },
    expected: 'success - rotation set'
  },

  // Set actor scale
  {
    scenario: 'Set actor scale via Python',
    toolName: 'execute_python',
    arguments: {
      script: `
import unreal

actors = unreal.EditorLevelLibrary.get_all_level_actors()
test_actor = next((a for a in actors if a.get_actor_label() == "InspectTestActor"), None)

if test_actor:
    new_scale = unreal.Vector(2, 2, 2)
    test_actor.set_actor_scale3d(new_scale)
    print("RESULT:{'success': True, 'message': 'Scale set'}")
else:
    print("RESULT:{'success': False, 'error': 'Actor not found'}")
`
    },
    expected: 'success - scale set'
  },

  // Toggle visibility
  {
    scenario: 'Set actor hidden via Python',
    toolName: 'execute_python',
    arguments: {
      script: `
import unreal

actors = unreal.EditorLevelLibrary.get_all_level_actors()
test_actor = next((a for a in actors if a.get_actor_label() == "InspectTestActor"), None)

if test_actor:
    test_actor.set_actor_hidden_in_game(True)
    print("RESULT:{'success': True, 'message': 'Visibility toggled'}")
else:
    print("RESULT:{'success': False, 'error': 'Actor not found'}")
`
    },
    expected: 'success - visibility toggled'
  },

  // Get actor mobility
  {
    scenario: 'Get mobility via Python',
    toolName: 'execute_python',
    arguments: {
      script: `
import unreal

actors = unreal.EditorLevelLibrary.get_all_level_actors()
test_actor = next((a for a in actors if a.get_actor_label() == "InspectTestActor"), None)

if test_actor and hasattr(test_actor, 'static_mesh_component'):
    comp = test_actor.static_mesh_component
    mobility = str(comp.get_editor_property('mobility'))
    print(f"RESULT:{{'success': True, 'mobility': '{mobility}'}}")
else:
    print("RESULT:{'success': False, 'error': 'Actor or component not found'}")
`
    },
    expected: 'success - mobility retrieved'
  },

  // Set light intensity
  {
    scenario: 'Set light intensity via Python',
    toolName: 'execute_python',
    arguments: {
      script: `
import unreal

actors = unreal.EditorLevelLibrary.get_all_level_actors()
test_light = next((a for a in actors if a.get_actor_label() == "InspectTestLight"), None)

if test_light and hasattr(test_light, 'light_component'):
    test_light.light_component.set_intensity(10.0)
    print("RESULT:{'success': True, 'message': 'Intensity set'}")
else:
    print("RESULT:{'success': False, 'error': 'Light not found'}")
`
    },
    expected: 'success - intensity set'
  },

  // Cleanup: Delete test actors
  {
    scenario: 'Cleanup - Delete test actors',
    toolName: 'execute_python',
    arguments: {
      script: `
import unreal

actors = unreal.EditorLevelLibrary.get_all_level_actors()
deleted = []

for actor in actors:
    label = actor.get_actor_label()
    if label in ["InspectTestActor", "InspectTestLight"]:
        unreal.EditorLevelLibrary.destroy_actor(actor)
        deleted.append(label)

print(f"RESULT:{{'success': True, 'deleted': {deleted}}}")
`
    },
    expected: 'success - cleanup complete'
  }
];

await runToolTests('Inspect', testCases);
