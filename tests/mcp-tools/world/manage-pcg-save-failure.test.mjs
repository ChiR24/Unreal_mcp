import { randomUUID } from 'node:crypto';
import { runToolTests } from '../../test-runner.mjs';

const stamp = `${Date.now()}_${randomUUID().replaceAll('-', '')}`;
const runFolder = `Run_${stamp}`;
const runRoot = `/Game/MCP_PCG_Validation/${runFolder}`;
const graphPath = `${runRoot}/Graph`;
const subgraphPath = `${runRoot}/Subgraph`;
const lockKey = `_mcp_pcg_save_failure_${stamp}`;
const captured = key => '${captured:' + key + '}';
const errorDetail = process.env.UNREAL_MCP_SERVER_ARGS?.split(',').includes('proxy')
  ? 'structuredContent.typedError.unrealDetail' : 'structuredContent.result.result';

const packagePathScript = packageName => `
import os, unreal
assert unreal.SystemLibrary.get_game_name() == 'PCGValidation'
content_root = os.path.realpath(unreal.Paths.project_content_dir())
owned_root = os.path.realpath(os.path.join(content_root, 'MCP_PCG_Validation', '${runFolder}'))
package_path = os.path.realpath(os.path.join(owned_root, '${packageName}.uasset'))
assert os.path.commonpath((content_root, owned_root)) == content_root and owned_root != content_root
assert os.path.commonpath((owned_root, package_path)) == owned_root and package_path != owned_root
`;

const acquireParentLock = `${packagePathScript('Graph')}
import builtins, ctypes, hashlib
assert not hasattr(builtins, '${lockKey}')
with open(package_path, 'rb') as package_file:
    baseline_hash = hashlib.sha256(package_file.read()).hexdigest()
kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
create_file = kernel32.CreateFileW
create_file.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p,
                        ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
create_file.restype = ctypes.c_void_p
close_handle = kernel32.CloseHandle
close_handle.argtypes = [ctypes.c_void_p]
close_handle.restype = ctypes.c_int
handle = create_file(package_path, 0x80000000, 0, None, 3, 0x80, None)
if handle is None or handle == ctypes.c_void_p(-1).value:
    raise RuntimeError('Could not exclusively lock the owned PCG package')
try:
    setattr(builtins, '${lockKey}', {'handle': int(handle), 'baseline_hash': baseline_hash})
except BaseException:
    close_handle(ctypes.c_void_p(handle))
    raise
`;

const parentBytesCheck = `
state = getattr(builtins, '${lockKey}', None)
assert state is not None
kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
set_file_pointer = kernel32.SetFilePointerEx
set_file_pointer.argtypes = [ctypes.c_void_p, ctypes.c_longlong, ctypes.POINTER(ctypes.c_longlong), ctypes.c_uint32]
set_file_pointer.restype = ctypes.c_int
read_file = kernel32.ReadFile
read_file.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint32,
                      ctypes.POINTER(ctypes.c_uint32), ctypes.c_void_p]
read_file.restype = ctypes.c_int
position = ctypes.c_longlong()
assert set_file_pointer(ctypes.c_void_p(state['handle']), 0, ctypes.byref(position), 0)
digest = hashlib.sha256()
while True:
    buffer = ctypes.create_string_buffer(1024 * 1024)
    bytes_read = ctypes.c_uint32()
    assert read_file(ctypes.c_void_p(state['handle']), buffer, len(buffer), ctypes.byref(bytes_read), None)
    if bytes_read.value == 0:
        break
    digest.update(buffer.raw[:bytes_read.value])
assert digest.hexdigest() == state['baseline_hash']
`;

const verifyParentBytesAndChild = `${packagePathScript('Graph')}
import builtins, ctypes, hashlib, os
${parentBytesCheck}
subgraph_root = os.path.realpath(os.path.join(content_root, 'MCP_PCG_Validation', '${runFolder}'))
subgraph_file = os.path.realpath(os.path.join(subgraph_root, 'Subgraph.uasset'))
assert os.path.commonpath((content_root, subgraph_root)) == content_root and subgraph_root != content_root
assert os.path.commonpath((subgraph_root, subgraph_file)) == subgraph_root and subgraph_file != subgraph_root
assert os.path.isfile(subgraph_file) and os.path.getsize(subgraph_file) > 0
`;

const releaseParentLock = `
import builtins, ctypes, unreal
assert unreal.SystemLibrary.get_game_name() == 'PCGValidation'
state = getattr(builtins, '${lockKey}', None)
if state is not None:
    close_handle = ctypes.WinDLL('kernel32', use_last_error=True).CloseHandle
    close_handle.argtypes = [ctypes.c_void_p]
    close_handle.restype = ctypes.c_int
    if not close_handle(ctypes.c_void_p(state['handle'])):
        raise RuntimeError('Could not release the owned PCG package lock')
    delattr(builtins, '${lockKey}')
`;

const cases = [
  { scenario: 'Setup: create and save this run’s exclusively owned graph', toolName: 'manage_pcg',
    arguments: { action: 'create_pcg_graph', graphPath, save: true }, expected: 'success', abortOnFailure: true,
    assertions: [{ path: 'structuredContent.result.created', equals: true },
      { path: 'structuredContent.result.saved', equals: true }],
    captureResult: [{ key: 'ownedGraph', fromField: 'result.graphPath' }] },
  { scenario: 'Setup: add a node before testing the save failure', toolName: 'manage_pcg',
    arguments: { action: 'add_transform_points', graphPath, settings: { Seed: 123 }, save: false }, expected: 'success', abortOnFailure: true,
    captureResult: [{ key: 'transformNode', fromField: 'result.nodeId' }] },
  { scenario: 'Lock only the newly saved graph package against writes', toolName: 'system_control',
    arguments: { action: 'execute_python', code: acquireParentLock }, expected: 'success', abortOnFailure: true },
  { scenario: 'A refused graph save keeps the edit committed in memory but reports saved false', toolName: 'manage_pcg',
    arguments: { action: 'set_pcg_node_settings', graphPath, nodeId: captured('transformNode'),
      settings: { Seed: 9876 }, save: true }, timeoutMs: 30000,
    expected: { condition: 'error', errorPattern: 'SAVE_FAILED' }, assertions: [
      { path: `${errorDetail}.committed`, equals: true },
      { path: `${errorDetail}.saved`, equals: false },
    ] },
  { scenario: 'Verify the failed save left the authored edit in memory', toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_graph', graphPath, nodeId: captured('transformNode'), includeSettings: true }, expected: 'success',
    assertions: [{ path: 'structuredContent.result.items.0.settings.Seed', equals: 9876 }] },
  { scenario: 'Verify the locked graph package bytes are unchanged', toolName: 'system_control',
    arguments: { action: 'execute_python', code: `${packagePathScript('Graph')}\nimport builtins, ctypes, hashlib\n${parentBytesCheck}` }, expected: 'success' },
  { scenario: 'Create and save the child before the owned parent save is refused', toolName: 'manage_pcg',
    arguments: { action: 'create_pcg_subgraph', subgraphPath, parentGraphPath: graphPath, save: true }, timeoutMs: 30000,
    expected: { condition: 'error', errorPattern: 'SAVE_FAILED' }, assertions: [
      { path: `${errorDetail}.parentCommitted`, equals: true },
      { path: `${errorDetail}.parentSaved`, equals: false },
      { path: `${errorDetail}.subgraphSaved`, equals: true },
      { path: `${errorDetail}.partialSave`, equals: true },
      { path: `${errorDetail}.residualChanges`, equals: true },
    ] },
  { scenario: 'Verify the child package exists and the parent package remains byte-identical', toolName: 'system_control',
    arguments: { action: 'execute_python', code: verifyParentBytesAndChild }, expected: 'success' },
  { scenario: 'Cleanup: release the per-run exclusive package lock', toolName: 'system_control',
    arguments: { action: 'execute_python', code: releaseParentLock }, expected: 'success', abortOnFailure: true },
  { scenario: 'Capture the unchanged-state token before the caller transaction', toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_graph', graphPath }, expected: 'success',
    captureResult: [{ key: 'callerRevision', fromField: 'result.revision' }] },
  { scenario: 'Begin an exclusively owned caller editor transaction', toolName: 'system_control',
    arguments: { action: 'execute_python', code: `import builtins, unreal
assert unreal.SystemLibrary.get_game_name() == 'PCGValidation'
assert not hasattr(builtins, '${lockKey}_transaction')
transaction = unreal.ScopedEditorTransaction('Owned PCG caller boundary')
transaction.__enter__()
try:
    setattr(builtins, '${lockKey}_transaction', transaction)
except BaseException:
    transaction.__exit__(None, None, None)
    raise
` }, expected: 'success', abortOnFailure: true },
  { scenario: 'PCG mutation is refused inside the active caller transaction', toolName: 'manage_pcg',
    arguments: { action: 'set_pcg_node_settings', graphPath, nodeId: captured('transformNode'), settings: { Seed: 1357 }, save: false },
    expected: { condition: 'error', errorPattern: 'EDITOR_BUSY' } },
  { scenario: 'Read-only PCG inspection still works and proves the refusal preserved state', toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_graph', graphPath }, expected: 'success',
    assertions: [{ path: 'structuredContent.result.revision', equals: captured('callerRevision') }] },
  { scenario: 'End only this test’s caller transaction without cancelling anyone else', toolName: 'system_control',
    arguments: { action: 'execute_python', code: `import builtins, unreal
assert unreal.SystemLibrary.get_game_name() == 'PCGValidation'
transaction = getattr(builtins, '${lockKey}_transaction', None)
if transaction is not None:
    transaction.__exit__(None, None, None)
    delattr(builtins, '${lockKey}_transaction')
` }, expected: 'success', abortOnFailure: true },
  { scenario: 'Cleanup: delete only this run’s parent graph', toolName: 'manage_asset',
    arguments: { action: 'delete', path: captured('ownedGraph') }, expected: 'success|not found',
    consent: { capability: 'asset.delete', acknowledge: 'elevated' } },
  { scenario: 'Cleanup: delete only this run’s child subgraph', toolName: 'manage_asset',
    arguments: { action: 'delete', path: subgraphPath }, expected: 'success|not found',
    consent: { capability: 'asset.delete', acknowledge: 'elevated' } },
];

runToolTests('manage_pcg_save_failure', cases, {
  isolatedProject: {
    projectName: process.env.UNREAL_MCP_TEST_PROJECT_NAME,
    projectDir: process.env.UE_PROJECT_PATH,
    editorBaseDir: process.env.UE_EDITOR_BASE_DIR,
    engineVersionPrefix: process.env.UNREAL_MCP_TEST_ENGINE_VERSION,
  },
  foldTwins: false,
});
