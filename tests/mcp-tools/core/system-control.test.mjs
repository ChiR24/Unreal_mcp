#!/usr/bin/env node
/**
 * system_control Tool Integration Tests
 * Covers all 24 actions with proper setup/teardown sequencing.
 */

import { runToolTests } from '../../test-runner.mjs';

const TEST_FOLDER = '/Game/MCPTest/SystemControl';
const WIDGET_NAME = 'WBP_SystemControl_Test';
const WIDGET_PATH = `${TEST_FOLDER}/${WIDGET_NAME}`;
const VALIDATION_MATERIAL = `${TEST_FOLDER}/M_SystemControlValidation`;
const PYTHON_TEST_ID = Date.now();
const PYTHON_FILE_RELATIVE = `Saved/MCPTests/system-control-${PYTHON_TEST_ID}.py`;
const PYTHON_HELPER_RELATIVE = `Saved/MCPTests/system-control-${PYTHON_TEST_ID}-helper.txt`;
const PYTHON_FILE_LITERAL = JSON.stringify(PYTHON_FILE_RELATIVE);
const PYTHON_HELPER_LITERAL = JSON.stringify(PYTHON_HELPER_RELATIVE);
const PROJECT_SETTING_SECTION = '/Script/Engine.Engine';
const PROJECT_SETTING_KEY = `McpSystemControlSmoke_${Date.now()}`;
const PROJECT_SETTING_SECTION_LITERAL = JSON.stringify(PROJECT_SETTING_SECTION);
const PROJECT_SETTING_KEY_LITERAL = JSON.stringify(PROJECT_SETTING_KEY);
const CREATE_PYTHON_FILE_CODE = `
import os
import unreal
path = os.path.join(unreal.Paths.project_dir(), ${PYTHON_FILE_LITERAL})
helper_path = os.path.join(unreal.Paths.project_dir(), ${PYTHON_HELPER_LITERAL})
os.makedirs(os.path.dirname(path), exist_ok=True)
with open(helper_path, 'w', encoding='utf-8') as f:
    f.write('sibling-file-ok')
with open(path, 'w', encoding='utf-8') as f:
    f.write('import os\\n')
    f.write('helper_path = os.path.join(os.path.dirname(__file__), os.path.basename(${PYTHON_HELPER_LITERAL}))\\n')
    f.write('with open(helper_path, "r", encoding="utf-8") as helper:\\n')
    f.write('    print("system-control-file-ok:" + helper.read())\\n')
print("system-control-file-created")
`.trim();
const DELETE_PYTHON_FILE_CODE = `
import os
import unreal
path = os.path.join(unreal.Paths.project_dir(), ${PYTHON_FILE_LITERAL})
helper_path = os.path.join(unreal.Paths.project_dir(), ${PYTHON_HELPER_LITERAL})
if os.path.exists(path):
    os.remove(path)
if os.path.exists(helper_path):
    os.remove(helper_path)
print("system-control-file-cleaned")
`.trim();
const CLEANUP_PROJECT_SETTING_CODE = `
import os
import unreal
section = ${PROJECT_SETTING_SECTION_LITERAL}
key = ${PROJECT_SETTING_KEY_LITERAL}
config_path = os.path.join(unreal.Paths.project_config_dir(), 'DefaultEngine.ini')
if os.path.exists(config_path):
    with open(config_path, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    kept = []
    in_target_section = False
    section_header = f'[{section}]'
    for line in lines:
        stripped = line.strip()
        if stripped.startswith('[') and stripped.endswith(']'):
            in_target_section = stripped == section_header
        if in_target_section and stripped.split('=', 1)[0].strip() == key:
            continue
        kept.append(line)
    with open(config_path, 'w', encoding='utf-8') as f:
        f.writelines(kept)
print("system-control-setting-cleaned")
`.trim();

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Setup: create validation material', toolName: 'manage_asset', arguments: { action: 'create_material', name: 'M_SystemControlValidation', path: TEST_FOLDER }, expected: 'success|already exists' },

  // === ACTION ===
  { scenario: 'ACTION: profile', toolName: 'system_control', arguments: { action: 'profile', profileType: 'cpu' }, expected: 'success' },
  { scenario: 'ACTION: show_fps', toolName: 'system_control', arguments: { action: 'show_fps', enabled: true }, expected: 'success' },
  // === CONFIG ===
  { scenario: 'CONFIG: set_quality', toolName: 'system_control', arguments: { action: 'set_quality', category: 'ViewDistance', level: 1 }, expected: 'success' },
  // === ACTION ===
  { scenario: 'ACTION: screenshot', toolName: 'system_control', arguments: { action: 'screenshot', filename: 'SystemControl_NullRHI', resolution: '640x360', mode: 'editor_viewport', returnBase64: false, includeMetadata: true, metadata: { source: 'system-control-suite' } }, expected: 'success' },
  // === CONFIG ===
  { scenario: 'CONFIG: set_resolution', toolName: 'system_control', arguments: { action: 'set_resolution', width: 1280, height: 720, windowed: true }, expected: 'success' },
  { scenario: 'CONFIG: set_fullscreen', toolName: 'system_control', arguments: { action: 'set_fullscreen', enabled: false }, expected: 'success' },
  // === ACTION ===
  { scenario: 'ACTION: execute_command', toolName: 'system_control', arguments: { action: 'execute_command', command: 'stat unit' }, expected: 'success' },
  { scenario: 'ACTION: console_command', toolName: 'system_control', arguments: { action: 'console_command', command: 'stat fps' }, expected: 'success' },
  { scenario: 'ACTION: run_ubt', toolName: 'system_control', arguments: { action: 'run_ubt', target: 'MCPtestEditor', platform: 'Linux', configuration: 'Development', arguments: '-NoHotReload' }, expected: 'success' },
  { scenario: 'ACTION: subscribe', toolName: 'system_control', arguments: { action: 'subscribe' }, expected: 'success' },
  { scenario: 'ACTION: unsubscribe', toolName: 'system_control', arguments: { action: 'unsubscribe' }, expected: 'success' },
  // === CREATE ===
  { scenario: 'CREATE: spawn_category', toolName: 'system_control', arguments: { action: 'spawn_category', categoryName: 'AI' }, expected: 'success' },
  // === ACTION ===
  { scenario: 'ACTION: start_session', toolName: 'system_control', arguments: { action: 'start_session', channels: 'cpu' }, expected: 'success' },
  { scenario: 'ACTION: lumen_update_scene', toolName: 'system_control', arguments: { action: 'lumen_update_scene' }, expected: 'success' },
  // === PLAYBACK ===
  { scenario: 'PLAYBACK: play_sound', toolName: 'system_control', arguments: { action: 'play_sound', volume: 0 }, expected: 'success' },
  // === CREATE ===
  { scenario: 'CREATE: create_widget', toolName: 'system_control', arguments: { action: 'create_widget', name: WIDGET_NAME, savePath: TEST_FOLDER }, expected: 'success|already exists' },
  // === ACTION ===
  { scenario: 'ACTION: show_widget', toolName: 'system_control', arguments: { action: 'show_widget', widgetId: 'notification', message: 'System control smoke', duration: 0.1 }, expected: 'success' },
  // === ADD ===
  { scenario: 'ADD: add_widget_child', toolName: 'system_control', arguments: { action: 'add_widget_child', widgetPath: WIDGET_PATH, childClass: 'TextBlock', name: 'SystemControlText', text: 'System control child' }, expected: 'success' },
  { scenario: 'ADD: add_widget_child parentName', toolName: 'system_control', arguments: { action: 'add_widget_child', widgetPath: WIDGET_PATH, childClass: 'TextBlock', name: 'SystemControlNestedText', parentName: 'RootCanvas', text: 'System control nested child' }, expected: 'success' },
  // === CONFIG ===
  { scenario: 'CONFIG: set_cvar', toolName: 'system_control', arguments: { action: 'set_cvar', name: 'r.ScreenPercentage', value: '100' }, expected: 'success' },
  // === INFO ===
  { scenario: 'INFO: get_project_settings', toolName: 'system_control', arguments: { action: 'get_project_settings', section: '/Script/Engine.Engine' }, expected: 'success' },
  // === ACTION ===
  { scenario: 'ACTION: validate_assets', toolName: 'system_control', arguments: { action: 'validate_assets', paths: [VALIDATION_MATERIAL] }, expected: 'success' },
  { scenario: 'ACTION: validate_assets assetPath', toolName: 'system_control', arguments: { action: 'validate_assets', assetPath: VALIDATION_MATERIAL }, expected: 'success' },
  { scenario: 'ACTION: validate_assets path recursive', toolName: 'system_control', arguments: { action: 'validate_assets', path: TEST_FOLDER, recursive: false }, expected: 'success' },
  // === CONFIG ===
  { scenario: 'CONFIG: set_project_setting', toolName: 'system_control', arguments: { action: 'set_project_setting', section: PROJECT_SETTING_SECTION, key: PROJECT_SETTING_KEY, value: '1' }, expected: 'success' },
  { scenario: 'ACTION: execute_python', toolName: 'system_control', arguments: { action: 'execute_python', code: 'print("system-control-ok")' }, expected: 'success' },
  { scenario: 'Setup: create execute_python file', toolName: 'system_control', arguments: { action: 'execute_python', code: CREATE_PYTHON_FILE_CODE }, expected: 'success' },
  // This result is asserted on the returned MCP response, so it also catches
  // temp wrapper cleanup racing file execution before output/status are written.
  { scenario: 'ACTION: execute_python file', toolName: 'system_control', arguments: { action: 'execute_python', file: PYTHON_FILE_RELATIVE }, expected: 'success', assertions: [{ path: 'structuredContent.result.output', equals: 'system-control-file-ok:sibling-file-ok', label: 'python file has __file__ and synchronous output' }] },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete execute_python file', toolName: 'system_control', arguments: { action: 'execute_python', code: DELETE_PYTHON_FILE_CODE }, expected: 'success' },
  { scenario: 'Cleanup: remove project setting', toolName: 'system_control', arguments: { action: 'execute_python', code: CLEANUP_PROJECT_SETTING_CODE }, expected: 'success' },
  { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
  { scenario: 'ACTION: run_tests', toolName: 'system_control', arguments: { action: 'run_tests', filter: 'System.Core.Time.Comparison' }, expected: 'success' },
];

// === PERFORMANCE ACTIONS ===
{
  /**
   * system_control performance action integration tests
   * Covers all 20 actions with proper setup/teardown sequencing.
   */

  const ts = Date.now();
  const TEST_FOLDER = `/Game/MCPTest/UtilityAssets_${ts}`;
  const MERGE_PARENT_ACTOR = `ParentActor_${ts}`;
  const MERGE_CHILD_ACTOR = `ChildActor_${ts}`;
  const MERGED_ACTOR_ASSET = `${TEST_FOLDER}/SM_PerformanceMerged_${ts}`;
  const MERGED_ACTOR_PACKAGE_ASSET = `${TEST_FOLDER}/SM_PerformanceMergedPackage_${ts}`;

  testCases.push(
    // === SETUP ===
    { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
    { scenario: 'Setup: spawn merge parent actor', toolName: 'control_actor', arguments: { action: 'spawn_actor', classPath: '/Script/Engine.StaticMeshActor', meshPath: '/Engine/BasicShapes/Cube.Cube', actorName: MERGE_PARENT_ACTOR, location: { x: 0, y: 300, z: 120 } }, expected: 'success|already exists' },
    { scenario: 'Setup: spawn merge child actor', toolName: 'control_actor', arguments: { action: 'spawn_actor', classPath: '/Script/Engine.StaticMeshActor', meshPath: '/Engine/BasicShapes/Cube.Cube', actorName: MERGE_CHILD_ACTOR, location: { x: 160, y: 300, z: 120 } }, expected: 'success|already exists' },

    // === ACTION ===
    { scenario: 'ACTION: start_profiling', toolName: 'system_control', arguments: {"action": "start_profiling", "type": "CPU", "duration": 1}, expected: 'success' },
    // === PLAYBACK ===
    { scenario: 'PLAYBACK: stop_profiling', toolName: 'system_control', arguments: {"action": "stop_profiling"}, expected: 'success' },
    // === ACTION ===
    { scenario: 'ACTION: run_benchmark', toolName: 'system_control', arguments: {"action": "run_benchmark", "duration": 1, "type": "CPU"}, expected: 'success' },
    { scenario: 'ACTION: show_fps', toolName: 'system_control', arguments: {"action": "show_fps"}, expected: 'success' },
    { scenario: 'ACTION: show_stats', toolName: 'system_control', arguments: {"action": "show_stats", "category": "Unit"}, expected: 'success' },
    { scenario: 'ACTION: generate_memory_report', toolName: 'system_control', arguments: {"action": "generate_memory_report", "detailed": true}, expected: 'success|already exists' },
    // === CONFIG ===
    { scenario: 'CONFIG: set_scalability', toolName: 'system_control', arguments: {"action": "set_scalability", "level": 1, "category": "ViewDistance"}, expected: 'success' },
    { scenario: 'CONFIG: set_resolution_scale', toolName: 'system_control', arguments: {"action": "set_resolution_scale", "scale": 75}, expected: 'success' },
    { scenario: 'CONFIG: set_vsync', toolName: 'system_control', arguments: {"action": "set_vsync", "enabled": true}, expected: 'success' },
    { scenario: 'CONFIG: set_frame_rate_limit', toolName: 'system_control', arguments: {"action": "set_frame_rate_limit", "maxFPS": 60}, expected: 'success' },
    // === TOGGLE ===
    { scenario: 'TOGGLE: enable_gpu_timing', toolName: 'system_control', arguments: {"action": "enable_gpu_timing"}, expected: 'success' },
    // === CONFIG ===
    { scenario: 'CONFIG: configure_texture_streaming', toolName: 'system_control', arguments: {"action": "configure_texture_streaming", "poolSize": 128, "boostPlayerLocation": false}, expected: 'success' },
    { scenario: 'CONFIG: configure_lod', toolName: 'system_control', arguments: {"action": "configure_lod", "forceLOD": -1, "lodBias": 0}, expected: 'success' },
    // === ACTION ===
    { scenario: 'ACTION: apply_baseline_settings', toolName: 'system_control', arguments: {"action": "apply_baseline_settings"}, expected: 'success' },
    { scenario: 'ACTION: optimize_draw_calls', toolName: 'system_control', arguments: {"action": "optimize_draw_calls", "enableInstancing": false, "enableBatching": true}, expected: 'success' },
    { scenario: 'ACTION: merge_actors', toolName: 'system_control', arguments: {"action": "merge_actors", "actors": [MERGE_PARENT_ACTOR, MERGE_CHILD_ACTOR], "replaceSourceActors": false, "mergeActors": true, "outputPath": MERGED_ACTOR_ASSET}, expected: 'success' },
    { scenario: 'ACTION: merge_actors via packageName', toolName: 'system_control', arguments: {"action": "merge_actors", "actors": [MERGE_PARENT_ACTOR, MERGE_CHILD_ACTOR], "replaceSourceActors": false, "packageName": MERGED_ACTOR_PACKAGE_ASSET}, expected: 'success' },
    // === CONFIG ===
    { scenario: 'CONFIG: configure_occlusion_culling', toolName: 'system_control', arguments: {"action": "configure_occlusion_culling"}, expected: 'success' },
    // === ACTION ===
    { scenario: 'ACTION: optimize_shaders', toolName: 'system_control', arguments: {"action": "optimize_shaders"}, expected: 'success' },
    // === CONFIG ===
    { scenario: 'CONFIG: configure_nanite', toolName: 'system_control', arguments: {"action": "configure_nanite"}, expected: 'success' },
    { scenario: 'CONFIG: configure_world_partition', toolName: 'system_control', arguments: {"action": "configure_world_partition", "cellSize": 6400, "streamingDistance": 25600}, expected: 'success' },

    // === CLEANUP ===
    { scenario: 'Cleanup: delete merged mesh asset', toolName: 'manage_asset', arguments: { action: 'delete', path: MERGED_ACTOR_ASSET, force: true }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete packageName merged mesh asset', toolName: 'manage_asset', arguments: { action: 'delete', path: MERGED_ACTOR_PACKAGE_ASSET, force: true }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete merge actors', toolName: 'control_actor', arguments: { action: 'delete', actorNames: [MERGE_PARENT_ACTOR, MERGE_CHILD_ACTOR] }, expected: 'success|not found' },
    { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
  );
}

runToolTests('system-control', testCases);
