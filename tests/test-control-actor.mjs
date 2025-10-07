#!/usr/bin/env node
/**
 * Actor Control Test Suite
 * Tool: control_actor
 * Actions: spawn, delete, apply_force
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  {
    scenario: 'Spawn StaticMeshActor',
    toolName: 'control_actor',
    arguments: {
      action: 'spawn',
      classPath: 'StaticMeshActor',
      actorName: 'TestCube',
      location: { x: 0, y: 0, z: 100 }
    },
    expected: 'success - actor spawned'
  },
  {
    scenario: 'Spawn actor with mesh asset',
    toolName: 'control_actor',
    arguments: {
      action: 'spawn',
      classPath: '/Engine/BasicShapes/Cube',
      location: { x: 200, y: 0, z: 100 },
      rotation: { pitch: 0, yaw: 45, roll: 0 },
      scale: { x: 1.5, y: 1.5, z: 1.5 }
    },
    expected: 'success - actor spawned'
  },
  {
    scenario: 'Spawn CameraActor',
    toolName: 'control_actor',
    arguments: {
      action: 'spawn',
      classPath: 'CameraActor',
      actorName: 'MainCamera',
      location: { x: -500, y: 0, z: 200 }
    },
    expected: 'success - camera spawned'
  },
  {
    scenario: 'Delete actor by name',
    toolName: 'control_actor',
    arguments: {
      action: 'delete',
      actorName: 'TestCube'
    },
    expected: 'success - actor deleted'
  },
  {
    scenario: 'Delete actor case-insensitive',
    toolName: 'control_actor',
    arguments: {
      action: 'delete',
      actorName: 'maincamera'  // Should find MainCamera
    },
    expected: 'success - actor deleted'
  },
  // Setup: Spawn an actor with physics enabled for force tests
  {
    scenario: 'Spawn PhysicsCube with simulated physics',
    toolName: 'execute_python',
    arguments: {
      script: `
import unreal

world = unreal.EditorLevelLibrary.get_editor_world()
location = unreal.Vector(0, 0, 500)
actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
    unreal.StaticMeshActor.static_class(),
    location,
    unreal.Rotator(0, 0, 0)
)
actor.set_actor_label("PhysicsCube")

# Load and assign mesh
mesh_asset = unreal.EditorAssetLibrary.load_asset('/Engine/BasicShapes/Cube')
if mesh_asset and actor.static_mesh_component:
    actor.static_mesh_component.set_static_mesh(mesh_asset)
    actor.static_mesh_component.set_simulate_physics(True)

print(f"RESULT:{{'success': True, 'actor': '{actor.get_name()}'}}")
`
    },
    expected: 'success - physics actor created'
  },
  {
    scenario: 'Apply physics force',
    toolName: 'control_actor',
    arguments: {
      action: 'apply_force',
      actorName: 'PhysicsCube',
      force: { x: 10000, y: 0, z: 0 }
    },
    expected: 'success - force applied'
  },
  {
    scenario: 'Apply upward force',
    toolName: 'control_actor',
    arguments: {
      action: 'apply_force',
      actorName: 'PhysicsCube',
      force: { x: 0, y: 0, z: 50000 }
    },
    expected: 'success - force applied'
  },
  {
    scenario: 'Spawn LightActor',
    toolName: 'control_actor',
    arguments: {
      action: 'spawn',
      classPath: 'PointLight',
      actorName: 'CustomLight',
      location: { x: 500, y: 500, z: 200 }
    },
    expected: 'success - light spawned'
  },
  {
    scenario: 'Spawn with rotation',
    toolName: 'control_actor',
    arguments: {
      action: 'spawn',
      classPath: 'StaticMeshActor',
      actorName: 'RotatedCube',
      location: { x: 1000, y: 0, z: 0 },
      rotation: { pitch: 0, yaw: 45, roll: 0 }
    },
    expected: 'success - rotated actor spawned'
  },
  // Cleanup
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
    if label in ["PhysicsCube", "CustomLight", "RotatedCube", "Cube_2230"]:
        unreal.EditorLevelLibrary.destroy_actor(actor)
        deleted.append(label)

print(f"RESULT:{{'success': True, 'deleted': {deleted}}}")
`
    },
    expected: 'success - cleanup complete'
  }
];

await runToolTests('Actor Control', testCases);
