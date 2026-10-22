import { lstatSync, readFileSync, realpathSync, statSync } from 'node:fs';
import path from 'node:path';
import process from 'node:process';
import { runToolTests } from '../../test-runner.mjs';

const ASSET_ROOT = '/Game/MCP_PCG_Validation/';
const MANIFEST_ENV = 'UNREAL_MCP_PCG_COMPONENT_PERSISTENCE_MANIFEST';
const MAX_MANIFEST_BYTES = 64 * 1024;
const isStaticAudit = Array.isArray(globalThis.__capturedToolSuites);
const staticAuditManifest = {
  runRoot: '/Game/MCP_PCG_Validation/ComponentSave_StaticAudit',
  mapPath: '/Game/MCP_PCG_Validation/ComponentSave_StaticAudit/Map',
  graphPath: '/Game/MCP_PCG_Validation/ComponentSave_StaticAudit/Graph',
  actorName: 'MCP_PCG_Save_StaticAudit',
  componentAName: 'MCP_PCG_Save_A_StaticAudit',
  componentBName: 'MCP_PCG_Save_B_StaticAudit',
  componentAPath: '/Game/MCP_PCG_Validation/ComponentSave_StaticAudit/Map.Map:PersistentLevel.StaticMeshActor_0.MCP_PCG_Save_A_StaticAudit',
  componentBPath: '/Game/MCP_PCG_Validation/ComponentSave_StaticAudit/Map.Map:PersistentLevel.StaticMeshActor_0.MCP_PCG_Save_B_StaticAudit',
  pointNodeId: 'points',
};
const expectedManifestKeys = [
  'runRoot', 'mapPath', 'graphPath', 'actorName', 'componentAName', 'componentBName',
  'componentAPath', 'componentBPath', 'pointNodeId',
];

function requireText(value, field) {
  if (typeof value !== 'string' || value.trim() !== value || value.length === 0 ||
      Array.from(value, character => character.charCodeAt(0)).some(code => code <= 0x1f || code === 0x7f)) {
    throw new Error(`PCG component persistence manifest has an invalid ${field}.`);
  }
  return value;
}

function requireSafeAssetPath(value, field) {
  const assetPath = requireText(value, field);
  if (!assetPath.startsWith(ASSET_ROOT) || assetPath.includes('\\') || assetPath.includes('//') || assetPath.endsWith('/') ||
      assetPath.split('/').some(segment => segment === '.' || segment === '..')) {
    throw new Error(`PCG component persistence manifest ${field} is outside ${ASSET_ROOT}.`);
  }
  return assetPath;
}

function validateManifest(value) {
  if (!value || typeof value !== 'object' || Array.isArray(value)) {
    throw new Error('PCG component persistence manifest must be a JSON object.');
  }
  const keys = Object.keys(value).sort();
  if (keys.length !== expectedManifestKeys.length || keys.some((key, index) => key !== [...expectedManifestKeys].sort()[index])) {
    throw new Error('PCG component persistence manifest must contain exactly the agreed fields.');
  }
  const manifest = {
    runRoot: requireSafeAssetPath(value.runRoot, 'runRoot'),
    mapPath: requireSafeAssetPath(value.mapPath, 'mapPath'),
    graphPath: requireSafeAssetPath(value.graphPath, 'graphPath'),
    actorName: requireText(value.actorName, 'actorName'),
    componentAName: requireText(value.componentAName, 'componentAName'),
    componentBName: requireText(value.componentBName, 'componentBName'),
    componentAPath: requireSafeAssetPath(value.componentAPath, 'componentAPath'),
    componentBPath: requireSafeAssetPath(value.componentBPath, 'componentBPath'),
    pointNodeId: requireText(value.pointNodeId, 'pointNodeId'),
  };
  if (!/^\/Game\/MCP_PCG_Validation\/ComponentSave_[A-Za-z0-9_]+$/.test(manifest.runRoot) ||
      manifest.mapPath !== `${manifest.runRoot}/Map` || manifest.graphPath !== `${manifest.runRoot}/Graph`) {
    throw new Error('PCG component persistence assets must be the Map and Graph inside one unique ComponentSave run root.');
  }
  for (const [field, name] of [
    ['actorName', manifest.actorName], ['componentAName', manifest.componentAName], ['componentBName', manifest.componentBName],
  ]) {
    if (!/^[A-Za-z0-9_]+$/.test(name)) throw new Error(`PCG component persistence manifest ${field} is not a safe label.`);
  }
  if (manifest.componentAName === manifest.componentBName || manifest.componentAPath === manifest.componentBPath) {
    throw new Error('PCG component persistence manifest must identify two distinct components.');
  }
  if (!/^[A-Za-z0-9_-]+$/.test(manifest.pointNodeId)) {
    throw new Error('PCG component persistence manifest pointNodeId is not a graph node identifier.');
  }
  const mapObjectPrefix = `${manifest.mapPath}.Map:PersistentLevel.`;
  for (const [field, componentName] of [
    ['componentAPath', manifest.componentAName], ['componentBPath', manifest.componentBName],
  ]) {
    const componentPath = manifest[field];
    if (!componentPath.startsWith(`${manifest.runRoot}/`) || !componentPath.startsWith(mapObjectPrefix)) {
      throw new Error(`PCG component persistence manifest ${field} is not an object in the selected map.`);
    }
    const objectSegments = componentPath.slice(mapObjectPrefix.length).split('.');
    if (objectSegments.length !== 2 || !objectSegments[0] || objectSegments[1] !== componentName) {
      throw new Error(`PCG component persistence manifest ${field} must end at its exact component name.`);
    }
  }
  return manifest;
}

function pathIsInside(paths, parent, child) {
  const relative = paths.relative(parent, child);
  return relative !== '' && relative !== '..' && !relative.startsWith(`..${paths.sep}`) && !paths.isAbsolute(relative);
}

function readPersistenceManifest() {
  const projectDir = requireText(process.env.UE_PROJECT_PATH, 'UE_PROJECT_PATH');
  const paths = /^[A-Za-z]:[\\/]/.test(projectDir) ? path.win32 : path.posix;
  if (!paths.isAbsolute(projectDir)) throw new Error('UE_PROJECT_PATH must be absolute for PCG component persistence validation.');
  const projectRoot = /\.uproject$/i.test(projectDir) ? paths.dirname(projectDir) : projectDir;
  const savedRoot = paths.resolve(projectRoot, 'Saved', 'PCGValidation');
  const manifestPathInput = requireText(process.env[MANIFEST_ENV], MANIFEST_ENV);
  if (!paths.isAbsolute(manifestPathInput) || paths.extname(manifestPathInput).toLowerCase() !== '.json') {
    throw new Error(`Set ${MANIFEST_ENV} to a JSON file under the selected project Saved/PCGValidation directory.`);
  }
  if (manifestPathInput.split(/[\\/]+/).some(segment => segment === '.' || segment === '..')) {
    throw new Error('PCG component persistence manifest paths cannot contain traversal segments.');
  }
  const lexicalManifestPath = paths.resolve(manifestPathInput);
  const lexicalRelativePath = paths.relative(savedRoot, lexicalManifestPath);
  if (!pathIsInside(paths, savedRoot, lexicalManifestPath)) {
    throw new Error('PCG component persistence manifest path is outside the selected project Saved/PCGValidation directory.');
  }
  let resolvedSavedRoot;
  let resolvedManifestPath;
  try {
    for (const directory of [projectRoot, paths.join(projectRoot, 'Saved'), savedRoot]) {
      const info = lstatSync(directory);
      if (!info.isDirectory() || info.isSymbolicLink()) throw new Error('linked project evidence root');
      if (paths.resolve(realpathSync(directory)).toLowerCase() !== paths.resolve(directory).toLowerCase()) {
        throw new Error('non-canonical project evidence root');
      }
    }
    resolvedSavedRoot = realpathSync(savedRoot);
    resolvedManifestPath = realpathSync(lexicalManifestPath);
  } catch {
    throw new Error('PCG component persistence manifest or its Saved/PCGValidation directory is missing.');
  }
  if (!pathIsInside(paths, resolvedSavedRoot, resolvedManifestPath)) {
    throw new Error('PCG component persistence manifest resolves outside the selected project Saved/PCGValidation directory.');
  }
  let cursor = resolvedSavedRoot;
  const segments = lexicalRelativePath.split(paths.sep);
  try {
    for (let index = 0; index < segments.length; index += 1) {
      cursor = paths.join(cursor, segments[index]);
      const linkInfo = lstatSync(cursor);
      if (linkInfo.isSymbolicLink()) throw new Error('link');
      if (index < segments.length - 1 && !linkInfo.isDirectory()) throw new Error('not a directory');
      if (index === segments.length - 1 && !linkInfo.isFile()) throw new Error('not a regular file');
    }
    const fileInfo = statSync(resolvedManifestPath);
    if (!fileInfo.isFile() || fileInfo.size > MAX_MANIFEST_BYTES) throw new Error('invalid size or type');
  } catch {
    throw new Error('PCG component persistence manifest must be a bounded regular file without symlink or junction path segments.');
  }
  let decoded;
  try {
    decoded = JSON.parse(readFileSync(resolvedManifestPath, 'utf8'));
  } catch {
    throw new Error('PCG component persistence manifest is not valid JSON.');
  }
  return validateManifest(decoded);
}

const manifest = validateManifest(isStaticAudit ? staticAuditManifest : readPersistenceManifest());
const captured = key => '${captured:' + key + '}';
const authoredRevision = captured('authoredRevision');
const datasetId = captured('rootDataset');
const dataViewId = captured('pointView');
const operationId = captured('generationOperation');
const graphObjectPath = `${manifest.graphPath}.Graph`;
const componentASeed = 77;
const componentBSeed = 83;
const pointA = {
  location: { x: 123, y: -234, z: 45 }, rotation: { x: 0, y: 0, z: 0.6, w: 0.8 },
  scale: { x: 1.25, y: 0.75, z: 1.5 }, density: 0.3125, seed: -73,
};
const pointB = {
  location: { x: 613, y: -727, z: 89 }, rotation: { x: 0, y: 0, z: -0.8, w: 0.6 },
  scale: { x: 0.5, y: 1.75, z: 2.25 }, density: 0.8125, seed: 911,
};

const pcg = (scenario, arguments_, extra = {}) => ({
  scenario, toolName: 'manage_pcg', arguments: arguments_, expected: 'success', abortOnFailure: true, ...extra,
});
const inspect = (scenario, arguments_, extra = {}) => ({
  scenario, toolName: 'inspect', arguments: arguments_, expected: 'success', abortOnFailure: true, ...extra,
});
const actor = (scenario, arguments_, extra = {}) => ({
  scenario, toolName: 'control_actor', arguments: arguments_, expected: 'success', abortOnFailure: true, ...extra,
});
const noOutputAssertions = [
  { path: 'structuredContent.result.generating', equals: false },
  { path: 'structuredContent.result.cleaning', equals: false },
  { path: 'structuredContent.result.instanceCount', equals: 0 },
  { path: 'structuredContent.result.outputDataCount', equals: 0 },
  { path: 'structuredContent.result.outputPointCount', equals: 0 },
  { path: 'structuredContent.result.outputs', length: 0 },
];
const emptyRootAssertions = [
  { path: 'structuredContent.result.items', length: 0 },
  { path: 'structuredContent.result.total', equals: 0 },
];
const graphRevisionRead = scenario => pcg(scenario, {
  action: 'get_pcg_graph', graphPath: manifest.graphPath, includeSettings: true, limit: 8, edgeLimit: 8,
}, { assertions: [
  { path: 'structuredContent.result.revision', equals: authoredRevision },
  { path: 'structuredContent.result.items', length: 3 },
  { path: 'structuredContent.result.items.1.nodeId', equals: manifest.pointNodeId },
  { path: 'structuredContent.result.items.1.settingsClass', equals: '/Script/PCG.PCGCreatePointsSettings' },
  { path: 'structuredContent.result.items.1.settings.bCullPointsOutsideVolume', equals: false },
  { path: 'structuredContent.result.edges', length: 1 },
  { path: 'structuredContent.result.edges', includesObject: {
    sourceNodeId: manifest.pointNodeId, sourcePin: 'Out', targetNodeId: captured('graphOutput'), targetPin: captured('graphOutputPin'),
  } },
] });
const componentAssertions = (expectedSeed) => [
  { path: 'structuredContent.result.properties', length: 5 },
  { path: 'structuredContent.result.properties', includesObject: {
    propertyName: 'Graph', propertyPath: 'GraphInstance.Graph', value: graphObjectPath,
  } },
  { path: 'structuredContent.result.properties', includesObject: { propertyName: 'GenerationTrigger', value: 'GenerateOnDemand' } },
  { path: 'structuredContent.result.properties', includesObject: { propertyName: 'bRegenerateInEditor', value: false } },
  { path: 'structuredContent.result.properties', includesObject: { propertyName: 'bActivated', value: true } },
  { path: 'structuredContent.result.properties', includesObject: { propertyName: 'Seed', value: expectedSeed } },
];
const componentInventory = (scenario, name, objectPath) => inspect(scenario, {
  action: 'get_components', actorName: manifest.actorName, componentNames: [name],
}, { assertions: [
  { path: 'structuredContent.result.components', length: 1 },
  { path: 'structuredContent.result.components.0.name', equals: name },
  { path: 'structuredContent.result.components.0.class', equals: '/Script/PCG.PCGComponent' },
  { path: 'structuredContent.result.components.0.path', equals: objectPath },
] });
const componentPropertyRead = (scenario, objectPath, expectedSeed) => inspect(scenario, {
  action: 'get_property', objectPath,
  propertyNames: ['GraphInstance.Graph', 'GenerationTrigger', 'bRegenerateInEditor', 'bActivated', 'Seed'],
}, { assertions: componentAssertions(expectedSeed) });
const emptyReads = (label, componentPath, pollUntilIdle = false) => [
  pcg(`Prove ${label} has no managed instances, generation work, or outputs`, {
    action: 'get_pcg_component_results', componentPath, limit: 8, sampleLimit: 0,
  }, {
    ...(pollUntilIdle ? { pollUntil: { timeoutMs: 20000, intervalMs: 1000 } } : {}),
    assertions: noOutputAssertions,
  }),
  pcg(`Prove ${label} has no root datasets`, {
    action: 'list_pcg_data', componentPath, offset: 0, limit: 4,
  }, { assertions: emptyRootAssertions }),
];
const pointTransformAssertions = (row, point) => [
  ...['x', 'y', 'z'].map(axis => ({
    path: `structuredContent.result.rows.${row}.fields.transform.location.${axis}`,
    approximately: point.location[axis], tolerance: 0.0001,
  })),
  ...['x', 'y', 'z', 'w'].map(axis => ({
    path: `structuredContent.result.rows.${row}.fields.transform.rotation.${axis}`,
    approximately: point.rotation[axis], tolerance: 0.000001,
  })),
  ...['x', 'y', 'z'].map(axis => ({
    path: `structuredContent.result.rows.${row}.fields.transform.scale.${axis}`,
    approximately: point.scale[axis], tolerance: 0.000001,
  })),
  { path: `structuredContent.result.rows.${row}.fields.density`, approximately: point.density, tolerance: 0.000001 },
  { path: `structuredContent.result.rows.${row}.fields.seed`, equals: point.seed },
];

const cases = [
  {
    scenario: 'Load only the manifest-owned map without saving or discarding dirty packages',
    toolName: 'manage_level',
    arguments: {
      action: 'load', levelPath: manifest.mapPath,
      saveDirtyPackages: false, discardUnsaved: false, streaming: false,
    },
    expected: 'success', abortOnFailure: true, timeoutMs: 30000,
    assertions: [
      { path: 'structuredContent.result.loadedPath', equals: manifest.mapPath },
      { path: 'structuredContent.result.alreadyLoaded', equals: false },
      { path: 'structuredContent.result.reloaded', equals: true },
    ],
  },
  {
    scenario: 'Verify the cold-loaded graph structure, native points settings class, scalar setting, and saved revision',
    toolName: 'manage_pcg',
    arguments: { action: 'get_pcg_graph', graphPath: manifest.graphPath, includeSettings: true, limit: 8, edgeLimit: 8 },
    expected: 'success', abortOnFailure: true,
    assertions: [
      { path: 'structuredContent.result.items', length: 3 },
      { path: 'structuredContent.result.items.0.input', equals: true },
      { path: 'structuredContent.result.items.1.nodeId', equals: manifest.pointNodeId },
      { path: 'structuredContent.result.items.1.settingsClass', equals: '/Script/PCG.PCGCreatePointsSettings' },
      { path: 'structuredContent.result.items.1.settings.bCullPointsOutsideVolume', equals: false },
      { path: 'structuredContent.result.items.2.output', equals: true },
      { path: 'structuredContent.result.items.2.inputPins', minLength: 1 },
      { path: 'structuredContent.result.edges', length: 1 },
    ],
    captureResult: [
      { key: 'authoredRevision', fromField: 'result.revision' },
      { key: 'graphOutput', fromField: 'result.items.2.nodeId' },
      { key: 'graphOutputPin', fromField: 'result.items.2.inputPins.0.label' },
    ],
  },
  componentInventory('Prove the manifest-owned first component is the exact native component owned by the manifest actor label', manifest.componentAName, manifest.componentAPath),
  componentInventory('Prove the manifest-owned second component is the exact native component owned by the manifest actor label', manifest.componentBName, manifest.componentBPath),
  componentPropertyRead('Verify the first cold-loaded full graph reference, manual flags, and distinct component seed', manifest.componentAPath, componentASeed),
  componentPropertyRead('Verify the second cold-loaded full graph reference, manual flags, and distinct component seed', manifest.componentBPath, componentBSeed),
  graphRevisionRead('Prove cold-loaded component binding reads preserved the captured graph revision'),
  ...emptyReads('the first cold-loaded component before explicit generation', manifest.componentAPath),
  ...emptyReads('the second cold-loaded component before explicit generation', manifest.componentBPath),
  graphRevisionRead('Prove pre-generation reads preserved the original cold-loaded graph revision'),
  pcg('Explicitly generate only the first exact bound component with its captured graph revision', {
    action: 'generate_pcg_graph', componentPath: manifest.componentAPath, force: true,
    timeoutSeconds: 25, expectedGraphRevision: authoredRevision, save: false,
  }, { timeoutMs: 30000, captureResult: [{ key: 'generationOperation', fromField: 'result.operationId' }] }),
  pcg('Poll the supported native operation status until first-component generation is terminal', {
    action: 'get_pcg_operation_status', componentPath: manifest.componentAPath, operationId,
  }, {
    timeoutMs: 30000, pollUntil: { timeoutMs: 25000, intervalMs: 1000 },
    assertions: [
      { path: 'structuredContent.result.completed', equals: true },
      { path: 'structuredContent.result.succeeded', equals: true },
      { path: 'structuredContent.result.engineStillRunning', equals: false },
      { path: 'structuredContent.result.state', equals: 'succeeded' },
      { path: 'structuredContent.result.nativeGenerated', equals: true },
    ],
  }),
  pcg('Verify native generation produced exactly two points on the first component', {
    action: 'get_pcg_component_results', componentPath: manifest.componentAPath, limit: 8, sampleLimit: 0,
  }, { assertions: [
    { path: 'structuredContent.result.generating', equals: false },
    { path: 'structuredContent.result.cleaning', equals: false },
    { path: 'structuredContent.result.instanceCount', equals: 0 },
    { path: 'structuredContent.result.outputDataCount', equals: 1 },
    { path: 'structuredContent.result.outputPointCount', equals: 2 },
    { path: 'structuredContent.result.outputs', length: 1 },
    { path: 'structuredContent.result.outputs.0.pin', equals: captured('graphOutputPin') },
    { path: 'structuredContent.result.outputs.0.pointCount', equals: 2 },
  ] }),
  pcg('Discover the exact first-component root dataset for typed readback', {
    action: 'list_pcg_data', componentPath: manifest.componentAPath, offset: 0, limit: 4,
  }, {
    assertions: [
      { path: 'structuredContent.result.items', length: 1 },
      { path: 'structuredContent.result.total', equals: 1 },
      { path: 'structuredContent.result.items.0.available', equals: true },
      { path: 'structuredContent.result.items.0.pin', equals: captured('graphOutputPin') },
    ],
    captureResult: [{ key: 'rootDataset', fromField: 'result.items.0.datasetId' }],
  }),
  pcg('Create a bounded typed view of the first component’s transforms, densities, and point seeds', {
    action: 'inspect_pcg', view: 'create_data_view', componentPath: manifest.componentAPath, datasetId,
    fields: ['transform', 'density', 'seed'], attributes: [], sourceOffset: 0, sourceLimit: 2,
  }, {
    assertions: [
      { path: 'structuredContent.result.projection.domain', equals: 'Elements' },
      { path: 'structuredContent.result.projection.sourceTotal', equals: 2 },
      { path: 'structuredContent.result.projection.rowCount', equals: 2 },
      { path: 'structuredContent.result.projection.fields', length: 3 },
    ],
    captureResult: [{ key: 'pointView', fromField: 'result.viewId' }],
  }),
  pcg('Verify every typed point location, quaternion, scale, density, and seed after cold load', {
    action: 'read_pcg_data', componentPath: manifest.componentAPath, viewId: dataViewId, offset: 0, limit: 2,
  }, { assertions: [
    { path: 'structuredContent.result.rows', length: 2 },
    { path: 'structuredContent.result.rows.0.sourceIndex', equals: 0 },
    ...pointTransformAssertions(0, pointA),
    { path: 'structuredContent.result.rows.1.sourceIndex', equals: 1 },
    ...pointTransformAssertions(1, pointB),
  ] }),
  ...emptyReads('the second component after first-only generation', manifest.componentBPath),
  pcg('Release the retained typed data view before cleaning generated resources', {
    action: 'inspect_pcg', view: 'release_data_view', componentPath: manifest.componentAPath, handleId: dataViewId,
  }, { assertions: [{ path: 'structuredContent.result.released', equals: true }] }),
  pcg('Release the retained root dataset handle before cleaning generated resources', {
    action: 'inspect_pcg', view: 'release_data_view', componentPath: manifest.componentAPath, handleId: datasetId,
  }, { assertions: [{ path: 'structuredContent.result.released', equals: true }] }),
  pcg('Clean only the first component at the captured cold-loaded graph revision', {
    action: 'control_pcg_execution', control: 'cleanup', componentPath: manifest.componentAPath,
    expectedGraphRevision: authoredRevision, timeoutSeconds: 25, save: false,
  }, {
    timeoutMs: 30000, consent: { capability: 'manage_pcg.control_pcg_execution', acknowledge: 'explicit' },
    assertions: [
      { path: 'structuredContent.result.completed', equals: true },
      { path: 'structuredContent.result.state', equals: 'succeeded' },
      { path: 'structuredContent.result.engineStillRunning', equals: false },
      { path: 'structuredContent.result.nativeCleaned', equals: true },
    ],
  }),
  graphRevisionRead('Prove first-component cleanup preserved the originally captured graph revision'),
  ...[
    ['first', manifest.componentAPath], ['second', manifest.componentBPath],
  ].flatMap(([label, componentPath]) => emptyReads(`${label} component after cleanup`, componentPath, true)),
  actor('Cleanup: delete only the manifest-owned actor after confirming both exact component paths and owners', {
    action: 'delete', actorNames: [manifest.actorName],
  }, { consent: { capability: 'control_actor.delete', acknowledge: 'explicit' } }),
  {
    scenario: 'Save only the owned map after deleting its graph-bound actor',
    toolName: 'manage_level', arguments: { action: 'save', levelPath: manifest.mapPath },
    expected: 'success', abortOnFailure: true,
    assertions: [{ path: 'structuredContent.result.savedAssetPath', equals: manifest.mapPath }],
  },
  graphRevisionRead('Prove saving the actor-free owned map preserved the graph revision'),
  {
    scenario: 'Cleanup: delete only the exact manifest-owned native graph after saving the actor-free map',
    toolName: 'manage_asset', arguments: { action: 'delete', path: manifest.graphPath },
    expected: 'success', abortOnFailure: true,
    consent: { capability: 'asset.delete', acknowledge: 'elevated' },
  },
];

// The main lifecycle driver unloads and deletes the owned empty map only after
// independently checking references and current-map state.
runToolTests('manage_pcg_component_cold_load', cases, {
  isolatedProject: {
    projectName: process.env.UNREAL_MCP_TEST_PROJECT_NAME,
    projectDir: process.env.UE_PROJECT_PATH,
    editorBaseDir: process.env.UE_EDITOR_BASE_DIR,
    engineVersionPrefix: process.env.UNREAL_MCP_TEST_ENGINE_VERSION,
  },
});
