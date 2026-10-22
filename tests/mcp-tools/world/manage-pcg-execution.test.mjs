import { randomUUID } from 'node:crypto';
import { runToolTests } from '../../test-runner.mjs';

const stamp = `${Date.now()}_${randomUUID().replaceAll('-', '')}`;
const graphPath = `/Game/MCP_PCG_Validation/Execution_${stamp}/Graph`;
const actorName = `MCP_PCG_Execution_${stamp}`;
const nonManaged = `MCP_PCG_Unmanaged_${stamp}`;
const captured = key => '${captured:' + key + '}';
const componentPath = captured('component');
const generate = (scenario, count, extra = {}) => ({ scenario, toolName: 'manage_pcg',
  arguments: { action: 'generate_pcg_graph', componentPath, force: true, timeoutSeconds: 30, save: false, ...extra },
  timeoutMs: 45000, expected: 'success', abortOnFailure: true, assertions: [
    { path: 'structuredContent.result.state', equals: 'succeeded' },
    { path: 'structuredContent.result.nativeGenerated', equals: true },
    { path: 'structuredContent.result.diagnosticsAvailable', equals: true },
    { path: 'structuredContent.result.resources.instanceCount', equals: count },
  ], captureResult: [{ key: 'operation', fromField: 'result.operationId' }] });
const cases = [
  { scenario: 'Setup: create an exclusively owned execution graph', toolName: 'manage_pcg',
    arguments: { action: 'create_pcg_graph', graphPath, save: false }, expected: 'success',
    assertions: [{ path: 'structuredContent.result.created', equals: true }],
    captureResult: [{ key: 'ownedGraph', fromField: 'result.graphPath' }] },
  { scenario: 'Read the empty graph revision before the batch', toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_graph', graphPath, includeSettings: false }, expected: 'success',
    captureResult: [{ key: 'emptyRevision', fromField: 'result.revision' }] },
  { scenario: 'Setup: add the grid and spawner in one revision-checked batch', toolName: 'manage_pcg',
    arguments: { action: 'batch_edit_pcg_graph', graphPath, expectedGraphRevision: captured('emptyRevision'), save: false, operations: [
      { op: 'add_node', settingsClass: 'PCGCreatePointsGridSettings', tempId: 'grid', settings: {
        GridExtents: { x: 500, y: 500, z: 0 }, CellSize: { x: 100, y: 100, z: 100 },
      } },
      { op: 'add_node', settingsClass: 'PCGStaticMeshSpawnerSettings', meshPath: '/Engine/BasicShapes/Cube.Cube', tempId: 'spawner' },
      { op: 'connect', sourceNodeId: 'grid', targetNodeId: 'spawner' },
      { op: 'connect', sourceNodeId: 'spawner', targetNodeId: 'output' },
    ] }, expected: 'success', captureResult: [
      { key: 'grid', fromField: 'result.nodeIds.grid' }, { key: 'readyRevision', fromField: 'result.revision' },
    ] },
  { scenario: 'Setup: spawn the unique PCG owner', toolName: 'control_actor', arguments: { action: 'spawn',
    classPath: '/Script/Engine.StaticMeshActor', meshPath: '/Engine/BasicShapes/Cube.Cube', actorName, scale: { x: 20, y: 20, z: 2 } }, expected: 'success' },
  { scenario: 'Setup: spawn a nonmanaged actor to protect during cleanup', toolName: 'control_actor', arguments: { action: 'spawn',
    classPath: '/Script/Engine.StaticMeshActor', meshPath: '/Engine/BasicShapes/Sphere.Sphere', actorName: nonManaged }, expected: 'success' },
  { scenario: 'Legacy execute still returns a numeric scheduling task ID', toolName: 'manage_pcg',
    arguments: { action: 'execute_pcg_graph', graphPath, actorName, createComponent: true, save: false }, expected: 'success',
    captureResult: [{ key: 'component', fromField: 'result.componentPath' }],
    assertions: [{ path: 'structuredContent.result.taskId', gte: 0 }] },
  generate('Confirm this generation produces 100 instances', 100, { expectedGraphRevision: captured('readyRevision') }),
  { scenario: 'A stale generation preflight refuses before scheduling native work', toolName: 'manage_pcg',
    arguments: { action: 'generate_pcg_graph', componentPath, expectedGraphRevision: captured('emptyRevision'), save: false },
    expected: { condition: 'error', errorPattern: 'STALE_STATE' }, abortOnFailure: true },
  { scenario: 'Stale generation preserves the authored graph revision', toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_graph', graphPath }, expected: 'success', abortOnFailure: true,
    assertions: [{ path: 'structuredContent.result.revision', equals: captured('readyRevision') }] },
  { scenario: 'Stale generation preserves idle products rather than starting another task', toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_component_results', componentPath, sampleLimit: 0 }, expected: 'success', abortOnFailure: true,
    assertions: [{ path: 'structuredContent.result.generating', equals: false },
      { path: 'structuredContent.result.cleaning', equals: false }, { path: 'structuredContent.result.instanceCount', equals: 100 }] },
  { scenario: 'Read the retained operation for its exact component', toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_operation_status', componentPath, operationId: captured('operation') }, expected: 'success',
    assertions: [{ path: 'structuredContent.result.succeeded', equals: true }, { path: 'structuredContent.result.engineStillRunning', equals: false }] },
  { scenario: 'Read bounded managed inventory and world transform samples', toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_component_results', componentPath, offset: 0, limit: 100, sampleLimit: 2 }, expected: 'success',
    assertions: [{ path: 'structuredContent.result.instanceCount', equals: 100 },
      { path: 'structuredContent.result.snapshotKind', equals: 'current_managed_inventory_not_a_completion_signal' }] },
  { scenario: 'Change grid spacing without replacing the component', toolName: 'manage_pcg',
    arguments: { action: 'set_pcg_node_settings', graphPath, nodeId: captured('grid'), settings: { CellSize: { x: 200, y: 200, z: 100 } }, save: false }, expected: 'success' },
  generate('Regeneration replaces 100 instances with 25', 25),
  generate('Repeated fresh generation does not accumulate instances', 25),
  { scenario: 'Explicit cancellation of an already finished operation is a no-op', toolName: 'manage_pcg',
    arguments: { action: 'control_pcg_execution', control: 'cancel', componentPath, operationId: captured('operation') }, expected: 'success',
    consent: { capability: 'manage_pcg.control_pcg_execution', acknowledge: 'explicit' },
    assertions: [{ path: 'structuredContent.result.completed', equals: true }] },
  { scenario: 'Cleanup without consent is refused', toolName: 'manage_pcg',
    arguments: { action: 'cleanup_pcg_component', componentPath, timeoutSeconds: 30, save: false }, consent: null,
    expected: { condition: 'error', errorPattern: 'CONSENT_REQUIRED' } },
  { scenario: 'Refused cleanup leaves the managed instances intact', toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_component_results', componentPath, sampleLimit: 0 }, expected: 'success',
    assertions: [{ path: 'structuredContent.result.instanceCount', equals: 25 }] },
  { scenario: 'Clean only the component managed resources with explicit consent', toolName: 'manage_pcg',
    arguments: { action: 'cleanup_pcg_component', componentPath, timeoutSeconds: 30, save: false }, timeoutMs: 45000, expected: 'success',
    consent: { capability: 'manage_pcg.cleanup_pcg_component', acknowledge: 'explicit' },
    assertions: [{ path: 'structuredContent.result.state', equals: 'succeeded' }, { path: 'structuredContent.result.resources.instanceCount', equals: 0 }] },
  { scenario: 'A nonmanaged actor survives PCG cleanup', toolName: 'inspect', arguments: { action: 'get_actor_details', actorName: nonManaged }, expected: 'success' },
  generate('The cleaned component can generate again', 25),
  { scenario: 'Require native idle before deleting only this run’s fixtures', toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_component_results', componentPath, sampleLimit: 0 }, expected: 'success',
    pollUntil: { timeoutMs: 30000, intervalMs: 1000 }, abortOnFailure: true, assertions: [
      { path: 'structuredContent.result.generating', equals: false },
      { path: 'structuredContent.result.cleaning', equals: false },
    ] },
  { scenario: 'Cleanup: remove only actors created by this run', toolName: 'control_actor',
    arguments: { action: 'delete', actorNames: [actorName, nonManaged] }, expected: 'success',
    consent: { capability: 'control_actor.delete', acknowledge: 'explicit' } },
  { scenario: 'Cleanup: delete only the graph proven created by this run', toolName: 'manage_asset',
    arguments: { action: 'delete', path: captured('ownedGraph') }, expected: 'success',
    consent: { capability: 'asset.delete', acknowledge: 'elevated' } },
];

runToolTests('manage_pcg_execution', cases, {
  isolatedProject: { projectName: process.env.UNREAL_MCP_TEST_PROJECT_NAME, projectDir: process.env.UE_PROJECT_PATH,
    editorBaseDir: process.env.UE_EDITOR_BASE_DIR, engineVersionPrefix: process.env.UNREAL_MCP_TEST_ENGINE_VERSION },
  foldTwins: false,
});
