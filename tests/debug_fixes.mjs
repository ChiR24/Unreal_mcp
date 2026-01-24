import { runToolTests } from './test-runner.mjs';

const TEST_FOLDER = '/Game/DebugTest';

const testCases = [
  { scenario: 'Asset: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'Blueprint: create Actor blueprint', toolName: 'manage_blueprint', arguments: { action: 'create', name: 'BP_DebugTest', path: TEST_FOLDER, parentClass: 'Actor' }, expected: 'success|already exists' },
  { scenario: 'Blueprint: ensure exists', toolName: 'manage_blueprint', arguments: { action: 'bp_ensure_exists', name: `${TEST_FOLDER}/BP_DebugTest`, timeoutMs: 30000 }, expected: 'success' },
  { scenario: 'Level: create level', toolName: 'manage_level', arguments: { action: 'create_level', levelName: 'L_DebugTest', levelPath: TEST_FOLDER }, expected: 'success' },
  { scenario: 'Level: save current level', toolName: 'manage_level', arguments: { action: 'save' }, expected: 'success' },
  { scenario: 'Level: save as', toolName: 'manage_level', arguments: { action: 'save_as', savePath: `${TEST_FOLDER}/L_DebugTest_SavedAs` }, expected: 'success' },
  { scenario: 'PCG: Create PCG graph', toolName: 'manage_level', arguments: { action: 'create_pcg_graph', graphName: 'PCG_DebugTestGraph', graphPath: TEST_FOLDER }, expected: 'success' },
  { scenario: 'PCG: Configure grid size', toolName: 'manage_level', arguments: { action: 'configure_grid_size', gridCellSize: 12800, loadingRange: 25600 }, expected: 'success' },
  { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' }
];

runToolTests('debug_fixes', testCases);
