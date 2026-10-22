import { randomUUID } from 'node:crypto';
import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StdioClientTransport } from '@modelcontextprotocol/sdk/client/stdio.js';
import { assertIsolatedProject } from './test-runner-isolation.mjs';
import { resolveResponsePath } from './test-runner-response-utils.mjs';
import { createToolCaller, normalizeMcpResponse } from './test-runner.mjs';

// Assumptions to verify when the live harness runs:
// - The normal selected-project token fallback can verify editor identity before scoped tests.
// - UNREAL_MCP_TEST_PROJECT_NAME is the selected editor project's expected name.
// - The manifest's nodeId is a PCG node whose Seed setting is writable.
// - The fixture's delayed graph remains generating long enough to exercise cross-principal checks.
// - get_pcg_component_results exposes activeOperationId to its owner while generating.

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const distCli = path.join(repoRoot, 'dist', 'cli.js');
const PROFILE_NAMES = ['read', 'write', 'destructive', 'owner', 'other', 'wrongProject'];
const CALL_TIMEOUT_MS = 60_000;
const ACTIVE_OPERATION_WAIT_MS = 6_000;
const IDLE_WAIT_MS = 30_000;
const ownedClients = [];
const clientCache = new Map();
let passedAssertions = 0;
let failedAssertions = 0;

class AssertionFailure extends Error {}

function assertion(name, condition) {
  if (!condition) {
    failedAssertions += 1;
    console.log(`FAIL: ${name}`);
    throw new AssertionFailure();
  }
  passedAssertions += 1;
  console.log(`PASS: ${name}`);
}

function isWithin(root, candidate) {
  const relative = path.relative(root, candidate);
  return relative === '' || (!path.isAbsolute(relative) && relative !== '..' && !relative.startsWith(`..${path.sep}`));
}

function hasTraversal(value) {
  return /(?:^|\/)(?:\.|\.\.)(?:\/|$)/.test(value) || value.includes('\\') || value.includes('\0');
}

function isAssetPathUnderRoot(value, root) {
  return typeof value === 'string' && !hasTraversal(value) &&
    (value === root || value.startsWith(`${root}/`) || value.startsWith(`${root}.`) || value.startsWith(`${root}:`));
}

function actorNameFromComponentPath(componentPath) {
  const objectPath = componentPath.split(':').at(-1) ?? componentPath;
  const parts = objectPath.split('.').filter(Boolean);
  if (parts.length >= 3) return parts.at(-2);
  if (parts.length === 2) return parts[0].split('/').at(-1);
  return undefined;
}

async function loadConfig() {
  const selectedProjectPath = process.env.UE_PROJECT_PATH;
  const manifestPath = process.env.UNREAL_MCP_PCG_AUTH_MANIFEST;
  if (!selectedProjectPath || !manifestPath || !process.env.UNREAL_MCP_TEST_PROJECT_NAME) {
    throw new Error('Required selection environment is missing.');
  }

  const selectedRealPath = await fs.realpath(selectedProjectPath);
  const projectRoot = selectedRealPath.toLowerCase().endsWith('.uproject')
    ? path.dirname(selectedRealPath)
    : selectedRealPath;
  const savedRoot = await fs.realpath(path.join(projectRoot, 'Saved'));
  const validationDir = await fs.realpath(path.join(projectRoot, 'Saved', 'PCGValidation'));
  if (!isWithin(projectRoot, savedRoot) || !isWithin(savedRoot, validationDir)) {
    throw new Error('Selected validation directory is outside the chosen project.');
  }

  const manifestRealPath = await fs.realpath(manifestPath);
  if (!isWithin(validationDir, manifestRealPath) || manifestRealPath === validationDir) {
    throw new Error('Manifest is outside the selected Saved/PCGValidation directory.');
  }

  let manifest;
  try {
    manifest = JSON.parse(await fs.readFile(manifestRealPath, 'utf8'));
  } catch {
    throw new Error('Manifest could not be read as JSON.');
  }

  const expectedRootPattern = /^\/Game\/MCP_PCG_Validation\/Auth_[A-Za-z0-9_-]+$/;
  if (!manifest || typeof manifest !== 'object' || !expectedRootPattern.test(manifest.root ?? '')) {
    throw new Error('Manifest root does not match the isolated PCG authorization fixture.');
  }
  for (const field of ['mapPath', 'graphPath', 'componentPath']) {
    if (!isAssetPathUnderRoot(manifest[field], manifest.root) || !manifest[field].startsWith(`${manifest.root}/`)) {
      throw new Error('Manifest asset paths must remain inside its unique root.');
    }
  }
  const componentMatchesMap = manifest.componentPath.startsWith(`${manifest.mapPath}.`) ||
    manifest.componentPath.startsWith(`${manifest.mapPath}:`);
  if (!componentMatchesMap || typeof manifest.nodeId !== 'string' || !manifest.nodeId.trim()) {
    throw new Error('Manifest component, map, or node identity is invalid.');
  }

  if (!manifest.profiles || typeof manifest.profiles !== 'object' ||
      PROFILE_NAMES.some((name) => typeof manifest.profiles[name] !== 'string' || manifest.profiles[name].length === 0)) {
    throw new Error('Manifest must contain all six nonempty profile tokens.');
  }
  if (new Set(PROFILE_NAMES.map((name) => manifest.profiles[name])).size !== PROFILE_NAMES.length) {
    throw new Error('Manifest profile tokens must identify distinct principals.');
  }

  const actorName = typeof manifest.actorName === 'string' && manifest.actorName.trim()
    ? manifest.actorName
    : actorNameFromComponentPath(manifest.componentPath);
  if (!actorName) throw new Error('Manifest must include actorName or an unambiguous component object path.');
  await fs.access(distCli);

  const expectedProjectPath = selectedRealPath;
  const nativeUrl = process.env.UNREAL_MCP_URL || 'http://127.0.0.1:13000/mcp';
  return {
    manifest,
    actorName,
    projectRoot,
    validationDir,
    selectedProjectPath: expectedProjectPath,
    expectedProjectName: process.env.UNREAL_MCP_TEST_PROJECT_NAME,
    expectedEngineVersionPrefix: process.env.UNREAL_MCP_TEST_ENGINE_VERSION,
    editorBaseDir: process.env.UE_EDITOR_BASE_DIR,
    nativeUrl,
    runId: randomUUID().replaceAll('-', '')
  };
}

function childEnvironment(config, transportName, profileName) {
  const env = {};
  for (const key of [
    'PATH', 'SystemRoot', 'WINDIR', 'COMSPEC', 'PATHEXT', 'TEMP', 'TMP',
    'USERPROFILE', 'APPDATA', 'LOCALAPPDATA', 'HOME',
    'MCP_AUTOMATION_WS_HOST', 'MCP_AUTOMATION_WS_PORTS'
  ]) {
    if (process.env[key] !== undefined) env[key] = process.env[key];
  }
  env.UE_PROJECT_PATH = config.selectedProjectPath;
  env.MCP_AUTOMATION_CAPABILITY_TOKEN = profileName === 'admin'
    ? (process.env.MCP_AUTOMATION_CAPABILITY_TOKEN ?? '')
    : config.manifest.profiles[profileName];
  if (transportName === 'native') {
    env.UNREAL_MCP_URL = config.nativeUrl;
    env.UNREAL_MCP_PROXY_CACHE = path.join(
      config.validationDir,
      `pcg-auth-proxy-${config.runId}-${profileName}.json`
    );
  }
  return env;
}

async function openClient(config, transportName, profileName) {
  const cacheKey = `${transportName}:${profileName}`;
  if (clientCache.has(cacheKey)) return clientCache.get(cacheKey);

  const transport = new StdioClientTransport({
    command: process.execPath,
    args: transportName === 'native' ? [distCli, 'proxy'] : [distCli],
    cwd: repoRoot,
    stderr: 'ignore',
    env: childEnvironment(config, transportName, profileName)
  });
  const client = new Client({ name: `pcg-auth-${transportName}-${profileName}`, version: '1.0.0' });
  const entry = { client, transport, caller: createToolCaller(client), failure: undefined };
  ownedClients.push(entry);
  clientCache.set(cacheKey, entry);

  try {
    await client.connect(transport);
  } catch (error) {
    entry.failure = error;
  }
  return entry;
}

function errorText(error) {
  if (!error) return '';
  const parts = [];
  if (typeof error.code === 'string') parts.push(error.code);
  if (typeof error.errorCode === 'string') parts.push(error.errorCode);
  if (typeof error.message === 'string') parts.push(error.message);
  return parts.join(' ');
}

function isNotConnected(value) {
  return /NOT_CONNECTED|not connected|econnrefused|econnreset|socket hang up|bridge disconnected/i.test(value);
}

function collectErrorCodes(value, output = []) {
  if (!value || typeof value !== 'object') return output;
  if (Array.isArray(value)) {
    for (const item of value) collectErrorCodes(item, output);
    return output;
  }
  for (const [key, item] of Object.entries(value)) {
    if ((key === 'errorCode' || key === 'code') && typeof item === 'string') output.push(item);
    collectErrorCodes(item, output);
  }
  return output;
}

async function invoke(entry, toolName, action, params, options = {}) {
  if (entry.failure) return { failure: entry.failure };
  const callOptions = {
    name: 'unreal',
    arguments: { operation: 'execute', tool: toolName, action, params }
  };
  if (options.consent !== undefined) callOptions.consent = options.consent;

  let pending;
  const previousLog = console.log;
  console.log = () => {};
  try {
    pending = entry.caller(callOptions, options.timeoutMs ?? CALL_TIMEOUT_MS);
  } catch (error) {
    return { failure: error };
  } finally {
    console.log = previousLog;
  }

  try {
    return { response: normalizeMcpResponse(await pending) };
  } catch (error) {
    return { failure: error };
  }
}

function responseSucceeded(result) {
  const response = result?.response;
  return Boolean(response && !response.isError && response.structuredContent?.success === true &&
    !isNotConnected(response.contentText));
}

function resultHasCode(result, expectedCode) {
  if (result?.failure) {
    const text = errorText(result.failure);
    return !isNotConnected(text) && text.includes(expectedCode);
  }
  const response = result?.response;
  if (!response || isNotConnected(response.contentText)) return false;
  const primaryCode = response.structuredContent?.errorCode;
  if (typeof primaryCode === 'string') return primaryCode === expectedCode;
  const codes = collectErrorCodes(response.structuredContent);
  return codes.includes(expectedCode) || (!codes.length && response.contentText.includes(expectedCode));
}

function responseFailedClosed(result) {
  const response = result?.response;
  return Boolean(response && !isNotConnected(response.contentText) &&
    (response.isError || response.structuredContent?.success === false));
}

function requireConnected(name, entry) {
  assertion(name, !entry.failure);
}

async function requireProfile(config, transportName, profileName) {
  const entry = await openClient(config, transportName, profileName);
  requireConnected(`${transportName} ${profileName} profile connects`, entry);
  return entry;
}

async function verifyProjectIdentity(config, transportName, owner) {
  const identity = await invoke(owner, 'inspect', 'get_project_settings', {});
  let verified = false;
  if (responseSucceeded(identity)) {
    try {
      assertIsolatedProject(identity.response, {
        projectName: config.expectedProjectName,
        projectDir: config.selectedProjectPath,
        engineVersionPrefix: config.expectedEngineVersionPrefix,
        editorBaseDir: config.editorBaseDir
      });
      verified = true;
    } catch {
      verified = false;
    }
  }
  assertion(`${transportName} owner identity matches selected project`, verified);
}

async function runAuthorizationChecks(config, transportName) {
  const { graphPath, root, nodeId } = config.manifest;
  const read = await requireProfile(config, transportName, 'read');
  const write = await requireProfile(config, transportName, 'write');
  const destructive = await requireProfile(config, transportName, 'destructive');
  const owner = await requireProfile(config, transportName, 'owner');

  const readGraph = await invoke(read, 'manage_pcg', 'get_pcg_graph', { graphPath });
  assertion(`${transportName} read profile can read its graph`, responseSucceeded(readGraph));

  const readCannotWrite = await invoke(read, 'manage_pcg', 'set_pcg_node_settings', {
    graphPath, nodeId, settings: { Seed: 1 }, save: false
  });
  assertion(`${transportName} read profile cannot write Seed`, resultHasCode(readCannotWrite, 'SCOPE_NOT_GRANTED'));

  const writeCannotRead = await invoke(write, 'manage_pcg', 'get_pcg_graph', { graphPath });
  assertion(`${transportName} write profile does not imply read`, resultHasCode(writeCannotRead, 'SCOPE_NOT_GRANTED'));

  const writeSeed = await invoke(write, 'manage_pcg', 'set_pcg_node_settings', {
    graphPath, nodeId, settings: { Seed: Math.floor(Math.random() * 2_000_000_000) + 1 }, save: false
  });
  assertion(`${transportName} write profile can set node Seed`, responseSucceeded(writeSeed));

  const destructiveCannotWrite = await invoke(destructive, 'manage_pcg', 'set_pcg_node_settings', {
    graphPath, nodeId, settings: { Seed: 2 }, save: false
  });
  assertion(`${transportName} destructive profile does not imply write`, resultHasCode(destructiveCannotWrite, 'SCOPE_NOT_GRANTED'));

  const destructiveCannotRead = await invoke(destructive, 'manage_pcg', 'get_pcg_graph', { graphPath });
  assertion(`${transportName} destructive profile does not imply read`, resultHasCode(destructiveCannotRead, 'SCOPE_NOT_GRANTED'));
  const cleanupWithoutConsent = await invoke(owner, 'manage_pcg', 'cleanup_pcg_component', {
    componentPath: config.manifest.componentPath, timeoutSeconds: 30, save: false
  });
  assertion(`${transportName} cleanup without consent is refused`, resultHasCode(cleanupWithoutConsent, 'CONSENT_REQUIRED'));
  const ownerOwnGraph = await invoke(owner, 'manage_pcg', 'get_pcg_graph', { graphPath });
  assertion(`${transportName} owner can read its graph`, responseSucceeded(ownerOwnGraph));

  const outsideGraphPath = `${root}_Outside/Graph`;
  const outsideRead = await invoke(owner, 'manage_pcg', 'get_pcg_graph', { graphPath: outsideGraphPath });
  assertion(`${transportName} owner cannot read outside its root`, resultHasCode(outsideRead, 'PATH_NOT_PERMITTED'));
  const outsideWrite = await invoke(owner, 'manage_pcg', 'set_pcg_node_settings', {
    graphPath: outsideGraphPath, nodeId, settings: { Seed: 3 }, save: false
  });
  assertion(`${transportName} owner cannot write outside its root`, resultHasCode(outsideWrite, 'PATH_NOT_PERMITTED'));

  const malformed = await invoke(owner, 'manage_pcg', 'get_pcg_graph', { graphPath: 'not-an-asset-path' });
  assertion(`${transportName} malformed graph path fails closed`, responseFailedClosed(malformed));
  const traversal = await invoke(owner, 'manage_pcg', 'get_pcg_graph', {
    graphPath: `${root}/../${path.posix.basename(root)}/Graph`
  });
  assertion(`${transportName} traversal graph path fails closed`, responseFailedClosed(traversal));

  const wrongProject = await openClient(config, transportName, 'wrongProject');
  if (wrongProject.failure) {
    const text = errorText(wrongProject.failure);
    assertion(`${transportName} wrong-project profile is refused`,
      !isNotConnected(text) && text.includes('PROJECT_NOT_PERMITTED'));
  } else {
    const wrongProjectRead = await invoke(wrongProject, 'manage_pcg', 'get_pcg_graph', { graphPath });
    assertion(`${transportName} wrong-project profile is refused`, resultHasCode(wrongProjectRead, 'PROJECT_NOT_PERMITTED'));
  }
}

function readResultField(response, field) {
  return resolveResponsePath(response, `structuredContent.result.${field}`);
}

async function pollForActiveOperation(config, ownerNative, executeState) {
  const deadline = Date.now() + ACTIVE_OPERATION_WAIT_MS;
  while (Date.now() < deadline) {
    const inventory = await invoke(ownerNative, 'manage_pcg', 'get_pcg_component_results', {
      componentPath: config.manifest.componentPath, sampleLimit: 0
    });
    if (!responseSucceeded(inventory)) {
      assertion('owner native inventory is readable during generation', false);
    }
    const generating = readResultField(inventory.response, 'generating');
    const activeOperationId = readResultField(inventory.response, 'activeOperationId');
    if (executeState.result && !responseSucceeded(executeState.result)) {
      assertion('owner WS execute call schedules successfully', false);
    }
    const executeOperationId = executeState.result
      ? readResultField(executeState.result.response, 'operationId')
      : undefined;
    const inventoryOperationId = typeof activeOperationId === 'string' && activeOperationId.length > 0
      ? activeOperationId
      : undefined;
    const operationId = inventoryOperationId ??
      (typeof executeOperationId === 'string' && executeOperationId.length > 0 ? executeOperationId : undefined);
    if (generating === true && operationId) {
      assertion('owner native inventory observes the active generation', true);
      if (inventoryOperationId && typeof executeOperationId === 'string' && executeOperationId.length > 0) {
        assertion('execute response and owner inventory identify the same operation', executeOperationId === inventoryOperationId);
      }
      return operationId;
    }
    if (executeState.result && !responseSucceeded(executeState.result)) {
      assertion('owner WS execute call schedules successfully', false);
    }
    await new Promise((resolve) => setTimeout(resolve, 200));
  }
  assertion('owner native inventory observes the delayed active operation', false);
  return undefined;
}

function leaksOperationIdentity(value, operationId) {
  if (!value || typeof value !== 'object') return false;
  if (Array.isArray(value)) return value.some((item) => leaksOperationIdentity(item, operationId));
  return Object.entries(value).some(([key, item]) => {
    const sensitiveField = key === 'activeOperationId' || key === 'operationId';
    const exposedValue = item !== null && item !== undefined && item !== '';
    return (sensitiveField && exposedValue) ||
      (typeof item === 'string' && item === operationId) || leaksOperationIdentity(item, operationId);
  });
}

async function runCrossPrincipalChecks(config) {
  const ownerWs = await requireProfile(config, 'ws', 'owner');
  const ownerNative = await requireProfile(config, 'native', 'owner');
  const otherWs = await requireProfile(config, 'ws', 'other');
  const otherNative = await requireProfile(config, 'native', 'other');
  const { graphPath, componentPath } = config.manifest;

  const initialDeadline = Date.now() + IDLE_WAIT_MS;
  let initiallyIdle = false;
  while (Date.now() < initialDeadline) {
    const inventory = await invoke(ownerNative, 'manage_pcg', 'get_pcg_component_results', { componentPath, sampleLimit: 0 });
    if (!responseSucceeded(inventory)) assertion('owner inventory is readable before scheduling', false);
    initiallyIdle = readResultField(inventory.response, 'generating') === false && readResultField(inventory.response, 'cleaning') === false;
    if (initiallyIdle) break;
    await new Promise(resolve => setTimeout(resolve, 1000));
  }
  assertion('any automatic generation from authored Seed writes is idle before the ownership test', initiallyIdle);
  const executeState = { result: undefined };
  const executePromise = invoke(ownerWs, 'manage_pcg', 'execute_pcg_graph', {
    graphPath,
    actorName: config.actorName,
    componentPath,
    createComponent: false,
    force: true,
    save: false
  }, { timeoutMs: CALL_TIMEOUT_MS }).then((result) => {
    executeState.result = result;
    return result;
  });

  const operationId = await pollForActiveOperation(config, ownerNative, executeState);
  const ownerStatus = await invoke(ownerNative, 'manage_pcg', 'get_pcg_operation_status', {
    componentPath, operationId
  });
  assertion('owner native can read the WS-started operation by original path', responseSucceeded(ownerStatus));
  assertion('the cross-transport operation is actually still running', readResultField(ownerStatus.response, 'engineStillRunning') === true);

  for (const [transportName, other] of [['native', otherNative], ['ws', otherWs]]) {
    const hiddenStatus = await invoke(other, 'manage_pcg', 'get_pcg_operation_status', {
      componentPath, operationId
    });
    assertion(`${transportName} other principal cannot read operation identity`,
      resultHasCode(hiddenStatus, 'PCG_OPERATION_NOT_FOUND'));

    const hiddenCancel = await invoke(other, 'manage_pcg', 'control_pcg_execution', {
      control: 'cancel', componentPath, operationId
    }, { consent: { capability: 'manage_pcg.control_pcg_execution', acknowledge: 'explicit' } });
    assertion(`${transportName} other principal cannot cancel operation identity`,
      resultHasCode(hiddenCancel, 'PCG_OPERATION_NOT_FOUND'));

    const hiddenInventory = await invoke(other, 'manage_pcg', 'get_pcg_component_results', {
      componentPath, sampleLimit: 0
    });
    const inventoryHasNoLeak = responseSucceeded(hiddenInventory) &&
      !leaksOperationIdentity(hiddenInventory.response.structuredContent, operationId) &&
      !hiddenInventory.response.contentText.includes(operationId);
    assertion(`${transportName} other inventory does not leak operation identity`, inventoryHasNoLeak);
  }

  const beforeCancel = await invoke(ownerNative, 'manage_pcg', 'get_pcg_operation_status', { componentPath, operationId });
  assertion('foreign cancellation refusals did not stop the native operation', responseSucceeded(beforeCancel) &&
    readResultField(beforeCancel.response, 'engineStillRunning') === true && readResultField(beforeCancel.response, 'cancellationRequested') === false);
  const ownerCancel = await invoke(ownerNative, 'manage_pcg', 'control_pcg_execution', {
    control: 'cancel', componentPath, operationId
  }, { consent: { capability: 'manage_pcg.control_pcg_execution', acknowledge: 'explicit' } });
  assertion('owner native can cancel with explicit consent envelope', responseSucceeded(ownerCancel));

  const idleDeadline = Date.now() + IDLE_WAIT_MS;
  let idle = false;
  while (Date.now() < idleDeadline) {
    const inventory = await invoke(ownerNative, 'manage_pcg', 'get_pcg_component_results', {
      componentPath, sampleLimit: 0
    });
    if (!responseSucceeded(inventory)) {
      assertion('owner native can poll read-only component inventory', false);
    }
    idle = readResultField(inventory.response, 'generating') === false &&
      readResultField(inventory.response, 'cleaning') === false;
    if (idle) break;
    await new Promise((resolve) => setTimeout(resolve, 1000));
  }
  assertion('owner native read-only polling observes generation and cleanup idle', idle);

  const cleanupWithoutConsent = await invoke(ownerNative, 'manage_pcg', 'cleanup_pcg_component', {
    componentPath, timeoutSeconds: 30, save: false
  });
  assertion('cleanup without consent is refused', resultHasCode(cleanupWithoutConsent, 'CONSENT_REQUIRED'));

  // This clears only PCG-generated resources; it does not delete the map, graph, actor, or component fixture.
  const cleanupWithConsent = await invoke(ownerNative, 'manage_pcg', 'cleanup_pcg_component', {
    componentPath, timeoutSeconds: 30, save: false
  }, { consent: { capability: 'manage_pcg.cleanup_pcg_component', acknowledge: 'explicit' } });
  assertion('cleanup succeeds with explicit consent envelope', responseSucceeded(cleanupWithConsent));

  for (const transportName of ['ws', 'native']) {
    const destructive = await requireProfile(config, transportName, 'destructive');
    const cleanup = await invoke(destructive, 'manage_pcg', 'cleanup_pcg_component', {
      componentPath, timeoutSeconds: 30, save: false
    }, { consent: { capability: 'manage_pcg.cleanup_pcg_component', acknowledge: 'explicit' } });
    assertion(`${transportName} destructive-only cleanup works with explicit envelope consent`, responseSucceeded(cleanup));
  }
  const settled = await Promise.race([
    executePromise.then((result) => ({ result })),
    new Promise((resolve) => setTimeout(() => resolve({ timeout: true }), 10_000))
  ]);
  assertion('owner WS execute call settles after explicit cancellation',
    !settled.timeout && responseSucceeded(settled.result));
}

async function runObservationCancellationCheck(config) {
  const ownerWs = await requireProfile(config, 'ws', 'owner');
  const ownerNative = await requireProfile(config, 'native', 'owner');
  const componentPath = config.manifest.componentPath;
  const controller = new AbortController();
  const deferred = ownerWs.client.callTool({ name: 'unreal', arguments: {
    operation: 'execute', tool: 'manage_pcg', action: 'generate_pcg_graph',
    params: { componentPath, timeoutSeconds: 30, save: false }, options: { timeoutMs: CALL_TIMEOUT_MS }
  } }, undefined, { signal: controller.signal, timeout: CALL_TIMEOUT_MS }).then(
    response => ({ response: normalizeMcpResponse(response) }), failure => ({ failure }));
  const operationId = await pollForActiveOperation(config, ownerNative, {});
  controller.abort();
  const cancelledCall = await deferred;
  assertion('cancelling the WS request ends its local observation', Boolean(cancelledCall.failure) || !responseSucceeded(cancelledCall));
  const observationDeadline = Date.now() + ACTIVE_OPERATION_WAIT_MS;
  let cancelledStatus;
  while (Date.now() < observationDeadline) {
    cancelledStatus = await invoke(ownerNative, 'manage_pcg', 'get_pcg_operation_status', { componentPath, operationId });
    if (responseSucceeded(cancelledStatus) && readResultField(cancelledStatus.response, 'state') === 'observation_cancelled') break;
    await new Promise(resolve => setTimeout(resolve, 200));
  }
  assertion('native transport retains the WS request-cancelled operation', responseSucceeded(cancelledStatus) &&
    readResultField(cancelledStatus.response, 'operationErrorCode') === 'PCG_OBSERVATION_CANCELLED');
  assertion('request observation cancellation does not silently cancel native work',
    readResultField(cancelledStatus.response, 'engineStillRunning') === true &&
    readResultField(cancelledStatus.response, 'cancellationRequested') === false);
  const busy = await invoke(ownerNative, 'manage_pcg', 'generate_pcg_graph', { componentPath, timeoutSeconds: 30, save: false });
  assertion('the native busy lease survives cross-transport observation cancellation', resultHasCode(busy, 'PCG_COMPONENT_BUSY'));
  const deadline = Date.now() + IDLE_WAIT_MS;
  let finalStatus;
  while (Date.now() < deadline) {
    finalStatus = await invoke(ownerNative, 'manage_pcg', 'get_pcg_operation_status', { componentPath, operationId });
    if (!responseSucceeded(finalStatus)) assertion('cancelled operation status remains readable', false);
    if (readResultField(finalStatus.response, 'engineStillRunning') === false) break;
    await new Promise(resolve => setTimeout(resolve, 1000));
  }
  assertion('late native completion does not rewrite the cancelled observation as success',
    readResultField(finalStatus?.response, 'engineStillRunning') === false &&
    readResultField(finalStatus.response, 'state') === 'observation_cancelled' &&
    readResultField(finalStatus.response, 'nativeGenerated') === true &&
    readResultField(finalStatus.response, 'lateCompletionObserved') === true);
  const cleaned = await invoke(ownerNative, 'manage_pcg', 'cleanup_pcg_component', { componentPath, timeoutSeconds: 30, save: false },
    { consent: { capability: 'manage_pcg.cleanup_pcg_component', acknowledge: 'explicit' } });
  assertion('only owned generated resources are cleaned after native idle', responseSucceeded(cleaned));
}

async function closeOwnedClients() {
  for (const entry of ownedClients) {
    try { await entry.client.close(); } catch { /* close only clients created by this driver */ }
    try { await entry.transport.close(); } catch { /* transport may already be closed by Client.close */ }
  }
}

async function main() {
  try {
    const config = await loadConfig();
    assertion('manifest and cache directory are confined to selected Saved/PCGValidation',
      isWithin(config.projectRoot, config.validationDir));

    // The normal project token is the identity-only fallback, not a scoped Admin grant.
    for (const transportName of ['ws', 'native']) {
      const owner = await openClient(config, transportName, 'admin');
      requireConnected(`${transportName} project identity fallback connects before mutations`, owner);
      const toolList = await owner.client.listTools({});
      assertion(`${transportName} exposes only the public unreal gateway`, toolList.tools.length === 1 && toolList.tools[0].name === 'unreal');
      await verifyProjectIdentity(config, transportName, owner);
    }

    for (const transportName of ['ws', 'native']) {
      await runAuthorizationChecks(config, transportName);
    }
    await runCrossPrincipalChecks(config);
    await runObservationCancellationCheck(config);
  } catch (error) {
    if (!(error instanceof AssertionFailure)) {
      failedAssertions += 1;
      console.log('FAIL: runner setup or transport error (details redacted)');
    }
    process.exitCode = 1;
  } finally {
    await closeOwnedClients();
    const status = process.exitCode === 1 ? 'failed' : 'finished';
    console.log(`SUMMARY: ${status}; ${passedAssertions} assertion(s) passed; ${failedAssertions} assertion(s) failed.`);
  }
}

await main();
