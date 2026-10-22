import { randomUUID } from 'node:crypto';
import { runToolTests } from '../../test-runner.mjs';

const variant = process.env.UNREAL_MCP_PCG_VARIANT;
const graphPath = `/Game/MCP_PCG_Validation/Compatibility_${randomUUID().replaceAll('-', '')}/Graph`;
if (!Array.isArray(globalThis.__capturedToolSuites) && !['disabled', 'noeditor'].includes(variant)) {
  throw new Error('Choose the explicitly built isolated PCG variant with UNREAL_MCP_PCG_VARIANT=disabled|noeditor.');
}
const cases = variant === 'disabled' ? [
  { scenario: 'A PCG-disabled build refuses discovery clearly', toolName: 'manage_pcg',
    arguments: { action: 'list_pcg_node_types', limit: 1 }, expected: { condition: 'error', errorPattern: 'PCG_PLUGIN_NOT_AVAILABLE' } },
  { scenario: 'The absent optional module cannot create an asset', toolName: 'manage_pcg',
    arguments: { action: 'create_pcg_graph', graphPath, save: false }, expected: { condition: 'error', errorPattern: 'PCG_PLUGIN_NOT_AVAILABLE' } },
] : [
  { scenario: 'Model discovery remains available without the PCGEditor API branch', toolName: 'manage_pcg',
    arguments: { action: 'list_pcg_node_types', limit: 1 }, expected: 'success' },
  { scenario: 'A model graph can be created without the PCGEditor API branch', toolName: 'manage_pcg',
    arguments: { action: 'create_pcg_graph', graphPath, save: false }, expected: 'success', abortOnFailure: true,
    captureResult: [{ key: 'ownedGraph', fromField: 'result.graphPath' }] },
  { scenario: 'Editor-only synchronization is explicitly unsupported', toolName: 'manage_pcg',
    arguments: { action: 'sync_pcg_editor_graph', graphPath, openEditor: false, save: false },
    expected: { condition: 'error', errorPattern: 'NOT_SUPPORTED' } },
  { scenario: 'Backend graph state is still readable after the refusal', toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_graph', graphPath, includeSettings: true }, expected: 'success',
    assertions: [{ path: 'structuredContent.result.total', equals: 2 }] },
  { scenario: 'Delete only the uniquely owned compatibility asset', toolName: 'manage_asset',
    arguments: { action: 'delete', path: '${captured:ownedGraph}' }, expected: 'success',
    consent: { capability: 'asset.delete', acknowledge: 'elevated' } },
];
runToolTests('manage_pcg_compatibility', cases, {
  isolatedProject: { projectName: process.env.UNREAL_MCP_TEST_PROJECT_NAME, projectDir: process.env.UE_PROJECT_PATH,
    editorBaseDir: process.env.UE_EDITOR_BASE_DIR, engineVersionPrefix: process.env.UNREAL_MCP_TEST_ENGINE_VERSION },
  foldTwins: false,
});
