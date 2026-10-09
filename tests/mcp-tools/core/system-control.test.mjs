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
const SAVE_BP_PATH = `${TEST_FOLDER}/BP_SuiteSave`;
const SAVE_CLASS = `${SAVE_BP_PATH}.BP_SuiteSave_C`;
const PYTHON_TEST_ID = Date.now();
const PYTHON_FILE_RELATIVE = `Saved/MCPTests/system-control-${PYTHON_TEST_ID}.py`;
const PYTHON_HELPER_RELATIVE = `Saved/MCPTests/system-control-${PYTHON_TEST_ID}-helper.txt`;
const PYTHON_FILE_LITERAL = JSON.stringify(PYTHON_FILE_RELATIVE);
const PYTHON_HELPER_LITERAL = JSON.stringify(PYTHON_HELPER_RELATIVE);
const PROJECT_SETTING_SECTION = '/Script/Engine.Engine';
const PROJECT_SETTING_KEY = `McpSystemControlSmoke_${Date.now()}`;
const TRACE_TEST_ID = Date.now();
const OPTIONAL_SOUND = '/Engine/VREditor/Sounds/VR_click1.VR_click1';
const TRACE_CAPTURE_FILE = `MCPTests/insights-capture-${TRACE_TEST_ID}.utrace`;
const TRACE_START_FILE = `MCPTests/insights-start-${TRACE_TEST_ID}.utrace`;
const TRACE_SNAPSHOT_FILE = `MCPTests/insights-snapshot-${TRACE_TEST_ID}.utrace`;
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
  { scenario: 'ACTION: screenshot', toolName: 'system_control', arguments: { action: 'screenshot', filename: 'SystemControl_NullRHI', resolution: '640x360', mode: 'editor_viewport', returnBase64: false, keepFile: true, includeMetadata: true, metadata: { source: 'system-control-suite' } }, expected: 'success' },
  { scenario: 'OPTIONAL: game_viewport screenshot into a chosen folder', toolName: 'system_control', arguments: { action: 'screenshot', filename: 'SystemControl_Game', mode: 'game_viewport', path: 'Saved/Screenshots', returnBase64: false }, expected: 'success|NO_VIEWPORT' },
  { scenario: 'OPTIONAL: screenshot of a named editor window', toolName: 'system_control', arguments: { action: 'screenshot', filename: 'SystemControl_Window', mode: 'full_editor_window', window: '0', resolution: '640x360' }, expected: 'success' },
  // === CONFIG ===
  { scenario: 'CONFIG: set_resolution', toolName: 'system_control', arguments: { action: 'set_resolution', width: 1280, height: 720, windowed: true }, expected: 'success' },
  { scenario: 'CONFIG: set_fullscreen', toolName: 'system_control', arguments: { action: 'set_fullscreen', enabled: false }, expected: 'success' },
  // === ACTION ===
  { scenario: 'ACTION: execute_command', toolName: 'system_control', arguments: { action: 'execute_command', command: 'stat unit' }, expected: 'success' },
  { scenario: 'ACTION: console_command', toolName: 'system_control', arguments: { action: 'console_command', command: 'stat fps' }, expected: 'success' },
  { scenario: 'ACTION: run_ubt', toolName: 'system_control', arguments: { action: 'run_ubt', target: 'MCPtestEditor', platform: 'Linux', configuration: 'Development', arguments: '-NoHotReload' }, expected: 'success' },
  { scenario: 'ACTION: package_project', toolName: 'system_control', arguments: { action: 'package_project', platform: 'Win64', configuration: 'Development', archiveDirectory: 'D:/Tmp/Packaged', maps: ['/Game/Maps/L_Hub'], pak: true, build: false }, expected: 'success' },
  { scenario: 'READ: package_status', toolName: 'system_control', arguments: { action: 'package_status', jobId: '00000000-0000-0000-0000-000000000000' }, expected: 'error|JOB_NOT_FOUND' },
  { scenario: 'ACTION: launch_build refuses a build outside the project', toolName: 'system_control', arguments: { action: 'launch_build', archiveDirectory: 'C:/Windows', seconds: 5, windowed: false }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'ACTION: subscribe', toolName: 'system_control', arguments: { action: 'subscribe' }, expected: 'success' },
  { scenario: 'ACTION: unsubscribe', toolName: 'system_control', arguments: { action: 'unsubscribe' }, expected: 'success' },
  { scenario: 'READ: read_log newest lines', toolName: 'system_control', arguments: { action: 'read_log', lines: 20 }, expected: 'success' },
  { scenario: 'READ: read_log warnings in one category', toolName: 'system_control', arguments: { action: 'read_log', lines: 50, minVerbosity: 'warning', category: 'LogTemp', filter: 'mcp' }, expected: 'success' },
  { scenario: 'READ: read_log the Live Coding console log', toolName: 'system_control', arguments: { action: 'read_log', source: 'livecoding', lines: 20, filter: 'error' }, expected: 'success' },
  { scenario: 'READ: read_log the last build log for compiler errors', toolName: 'system_control', arguments: { action: 'read_log', source: 'build', lines: 20, filter: 'error' }, expected: 'success' },
  { scenario: 'READ: read_log refuses category on a log file source', toolName: 'system_control', arguments: { action: 'read_log', source: 'build', category: 'LogTemp' }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'READ: read_log the previous editor run', toolName: 'system_control', arguments: { action: 'read_log', source: 'previous', lines: 20 }, expected: 'success|not found' },
  { scenario: 'READ: read_log two editor runs back', toolName: 'system_control', arguments: { action: 'read_log', source: 'previous', runsBack: 2, lines: 20 }, expected: 'success|not found' },
  // The suite's own screenshots are MCP output files: list them, then remove
  // them through the tool instead of leaving them in Saved/Screenshots.
  { scenario: 'READ: list_output_files', toolName: 'system_control', arguments: { action: 'list_output_files' }, expected: 'success' },
  { scenario: 'READ: list_output_files one page of screenshots', toolName: 'system_control', arguments: { action: 'list_output_files', root: 'Saved/Screenshots', extension: 'png', limit: 5, offset: 0 }, expected: 'success', assertions: [{ path: 'structuredContent.result.returned', gte: 0 }] },
  { scenario: 'CLEANUP: delete_output_file paths the suite screenshot', toolName: 'system_control', arguments: { action: 'delete_output_file', paths: ['Saved/Screenshots/SystemControl_NullRHI.png'] }, expected: 'success|PARTIAL_DELETE' },
  { scenario: 'CLEANUP: delete_output_file the suite screenshot', toolName: 'system_control', arguments: { action: 'delete_output_file', path: 'Saved/Screenshots/SystemControl_NullRHI.png' }, expected: 'success|not found' },
  { scenario: 'CLEANUP: delete_output_file reports each of several paths', toolName: 'system_control', arguments: { action: 'delete_output_file', paths: ['Saved/Screenshots/SystemControl_NullRHI.png', 'Config/DefaultGame.ini'] }, expected: 'error|PARTIAL_DELETE' },
  { scenario: 'CLEANUP: delete_output_file refuses a project file', toolName: 'system_control', arguments: { action: 'delete_output_file', path: 'Config/DefaultGame.ini' }, expected: 'error|PATH_OUTSIDE_OUTPUT_ROOTS' },
  // A save slot round trip on a Blueprint SaveGame the suite makes (SaveGame itself is abstract):
  // create the slot with its defaults, set a value, list and read it, refuse bad edits, then delete it.
  { scenario: 'Setup: create a SaveGame Blueprint', toolName: 'manage_blueprint', arguments: { action: 'create', name: 'BP_SuiteSave', savePath: TEST_FOLDER, parentClass: '/Script/Engine.SaveGame' }, expected: 'success|already exists' },
  { scenario: 'Setup: give the SaveGame Blueprint a saved Score', toolName: 'manage_blueprint', arguments: { action: 'add_variable', blueprintPath: SAVE_BP_PATH, variableName: 'Score', variableType: 'Integer' }, expected: 'success|already exists' },
  { scenario: 'Setup: compile the SaveGame Blueprint', toolName: 'manage_blueprint', arguments: { action: 'compile', blueprintPath: SAVE_BP_PATH }, expected: 'success' },
  { scenario: 'CREATE: edit_save_game refuses the abstract SaveGame class', toolName: 'system_control', arguments: { action: 'edit_save_game', slotName: 'McpSuiteSlot', saveGameClass: '/Script/Engine.SaveGame' }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'CREATE: edit_save_game creates a slot from a SaveGame class', toolName: 'system_control', arguments: { action: 'edit_save_game', slotName: 'McpSuiteSlot', userIndex: 0, saveGameClass: SAVE_CLASS }, expected: 'success', assertions: [{ path: 'structuredContent.result.created', equals: true }] },
  { scenario: 'WRITE: edit_save_game sets a saved value', toolName: 'system_control', arguments: { action: 'edit_save_game', slotName: 'McpSuiteSlot', properties: { Score: 7 } }, expected: 'success', assertions: [{ path: 'structuredContent.result.properties.Score', equals: 7 }] },
  { scenario: 'READ: list_save_games lists the slots', toolName: 'system_control', arguments: { action: 'list_save_games' }, expected: 'success', assertions: [{ path: 'structuredContent.result.count', gte: 1 }] },
  { scenario: 'READ: list_save_games reads one slot', toolName: 'system_control', arguments: { action: 'list_save_games', slotName: 'McpSuiteSlot', userIndex: 0 }, expected: 'success', assertions: [{ path: 'structuredContent.result.saveGameClass', equals: SAVE_CLASS }] },
  { scenario: 'WRITE: edit_save_game refuses a property the slot does not save', toolName: 'system_control', arguments: { action: 'edit_save_game', slotName: 'McpSuiteSlot', properties: { NotSaved: 1 } }, expected: 'error|UNKNOWN_PROPERTY' },
  { scenario: 'WRITE: edit_save_game refuses a slot name that leaves SaveGames', toolName: 'system_control', arguments: { action: 'edit_save_game', slotName: '../Config/McpSuiteSlot', deleteSlot: true }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'CLEANUP: edit_save_game deletes the slot', toolName: 'system_control', arguments: { action: 'edit_save_game', slotName: 'McpSuiteSlot', deleteSlot: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.existsAfter', equals: false }] },
  { scenario: 'READ: list_save_games on a deleted slot', toolName: 'system_control', arguments: { action: 'list_save_games', slotName: 'McpSuiteSlot' }, expected: 'error|SAVE_SLOT_NOT_FOUND' },
  // === CREATE ===
  { scenario: 'CREATE: spawn_category', toolName: 'system_control', arguments: { action: 'spawn_category', categoryName: 'AI', enabled: true }, expected: 'success' },
  // === ACTION ===
  { scenario: 'ACTION: start_session', toolName: 'system_control', arguments: { action: 'start_session', channels: 'cpu' }, expected: 'success' },
  { scenario: 'INFO: get_trace_status', toolName: 'system_control', arguments: { action: 'get_trace_status' }, expected: 'success' },
  { scenario: 'ACTION: pause_session', toolName: 'system_control', arguments: { action: 'pause_session' }, expected: { successPattern: 'paused', errorPattern: 'TRACE_PAUSE_FAILED' } },
  { scenario: 'ACTION: resume_session', toolName: 'system_control', arguments: { action: 'resume_session' }, expected: { successPattern: 'resumed', errorPattern: 'TRACE_RESUME_FAILED' } },
  { scenario: 'ACTION: write_snapshot', toolName: 'system_control', arguments: { action: 'write_snapshot', snapshotPath: TRACE_SNAPSHOT_FILE, overwrite: true }, expected: 'success' },
  { scenario: 'ACTION: analyze_trace', toolName: 'system_control', arguments: { action: 'analyze_trace', traceFile: TRACE_SNAPSHOT_FILE }, expected: 'success' },
  { scenario: 'ACTION: send_snapshot', toolName: 'system_control', arguments: { action: 'send_snapshot', host: 'localhost', port: 1981 }, expected: { successPattern: 'snapshot', errorPattern: 'SNAPSHOT_SEND_FAILED' } },
  { scenario: 'ACTION: stop_session', toolName: 'system_control', arguments: { action: 'stop_session' }, expected: 'success' },
  { scenario: 'ACTION: capture_insights_trace', toolName: 'system_control', arguments: { action: 'capture_insights_trace', channels: 'cpu', traceFile: TRACE_CAPTURE_FILE, overwrite: true }, expected: 'success' },
  { scenario: 'ACTION: stop_session after capture', toolName: 'system_control', arguments: { action: 'stop_session' }, expected: 'success' },
  { scenario: 'ACTION: start_unreal_insights', toolName: 'system_control', arguments: { action: 'start_unreal_insights', launchViewer: false, channels: 'cpu', connectionType: 'file', traceFile: TRACE_START_FILE, overwrite: true }, expected: 'success' },
  { scenario: 'ACTION: stop_session after start_unreal_insights', toolName: 'system_control', arguments: { action: 'stop_session' }, expected: 'success' },
  { scenario: 'ACTION: lumen_update_scene', toolName: 'system_control', arguments: { action: 'lumen_update_scene' }, expected: 'success' },
  // === PLAYBACK ===
  { scenario: 'PLAYBACK: play_sound', toolName: 'system_control', arguments: { action: 'play_sound', volume: 0 }, expected: 'success' },
  { scenario: 'OPTIONAL: play_sound with soundPath and pitch', toolName: 'system_control', arguments: { action: 'play_sound', soundPath: OPTIONAL_SOUND, pitch: 1.0, volume: 0, startTime: 0 }, expected: 'success' },
  // === CREATE ===
  { scenario: 'CREATE: create_widget', toolName: 'system_control', arguments: { action: 'create_widget', name: WIDGET_NAME, savePath: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'OPTIONAL: create_widget from a widgetPath', toolName: 'system_control', arguments: { action: 'create_widget', widgetPath: `${TEST_FOLDER}/${WIDGET_NAME}_FromPath` }, expected: 'success|already exists' },
  { scenario: 'OPTIONAL: create_widget refuses an unknown widgetType', toolName: 'system_control', arguments: { action: 'create_widget', name: `${WIDGET_NAME}_BadType`, savePath: TEST_FOLDER, widgetType: 'NoSuchWidgetClass' }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'OPTIONAL: create_widget with widgetType', toolName: 'system_control', arguments: { action: 'create_widget', name: `${WIDGET_NAME}_Typed`, savePath: TEST_FOLDER, widgetType: 'UserWidget' }, expected: 'success|already exists' },
  // === ACTION ===
  { scenario: 'ACTION: show_widget', toolName: 'system_control', arguments: { action: 'show_widget', widgetId: 'notification', message: 'System control smoke', duration: 0.1 }, expected: 'success' },
  // === ADD ===
  { scenario: 'ADD: add_widget_child', toolName: 'system_control', arguments: { action: 'add_widget_child', widgetPath: WIDGET_PATH, childClass: 'TextBlock', name: 'SystemControlText', text: 'System control child' }, expected: 'success', assertions: [{ path: 'structuredContent.result.componentName', equals: 'SystemControlText', label: 'the child keeps the requested name' }] },
  { scenario: 'ADD: add_widget_child parentName', toolName: 'system_control', arguments: { action: 'add_widget_child', widgetPath: WIDGET_PATH, childClass: 'TextBlock', name: 'SystemControlNestedText', parentName: 'RootCanvas', text: 'System control nested child' }, expected: 'success', assertions: [{ path: 'structuredContent.result.parentName', equals: 'RootCanvas', label: 'the child lands under the named panel' }] },
  // === CONFIG ===
  { scenario: 'CONFIG: set_cvar', toolName: 'system_control', arguments: { action: 'set_cvar', name: 'r.ScreenPercentage', value: '100' }, expected: 'success' },
  { scenario: 'OPTIONAL: set_cvar via cvar alias', toolName: 'system_control', arguments: { action: 'set_cvar', cvar: 'r.ScreenPercentage', value: '100' }, expected: 'success' },
  // === INFO ===
  { scenario: 'INFO: get_project_settings', toolName: 'system_control', arguments: { action: 'get_project_settings', section: '/Script/Engine.Engine' }, expected: 'success' },
  // === ACTION ===
  { scenario: 'ACTION: validate_assets', toolName: 'system_control', arguments: { action: 'validate_assets', paths: [VALIDATION_MATERIAL] }, expected: 'success' },
  { scenario: 'ACTION: validate_assets assetPath', toolName: 'system_control', arguments: { action: 'validate_assets', assetPath: VALIDATION_MATERIAL }, expected: 'success' },
  { scenario: 'ACTION: validate_assets path recursive', toolName: 'system_control', arguments: { action: 'validate_assets', path: TEST_FOLDER, recursive: false }, expected: 'success' },
  // Data Validation runs the project's and the engine's validators, which loading alone never did.
  { scenario: 'ACTION: validate_assets dataValidation', toolName: 'system_control', arguments: { action: 'validate_assets', assetPath: VALIDATION_MATERIAL, dataValidation: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.results', includesObject: { kind: 'asset', isValid: true }, label: 'the material loads and passes its validators' }] },
  { scenario: 'ACTION: validate_assets dataValidation folder', toolName: 'system_control', arguments: { action: 'validate_assets', path: TEST_FOLDER, recursive: false, dataValidation: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.results', includesObject: { kind: 'directory' }, label: 'the folder row carries the verdict counts' }] },
  // === CONFIG ===
  { scenario: 'CONFIG: set_project_setting', toolName: 'system_control', arguments: { action: 'set_project_setting', section: PROJECT_SETTING_SECTION, key: PROJECT_SETTING_KEY, value: '1' }, expected: 'success' },
  { scenario: 'INFO: configure_rendering reads the rendering methods', toolName: 'system_control', arguments: { action: 'configure_rendering' }, expected: 'success', captureResult: [{ key: 'render_globalIllumination', fromField: 'result.settings.globalIllumination' }, { key: 'render_reflections', fromField: 'result.settings.reflections' }, { key: 'render_shadows', fromField: 'result.settings.shadows' }, { key: 'render_antiAliasing', fromField: 'result.settings.antiAliasing' }, { key: 'render_hardwareRayTracing', fromField: 'result.settings.hardwareRayTracing' }, { key: 'render_meshDistanceFields', fromField: 'result.settings.meshDistanceFields' }, { key: 'render_megaLights', fromField: 'result.settings.megaLights' }, { key: 'render_bloom', fromField: 'result.settings.bloom' }, { key: 'render_autoExposure', fromField: 'result.settings.autoExposure' }, { key: 'render_motionBlur', fromField: 'result.settings.motionBlur' }] },
  { scenario: 'CONFIG: configure_rendering turns the default bloom off', toolName: 'system_control', arguments: { action: 'configure_rendering', bloom: false }, expected: 'success', assertions: [{ path: 'structuredContent.result.settings.bloom', equals: false, label: 'bloom reads back off' }] },
  { scenario: 'CONFIG: configure_rendering puts every setting back as it read', toolName: 'system_control', arguments: { action: 'configure_rendering', globalIllumination: '${captured:render_globalIllumination}', reflections: '${captured:render_reflections}', shadows: '${captured:render_shadows}', antiAliasing: '${captured:render_antiAliasing}', hardwareRayTracing: '${captured:render_hardwareRayTracing}', meshDistanceFields: '${captured:render_meshDistanceFields}', megaLights: '${captured:render_megaLights}', bloom: '${captured:render_bloom}', autoExposure: '${captured:render_autoExposure}', motionBlur: '${captured:render_motionBlur}' }, expected: 'success' },
  { scenario: 'ACTION: execute_python', toolName: 'system_control', arguments: { action: 'execute_python', code: 'print("system-control-ok")' }, expected: 'success' },
  { scenario: 'Setup: create execute_python file', toolName: 'system_control', arguments: { action: 'execute_python', code: CREATE_PYTHON_FILE_CODE }, expected: 'success' },
  // This result is asserted on the returned MCP response, so it also catches
  // temp wrapper cleanup racing file execution before output/status are written.
  { scenario: 'ACTION: execute_python file', toolName: 'system_control', arguments: { action: 'execute_python', file: PYTHON_FILE_RELATIVE }, expected: 'success', assertions: [{ path: 'structuredContent.result.output', equals: 'system-control-file-ok:sibling-file-ok', label: 'python file has __file__ and synchronous output' }] },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete execute_python file', toolName: 'system_control', arguments: { action: 'execute_python', code: DELETE_PYTHON_FILE_CODE }, expected: 'success' },
  { scenario: 'Cleanup: remove project setting', toolName: 'system_control', arguments: { action: 'execute_python', code: CLEANUP_PROJECT_SETTING_CODE }, expected: 'success' },
  { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
  // run_tests answers with each test's result; it used to say only "check the Output Log".
  { scenario: 'ACTION: run_tests', toolName: 'system_control', arguments: { action: 'run_tests', filter: 'System.Core.Math', maxTests: 3, timeoutSeconds: 30 }, expected: 'success', assertions: [{ path: 'structuredContent.result.tests', minLength: 1, label: 'each test run answers with its result' }] },
  { scenario: 'ERROR: run_tests names a filter no test matches', toolName: 'system_control', arguments: { action: 'run_tests', filter: 'No.Such.Test.Group' }, expected: 'error|NOT_FOUND' },
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
    { scenario: 'ACTION: start_profiling', toolName: 'system_control', arguments: {"action": "start_profiling", "duration": 1}, expected: 'success' },
    // === PLAYBACK ===
    { scenario: 'PLAYBACK: stop_profiling', toolName: 'system_control', arguments: {"action": "stop_profiling"}, expected: 'success' },
    // === ACTION ===
    { scenario: 'ACTION: run_benchmark', toolName: 'system_control', arguments: {"action": "run_benchmark", "duration": 1, "type": "CPU"}, expected: 'success' },
    { scenario: 'ACTION: show_fps', toolName: 'system_control', arguments: {"action": "show_fps"}, expected: 'success' },
    { scenario: 'ACTION: show_stats', toolName: 'system_control', arguments: {"action": "show_stats", "category": "Unit", "enabled": true}, expected: 'success' },
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
    { scenario: 'OPTIONAL: apply_baseline_settings with profile', toolName: 'system_control', arguments: { action: 'apply_baseline_settings', profile: 'balanced' }, expected: 'success' },
    { scenario: 'ACTION: optimize_draw_calls', toolName: 'system_control', arguments: {"action": "optimize_draw_calls", "enableInstancing": false, "enableBatching": true}, expected: 'success' },
    { scenario: 'ACTION: merge_actors', toolName: 'system_control', arguments: {"action": "merge_actors", "actors": [MERGE_PARENT_ACTOR, MERGE_CHILD_ACTOR], "replaceSourceActors": false, "outputPath": MERGED_ACTOR_ASSET}, expected: 'success' },
    { scenario: 'ACTION: merge_actors via packageName', toolName: 'system_control', arguments: {"action": "merge_actors", "actors": [MERGE_PARENT_ACTOR, MERGE_CHILD_ACTOR], "replaceSourceActors": false, "packageName": MERGED_ACTOR_PACKAGE_ASSET}, expected: 'success' },
    // === CONFIG ===
    { scenario: 'CONFIG: configure_occlusion_culling', toolName: 'system_control', arguments: {"action": "configure_occlusion_culling"}, expected: 'success' },
  { scenario: 'CONFIG: configure_occlusion_culling slop and min screen radius', toolName: 'system_control', arguments: { action: 'configure_occlusion_culling', enabled: true, slop: 1, minScreenRadius: 0.01 }, expected: 'success' },
    // === ACTION ===
    { scenario: 'ACTION: optimize_shaders', toolName: 'system_control', arguments: {"action": "optimize_shaders"}, expected: 'success' },
    { scenario: 'OPTIONAL: optimize_shaders without forced recompile', toolName: 'system_control', arguments: { action: 'optimize_shaders', forceRecompile: false }, expected: 'success' },
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
