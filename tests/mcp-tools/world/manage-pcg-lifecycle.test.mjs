import { randomUUID } from 'node:crypto';
import { runToolTests } from '../../test-runner.mjs';

const stamp = `${Date.now()}_${randomUUID().replaceAll('-', '')}`;
const captured = key => '${captured:' + key + '}';
const cases = [{ scenario: 'Create and load only this run’s nonpartitioned lifecycle map', toolName: 'manage_level_structure',
  arguments: { action: 'create_level', levelName: 'Map', levelPath: `/Game/MCP_PCG_Validation/Lifecycle_${stamp}`,
    bCreateWorldPartition: false, bUseExternalActors: false, save: true, loadAfterCreate: true },
  expected: 'success', abortOnFailure: true }];
for (const [label, createScratch] of [['Control', false], ['TransientCache', true]]) {
  const graphPath = `/Game/MCP_PCG_Validation/Lifecycle_${stamp}/${label}`;
  const actorName = `MCP_PCG_Lifecycle_${label}_${stamp}`;
  const componentPath = captured(`${label}Component`);
  const operationId = captured(`${label}Operation`);
  const ownedGraph = captured(`${label}Graph`);
  const movedName = `${actorName}_Moved`;
  const finalComponentPath = label === 'Control' ? `${captured('ControlMovedActor')}.LifecycleComponent` : componentPath;
  cases.push(
    { scenario: `${label}: create an exclusively owned graph`, toolName: 'manage_pcg',
      arguments: { action: 'create_pcg_graph', graphPath, save: false }, expected: 'success', abortOnFailure: true,
      captureResult: [{ key: `${label}Graph`, fromField: 'result.graphPath' }] },
    { scenario: `${label}: capture the empty revision`, toolName: 'manage_pcg',
      arguments: { action: 'get_pcg_graph', graphPath }, expected: 'success', abortOnFailure: true,
      captureResult: [{ key: `${label}Empty`, fromField: 'result.revision' }] },
    { scenario: `${label}: author grid to safe delayed custom node to spawner`, toolName: 'manage_pcg',
      arguments: { action: 'batch_edit_pcg_graph', graphPath, expectedGraphRevision: captured(`${label}Empty`), save: false,
        operations: [
          { op: 'add_node', settingsClass: 'PCGCreatePointsGridSettings', tempId: 'grid', settings: {
            GridExtents: { x: 500, y: 500, z: 0 }, CellSize: { x: 100, y: 100, z: 100 },
          } },
          { op: 'add_node', settingsClass: '/Script/PCGValidation.PCGValidationDelaySettings', tempId: 'delay',
            settings: { DelaySeconds: label === 'Control' ? 4 : 3, bCreateTransientChild: createScratch } },
          { op: 'add_node', settingsClass: 'PCGStaticMeshSpawnerSettings', tempId: 'spawner', meshPath: '/Engine/BasicShapes/Cube.Cube' },
          { op: 'connect', sourceNodeId: 'grid', targetNodeId: 'delay' },
          { op: 'connect', sourceNodeId: 'delay', targetNodeId: 'spawner' },
          { op: 'connect', sourceNodeId: 'spawner', targetNodeId: 'output' },
        ] }, expected: 'success', abortOnFailure: true,
      captureResult: [{ key: `${label}Ready`, fromField: 'result.revision' }] },
    { scenario: `${label}: spawn the unique owner`, toolName: 'control_actor',
      arguments: { action: 'spawn', classPath: '/Script/Engine.StaticMeshActor', meshPath: '/Engine/BasicShapes/Cube.Cube',
        actorName, scale: { x: 20, y: 20, z: 2 } }, expected: 'success', abortOnFailure: true },
    { scenario: `${label}: schedule and retain this exact native operation`, toolName: 'manage_pcg',
      arguments: { action: 'execute_pcg_graph', graphPath, actorName, componentName: 'LifecycleComponent', createComponent: true, save: false },
      expected: 'success', abortOnFailure: true, assertions: [{ path: 'structuredContent.result.operationTracking', equals: true }],
      captureResult: [{ key: `${label}Component`, fromField: 'result.componentPath' }, { key: `${label}Operation`, fromField: 'result.operationId' }] },
    { scenario: `${label}: wait for native idle, not merely observer completion`, toolName: 'manage_pcg',
      arguments: { action: 'get_pcg_operation_status', componentPath, operationId }, expected: 'success',
      pollUntil: { timeoutMs: 15000, intervalMs: 1000 }, abortOnFailure: true, assertions: [
        { path: 'structuredContent.result.completed', equals: true }, { path: 'structuredContent.result.engineStillRunning', equals: false },
      ] },
    { scenario: `${label}: unchanged custom execution must report success`, toolName: 'manage_pcg',
      arguments: { action: 'get_pcg_operation_status', componentPath, operationId }, expected: 'success', assertions: [
        { path: 'structuredContent.result.state', equals: 'succeeded' }, { path: 'structuredContent.result.nativeGenerated', equals: true },
      ] },
    { scenario: `${label}: generation did not edit authored graph state`, toolName: 'manage_pcg',
      arguments: { action: 'get_pcg_graph', graphPath }, expected: 'success',
      assertions: [{ path: 'structuredContent.result.revision', equals: captured(`${label}Ready`) }] },
    { scenario: `${label}: products are real even when observation failed`, toolName: 'manage_pcg',
      arguments: { action: 'get_pcg_component_results', componentPath, sampleLimit: 0 }, expected: 'success',
      assertions: [{ path: 'structuredContent.result.instanceCount', equals: 100 }] },
    { scenario: `${label}: observation timeout does not claim native cancellation`, toolName: 'manage_pcg',
      arguments: { action: 'generate_pcg_graph', componentPath, timeoutSeconds: 0.1, save: false }, timeoutMs: 10000,
      expected: { condition: 'error', errorPattern: 'PCG_OPERATION_TIMEOUT' }, abortOnFailure: true,
      captureResult: [{ key: `${label}TimedOut`, fromField: 'result.result.operationId' },
        { key: `${label}TimedOut`, fromField: 'typedError.unrealDetail.operationId' }] },
    { scenario: `${label}: timed-out observer still owns the busy component lease`, toolName: 'manage_pcg',
      arguments: { action: 'generate_pcg_graph', componentPath, timeoutSeconds: 5, save: false },
      expected: { condition: 'error', errorPattern: 'PCG_COMPONENT_BUSY' }, abortOnFailure: true },
    { scenario: `${label}: retain the timeout outcome after real late completion`, toolName: 'manage_pcg',
      arguments: { action: 'get_pcg_operation_status', componentPath, operationId: captured(`${label}TimedOut`) }, expected: 'success',
      pollUntil: { timeoutMs: 15000, intervalMs: 1000 }, abortOnFailure: true, assertions: [
        { path: 'structuredContent.result.completed', equals: true }, { path: 'structuredContent.result.engineStillRunning', equals: false },
        { path: 'structuredContent.result.state', equals: 'timed_out' }, { path: 'structuredContent.result.nativeGenerated', equals: true },
        { path: 'structuredContent.result.lateCompletionObserved', equals: true },
      ] },
    ...(label === 'Control' ? [
      { scenario: 'Superseded: schedule a delayed native operation before an external editor edit', toolName: 'manage_pcg',
        arguments: { action: 'execute_pcg_graph', graphPath, actorName, componentName: 'LifecycleComponent', createComponent: false, save: false },
        expected: 'success', abortOnFailure: true,
        captureResult: [{ key: 'ControlSupersededOperation', fromField: 'result.operationId' },
          { key: 'ControlSupersededTask', fromField: 'result.taskId' }] },
      { scenario: 'Superseded: schedule a different real native task in the same editor turn', toolName: 'system_control',
        arguments: { action: 'execute_python', code: `import unreal
assert unreal.SystemLibrary.get_game_name() == 'PCGValidation'
component = unreal.find_object(None, '${componentPath}')
assert component is not None and component.get_class() == unreal.PCGComponent.static_class()
replacement = unreal.PCGValidationDelaySettings.replace_in_flight_generation(component)
assert replacement >= 0 and replacement != int('${captured('ControlSupersededTask')}')
` }, expected: 'success', abortOnFailure: true },
      { scenario: 'Superseded: do not attribute replacement-generation output to the original request', toolName: 'manage_pcg',
        arguments: { action: 'get_pcg_operation_status', componentPath, operationId: captured('ControlSupersededOperation') }, expected: 'success',
        pollUntil: { timeoutMs: 15000, intervalMs: 1000 }, abortOnFailure: true, assertions: [
          { path: 'structuredContent.result.completed', equals: true }, { path: 'structuredContent.result.engineStillRunning', equals: false },
          { path: 'structuredContent.result.state', equals: 'superseded' }, { path: 'structuredContent.result.operationErrorCode', equals: 'PCG_OPERATION_SUPERSEDED' },
          { path: 'structuredContent.result.nativeCancelled', equals: true },
          { path: 'structuredContent.result.nativeGenerated', equals: true },
          { path: 'structuredContent.result.lateCompletionObserved', equals: true },
        ] },
      { scenario: 'Superseded: native replacement did not edit authored graph inputs', toolName: 'manage_pcg',
        arguments: { action: 'get_pcg_graph', graphPath, limit: 1, edgeLimit: 1 }, expected: 'success', abortOnFailure: true,
        assertions: [{ path: 'structuredContent.result.revision', equals: captured(`${label}Ready`) }] },
      { scenario: 'Superseded: prove native idle and real replacement products', toolName: 'manage_pcg',
        arguments: { action: 'get_pcg_component_results', componentPath, sampleLimit: 0 }, expected: 'success',
        pollUntil: { timeoutMs: 15000, intervalMs: 1000 }, abortOnFailure: true, assertions: [
          { path: 'structuredContent.result.generating', equals: false }, { path: 'structuredContent.result.cleaning', equals: false },
          { path: 'structuredContent.result.instanceCount', equals: 100 },
        ] },
      { scenario: 'Moved target: schedule another real delayed operation', toolName: 'manage_pcg',
        arguments: { action: 'execute_pcg_graph', graphPath, actorName, componentName: 'LifecycleComponent', createComponent: false, save: false },
        expected: 'success', abortOnFailure: true,
        captureResult: [{ key: 'ControlMovedOperation', fromField: 'result.operationId' }] },
      { scenario: 'Moved target: rename only the owned nonpartitioned actor object during generation', toolName: 'control_actor',
        arguments: { action: 'rename', actorName, newName: movedName, renameObject: true }, expected: 'success', abortOnFailure: true,
        assertions: [{ path: 'structuredContent.result.objectName', equals: movedName }],
        captureResult: [{ key: 'ControlMovedActor', fromField: 'result.actorPath' }] },
      { scenario: 'Moved target: retain original operation identity until actual native idle', toolName: 'manage_pcg',
        arguments: { action: 'get_pcg_operation_status', componentPath, operationId: captured('ControlMovedOperation') }, expected: 'success',
        pollUntil: { timeoutMs: 15000, intervalMs: 1000 }, abortOnFailure: true, assertions: [
          { path: 'structuredContent.result.completed', equals: true }, { path: 'structuredContent.result.engineStillRunning', equals: false },
          { path: 'structuredContent.result.state', equals: 'target_moved' }, { path: 'structuredContent.result.operationErrorCode', equals: 'PCG_TARGET_MOVED' },
        ] },
    ] : []),
    { scenario: `${label}: current exact target must be native idle before cleanup is attempted`, toolName: 'manage_pcg',
      arguments: { action: 'get_pcg_component_results', componentPath: finalComponentPath, sampleLimit: 0 }, expected: 'success',
      pollUntil: { timeoutMs: 15000, intervalMs: 1000 }, abortOnFailure: true, assertions: [
        { path: 'structuredContent.result.generating', equals: false }, { path: 'structuredContent.result.cleaning', equals: false },
      ] },
    { scenario: `${label}: cleanup owns only this component's resources`, toolName: 'manage_pcg',
      arguments: { action: 'cleanup_pcg_component', componentPath: finalComponentPath, timeoutSeconds: 15, save: false }, timeoutMs: 20000,
      expected: 'success', abortOnFailure: true, consent: { capability: 'manage_pcg.cleanup_pcg_component', acknowledge: 'explicit' } },
    { scenario: `${label}: require idle and empty before deleting owned objects`, toolName: 'manage_pcg',
      arguments: { action: 'get_pcg_component_results', componentPath: finalComponentPath, sampleLimit: 0 }, expected: 'success',
      pollUntil: { timeoutMs: 15000, intervalMs: 1000 }, abortOnFailure: true, assertions: [
        { path: 'structuredContent.result.generating', equals: false }, { path: 'structuredContent.result.cleaning', equals: false },
        { path: 'structuredContent.result.instanceCount', equals: 0 },
      ] },
    { scenario: `${label}: remove the owned idle actor`, toolName: 'control_actor',
      arguments: { action: 'delete', actorNames: [label === 'Control' ? movedName : actorName] }, expected: 'success', abortOnFailure: true,
      consent: { capability: 'control_actor.delete', acknowledge: 'explicit' } },
    { scenario: `${label}: remove only the graph proven created above`, toolName: 'manage_asset',
      arguments: { action: 'delete', path: ownedGraph }, expected: 'success',
      consent: { capability: 'asset.delete', acknowledge: 'elevated' } },
  );
}

runToolTests('manage_pcg_lifecycle', cases, {
  isolatedProject: { projectName: process.env.UNREAL_MCP_TEST_PROJECT_NAME, projectDir: process.env.UE_PROJECT_PATH,
    editorBaseDir: process.env.UE_EDITOR_BASE_DIR, engineVersionPrefix: process.env.UNREAL_MCP_TEST_ENGINE_VERSION },
  foldTwins: false,
});
