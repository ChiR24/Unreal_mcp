#!/usr/bin/env node
/**
 * Python Execution Test Suite
 * Tool: execute_python
 * Single script parameter - no action enum
 */

import { runToolTests } from './test-runner.mjs';

const testCases = [
  {
    scenario: 'Print simple message',
    toolName: 'execute_python',
    arguments: {
      script: 'print("RESULT: {\\"success\\": true, \\"message\\": \\"Hello from Python\\"}")'
    },
    expected: 'success - script executed'
  },
  {
    scenario: 'Get all actors in level',
    toolName: 'execute_python',
    arguments: {
      script: `
import unreal
actors = unreal.EditorLevelLibrary.get_all_level_actors()
print(f"RESULT: {{\\"success\\": true, \\"count\\": {len(actors)}}}")
`
    },
    expected: 'success - actors listed'
  },
  {
    scenario: 'Get current level name',
    toolName: 'execute_python',
    arguments: {
      script: `
import unreal
level = unreal.EditorLevelLibrary.get_editor_world()
level_name = level.get_name() if level else "Unknown"
print(f"RESULT: {{\\"success\\": true, \\"level\\": \\"{level_name}\\"}}")
`
    },
    expected: 'success - level name retrieved'
  },
  {
    scenario: 'Create asset via Python',
    toolName: 'execute_python',
    arguments: {
      script: `
import unreal
factory = unreal.MaterialFactoryNew()
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
material = asset_tools.create_asset("M_PythonMaterial", "/Game/Materials", unreal.Material, factory)
print(f"RESULT: {{\\"success\\": true, \\"path\\": \\"{material.get_path_name() if material else 'Failed'}\\"}}")
`
    },
    expected: 'success - material created'
  },
  {
    scenario: 'Get selected actors',
    toolName: 'execute_python',
    arguments: {
      script: `
import unreal
selected = unreal.EditorLevelLibrary.get_selected_level_actors()
names = [actor.get_actor_label() for actor in selected]
print(f"RESULT: {{\\"success\\": true, \\"count\\": {len(selected)}, \\"names\\": {names}}}")
`
    },
    expected: 'success - selected actors retrieved'
  },
  {
    scenario: 'Get asset registry info',
    toolName: 'execute_python',
    arguments: {
      script: `
import unreal
asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
assets = asset_registry.get_assets_by_path("/Game", recursive=False)
print(f"RESULT: {{\\"success\\": true, \\"count\\": {len(assets)}}}")
`
    },
    expected: 'success - asset registry queried'
  },
  {
    scenario: 'Check editor world exists',
    toolName: 'execute_python',
    arguments: {
      script: `
import unreal
world = unreal.EditorLevelLibrary.get_editor_world()
exists = world is not None
print(f"RESULT: {{\\"success\\": true, \\"worldExists\\": {str(exists).lower()}}}")
`
    },
    expected: 'success - world checked'
  },
  {
    scenario: 'Get engine version',
    toolName: 'execute_python',
    arguments: {
      script: `
import unreal
version = unreal.SystemLibrary.get_engine_version()
print(f"RESULT: {{\\"success\\": true, \\"version\\": \\"{version}\\"}}")
`
    },
    expected: 'success - engine version retrieved'
  },
  {
    scenario: 'Math calculation',
    toolName: 'execute_python',
    arguments: {
      script: `
result = 2 + 2
print(f"RESULT: {{\\"success\\": true, \\"result\\": {result}}}")
`
    },
    expected: 'success - calculation done'
  },
  {
    scenario: 'List content browser paths',
    toolName: 'execute_python',
    arguments: {
      script: `
import unreal
asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
paths = asset_registry.get_all_cached_paths()
print(f"RESULT: {{\\"success\\": true, \\"pathCount\\": {len(paths)}}}")
`
    },
    expected: 'success - paths listed'
  },
  {
    scenario: 'Check project directory',
    toolName: 'execute_python',
    arguments: {
      script: `
import unreal
project_dir = unreal.Paths.project_dir()
print(f"RESULT: {{\\"success\\": true, \\"projectDir\\": \\"{project_dir}\\"}}")
`
    },
    expected: 'success - project directory retrieved'
  }
];

await runToolTests('Python Execution', testCases);
