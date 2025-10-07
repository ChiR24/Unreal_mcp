#!/usr/bin/env node

import fs from 'node:fs/promises';
import path from 'node:path';
import process from 'node:process';
import { fileURLToPath } from 'node:url';
import { performance } from 'node:perf_hooks';

import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StdioClientTransport } from '@modelcontextprotocol/sdk/client/stdio.js';

const failureKeywords = [
  'error',
  'fail',
  'invalid',
  'missing',
  'not found',
  'reject',
  'warning'
];

const successKeywords = [
  'success',
  'spawn',
  'visible',
  'applied',
  'returns',
  'plays',
  'updates',
  'created',
  'saved'
];

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(__dirname, '..');
const defaultDocPath = path.resolve(repoRoot, 'docs', 'unreal-tool-test-cases.md');
const docPath = path.resolve(repoRoot, process.env.UNREAL_MCP_TEST_DOC ?? defaultDocPath);
const reportsDir = path.resolve(repoRoot, 'tests', 'reports');
const resultsPath = path.join(reportsDir, `unreal-tool-test-results-${new Date().toISOString().replace(/[:]/g, '-')}.json`);
const defaultFbxDir = normalizeWindowsPath(process.env.UNREAL_MCP_FBX_DIR ?? 'C:\\Users\\micro\\Downloads\\Compressed\\fbx');
const defaultFbxFile = normalizeWindowsPath(process.env.UNREAL_MCP_FBX_FILE ?? path.join(defaultFbxDir, 'test_model.fbx'));

const cliOptions = parseCliOptions(process.argv.slice(2));
const serverCommand = process.env.UNREAL_MCP_SERVER_CMD ?? 'node';
const serverArgs = parseArgsList(process.env.UNREAL_MCP_SERVER_ARGS) ?? [path.join(repoRoot, 'dist', 'cli.js')];
const serverCwd = process.env.UNREAL_MCP_SERVER_CWD ?? repoRoot;
const stressTestMode = process.env.STRESS_TEST_MODE === '1';
const benchmarkMode = process.env.BENCHMARK_MODE === '1';

async function main() {
  await ensureFbxDirectory();
  const allCases = await loadTestCasesFromDoc(docPath);
  if (allCases.length === 0) {
    console.error(`No test cases detected in ${docPath}.`);
    process.exitCode = 1;
    return;
  }

  const filteredCases = allCases.filter((testCase) => {
    if (cliOptions.group && testCase.groupName !== cliOptions.group) return false;
    if (cliOptions.caseId && testCase.caseId !== cliOptions.caseId) return false;
    if (cliOptions.text && !testCase.scenario.toLowerCase().includes(cliOptions.text.toLowerCase())) {
      return false;
    }
    return true;
  });

  if (filteredCases.length === 0) {
    console.warn('No test cases matched the provided filters. Exiting.');
    return;
  }

  let transport; let client;
  const runResults = [];
  let automationBridgeStatus = { connected: false, summary: null };
  let automationBridgeTestsEnabled = process.env.UNREAL_MCP_AUTOMATION_BRIDGE === '1';

  if (!cliOptions.dryRun) {
    try {
      transport = new StdioClientTransport({
        command: serverCommand,
        args: serverArgs,
        cwd: serverCwd,
        stderr: 'inherit'
      });

      client = new Client({
        name: 'unreal-mcp-tool-test-runner',
        version: '0.1.0'
      });

      await client.connect(transport);
      await client.listTools({});

      try {
        const bridgeResource = await client.readResource({ uri: 'ue://automation-bridge' });
        const text = bridgeResource.contents?.[0]?.text;
        if (text) {
          const parsed = JSON.parse(text);
          if (parsed && typeof parsed === 'object') {
            const summary = parsed.summary ?? parsed;
            const connected = Boolean(summary?.connected);
            automationBridgeStatus = { connected, summary };
            if (connected) {
              automationBridgeTestsEnabled = true;
            }
          }
        }
      } catch (err) {
        // Resource may not exist on older servers; treat as unavailable without failing run
        console.warn('[warn] Unable to query ue://automation-bridge resource:', err?.message ?? String(err));
      }
    } catch (err) {
      console.error('Failed to start or initialize MCP server:', err);
      if (transport) {
        try { await transport.close(); } catch { /* ignore */ }
      }
      process.exitCode = 1;
      return;
    }
  }

  for (const testCase of filteredCases) {
    let skipReason = testCase.skipReason;

    if (!skipReason && testCase.groupName === 'Automation Bridge') {
      if (!automationBridgeTestsEnabled) {
        skipReason = 'Automation bridge tests disabled (set UNREAL_MCP_AUTOMATION_BRIDGE=1 or connect the plugin).';
      } else {
        const requestedTransport = typeof testCase.arguments?.transport === 'string'
          ? testCase.arguments.transport.trim().toLowerCase()
          : '';
        const wantsBridge = ['automation_bridge', 'automation', 'bridge'].includes(requestedTransport);
        if (wantsBridge && !automationBridgeStatus.connected) {
          skipReason = 'Automation bridge transport requested but plugin is not connected.';
        }
      }
    }

    if (testCase.skipReason) {
      runResults.push({
        ...testCase,
        status: 'skipped',
        detail: testCase.skipReason
      });
      console.log(formatResultLine(testCase, 'skipped', testCase.skipReason));
      continue;
    }

    if (skipReason) {
      runResults.push({
        ...testCase,
        status: 'skipped',
        detail: skipReason
      });
      console.log(formatResultLine(testCase, 'skipped', skipReason));
      continue;
    }

    if (cliOptions.dryRun) {
      runResults.push({
        ...testCase,
        status: 'skipped',
        detail: 'Dry run'
      });
      console.log(formatResultLine(testCase, 'skipped', 'Dry run'));
      continue;
    }

    const started = performance.now();
    try {
      const response = await client.callTool({
        name: testCase.toolName,
        arguments: testCase.arguments
      });
      const duration = performance.now() - started;
      const evaluation = evaluateExpectation(testCase, response);
      runResults.push({
        ...testCase,
        status: evaluation.passed ? 'passed' : 'failed',
        durationMs: duration,
        detail: evaluation.reason,
        response
      });
      console.log(formatResultLine(testCase, evaluation.passed ? 'passed' : 'failed', evaluation.reason, duration));
    } catch (err) {
      const duration = performance.now() - started;
      runResults.push({
        ...testCase,
        status: 'failed',
        durationMs: duration,
        detail: err instanceof Error ? err.message : String(err)
      });
      console.log(formatResultLine(testCase, 'failed', err instanceof Error ? err.message : String(err), duration));
    }
  }

  if (!cliOptions.dryRun) {
    try {
      await client.close();
    } catch {
      // ignore
    }
    try {
      await transport.close();
    } catch {
      // ignore
    }
  }

  await persistResults(runResults);
  summarize(runResults);

  // Performance statistics if benchmarking
  if (benchmarkMode) {
    generateBenchmarkReport(runResults);
  }

  if (runResults.some((result) => result.status === 'failed')) {
    process.exitCode = 1;
  }
}

function parseCliOptions(args) {
  const options = {
    dryRun: false,
    group: undefined,
    caseId: undefined,
    text: undefined
  };

  for (const arg of args) {
    if (arg === '--dry-run') {
      options.dryRun = true;
    } else if (arg.startsWith('--group=')) {
      options.group = arg.slice('--group='.length);
    } else if (arg.startsWith('--case=')) {
      options.caseId = arg.slice('--case='.length);
    } else if (arg.startsWith('--text=')) {
      options.text = arg.slice('--text='.length);
    }
  }

  return options;
}

function parseArgsList(value) {
  if (!value) return undefined;
  const trimmed = value.trim();
  if (!trimmed) return undefined;
  if (trimmed.startsWith('[')) {
    try {
      const parsed = JSON.parse(trimmed);
      if (Array.isArray(parsed)) return parsed.map(String);
    } catch (_) {
      // fall through
    }
  }
  return trimmed.split(/\s+/).filter(Boolean);
}

async function loadTestCasesFromDoc(filePath) {
  const raw = await fs.readFile(filePath, 'utf8');
  const lines = raw.split(/\r?\n/);
  const cases = [];
  let currentGroup = undefined;
  let inLegacySection = false;

  for (const line of lines) {
    if (line.startsWith('## ')) {
      const headerTitle = line.replace(/^##\s+/, '').trim();
      if (headerTitle.toLowerCase().includes('legacy comprehensive matrix')) {
        inLegacySection = true;
        currentGroup = undefined;
        continue;
      }
      if (inLegacySection) {
        currentGroup = undefined;
        continue;
      }
      currentGroup = headerTitle;
      continue;
    }
    if (!currentGroup) continue;
    if (!line.startsWith('|') || /^\|\s*-+/.test(line) || /^\|\s*#\s*\|/.test(line)) {
      continue;
    }

    const columns = line.split('|').map((part) => part.trim());
    if (columns.length < 5) continue;
    const index = columns[1];
    const scenario = columns[2];
    const example = columns[3];
    const expected = columns[4];
    const payload = extractPayload(example);
    const simplifiedGroup = simplifyGroupName(currentGroup);

    const enriched = enrichTestCase({
      group: currentGroup,
      groupName: simplifiedGroup,
      index,
      scenario,
      example,
      expected,
      payload
    });

    cases.push(enriched);
  }
  return cases;
}

function simplifyGroupName(groupTitle) {
  return groupTitle
    .replace(/`[^`]+`/g, '')
    .replace(/\([^)]*\)/g, '')
    .replace('Tools', 'Tools')
    .trim();
}

function extractPayload(exampleColumn) {
  if (!exampleColumn) return undefined;
  const codeMatches = [...exampleColumn.matchAll(/`([^`]+)`/g)];
  for (const match of codeMatches) {
    const snippet = match[1].trim();
    if (!snippet) continue;
    const jsonCandidate = normalizeJsonCandidate(snippet);
    if (!jsonCandidate) continue;
    try {
      return {
        raw: snippet,
        value: JSON.parse(jsonCandidate)
      };
    } catch (_) {
      continue;
    }
  }
  return undefined;
}

function normalizeJsonCandidate(snippet) {
  const trimmed = snippet.trim();
  if ((trimmed.startsWith('{') && trimmed.endsWith('}')) || (trimmed.startsWith('[') && trimmed.endsWith(']'))) {
    return trimmed;
  }
  return undefined;
}

function enrichTestCase(rawCase) {
  const caseIdBase = `${rawCase.groupName.toLowerCase().replace(/\s+/g, '-')}-${rawCase.index}`;
  const base = {
    group: rawCase.group,
    groupName: rawCase.groupName,
    index: rawCase.index,
    scenario: rawCase.scenario,
    expected: rawCase.expected,
    example: rawCase.example,
    caseId: caseIdBase,
    payloadSnippet: rawCase.payload?.raw,
    arguments: undefined,
    toolName: undefined,
    skipReason: undefined
  };

  const payloadValue = rawCase.payload?.value
    ? hydratePlaceholders(rawCase.payload.value)
    : undefined;
  const scenarioLower = rawCase.scenario.toLowerCase();

  switch (rawCase.groupName) {
    case 'Lighting Tools': {
      if (!payloadValue) {
        return { ...base, skipReason: 'No JSON payload provided' };
      }
      if (/lightmass|ensure/i.test(rawCase.scenario)) {
        return { ...base, skipReason: 'Scenario requires manual steps not exposed via consolidated tool' };
      }
      let lightType;
      if (scenarioLower.includes('directional')) lightType = 'Directional';
      else if (scenarioLower.includes('point')) lightType = 'Point';
      else if (scenarioLower.includes('spot')) lightType = 'Spot';
      else if (scenarioLower.includes('rect')) lightType = 'Rect';
      else if (scenarioLower.includes('sky')) lightType = 'Sky';
      else if (scenarioLower.includes('build lighting')) {
        return {
          ...base,
          skipReason: 'Skipping build lighting scenarios to avoid long editor runs'
        };
      } else {
        return { ...base, skipReason: 'Unrecognized light type or scenario' };
      }

      const args = {
        action: 'create_light',
        lightType,
        name: payloadValue.name ?? `${lightType}Light_${rawCase.index}`
      };

      if (typeof payloadValue.intensity === 'number') {
        args.intensity = payloadValue.intensity;
      }
      if (Array.isArray(payloadValue.location) && payloadValue.location.length === 3) {
        args.location = {
          x: payloadValue.location[0],
          y: payloadValue.location[1],
          z: payloadValue.location[2]
        };
      }
      if (Array.isArray(payloadValue.rotation) && payloadValue.rotation.length === 3) {
        args.rotation = {
          pitch: payloadValue.rotation[0],
          yaw: payloadValue.rotation[1],
          roll: payloadValue.rotation[2]
        };
      }
      if (Array.isArray(payloadValue.color) && payloadValue.color.length === 3) {
        args.color = payloadValue.color;
      }
      if (payloadValue.radius !== undefined) {
        args.radius = payloadValue.radius;
      }
      if (payloadValue.innerCone !== undefined) {
        args.innerCone = payloadValue.innerCone;
      }
      if (payloadValue.outerCone !== undefined) {
        args.outerCone = payloadValue.outerCone;
      }
      if (payloadValue.width !== undefined) {
        args.width = payloadValue.width;
      }
      if (payloadValue.height !== undefined) {
        args.height = payloadValue.height;
      }
      if (payloadValue.falloffExponent !== undefined) {
        args.falloffExponent = payloadValue.falloffExponent;
      }
      if (typeof payloadValue.castShadows === 'boolean') {
        args.castShadows = payloadValue.castShadows;
      }
      if (payloadValue.temperature !== undefined) {
        args.temperature = payloadValue.temperature;
      }
      if (typeof payloadValue.sourceType === 'string') {
        args.sourceType = payloadValue.sourceType;
      }
      if (typeof payloadValue.cubemapPath === 'string') {
        args.cubemapPath = payloadValue.cubemapPath;
      }
      if (typeof payloadValue.recapture === 'boolean') {
        args.recapture = payloadValue.recapture;
      }

      return {
        ...base,
        toolName: 'manage_level',
        arguments: args
      };
    }
    case 'Actor Tools': {
      if (!payloadValue) {
        return { ...base, skipReason: 'No JSON payload provided' };
      }
      const args = { ...payloadValue };
      if (Array.isArray(args.location) && args.location.length === 3) {
        args.location = { x: args.location[0], y: args.location[1], z: args.location[2] };
      }
      if (Array.isArray(args.rotation) && args.rotation.length === 3) {
        args.rotation = { pitch: args.rotation[0], yaw: args.rotation[1], roll: args.rotation[2] };
      }
      return {
        ...base,
        toolName: 'control_actor',
        arguments: args
      };
    }
    case 'Asset Tools': {
      if (!payloadValue) {
        return { ...base, skipReason: 'Non-JSON payload requires manual execution' };
      }
      if (!payloadValue.action) {
        return { ...base, skipReason: 'Action missing; unable to route to consolidated tool' };
      }
      if (!['list', 'import', 'create_material'].includes(payloadValue.action)) {
        return { ...base, skipReason: `Action '${payloadValue.action}' not supported by automated runner` };
      }
      return {
        ...base,
        toolName: 'manage_asset',
        arguments: payloadValue
      };
    }
    case 'Animation Tools': {
      if (!payloadValue) {
        return { ...base, skipReason: 'No JSON payload provided' };
      }
      const args = { ...payloadValue };
      if (scenarioLower.includes('animation blueprint')) {
        args.action = 'create_animation_bp';
      } else if (scenarioLower.includes('montage') || scenarioLower.includes('animation asset once')) {
        args.action = 'play_montage';
      } else if (scenarioLower.includes('ragdoll')) {
        args.action = 'setup_ragdoll';
      }
      if (!args.action) {
        return { ...base, skipReason: 'Scenario not supported by automated runner' };
      }
      return {
        ...base,
        toolName: 'animation_physics',
        arguments: args
      };
    }
    case 'Blueprint Tools': {
      if (!payloadValue) {
        return { ...base, skipReason: 'No JSON payload provided' };
      }
      const args = { ...payloadValue };
      if (!args.action) {
        args.action = scenarioLower.includes('component') ? 'add_component' : 'create';
      }
      return {
        ...base,
        toolName: 'manage_blueprint',
        arguments: args
      };
    }
    case 'Material Tools': {
      if (!payloadValue) {
        return { ...base, skipReason: 'No JSON payload provided' };
      }
      const args = { ...payloadValue };
      if (!args.action && typeof args.name === 'string' && typeof args.path === 'string') {
        args.action = 'create_material';
      }
      if (args.action === 'create_material') {
        return {
          ...base,
          toolName: 'manage_asset',
          arguments: {
            action: 'create_material',
            name: typeof args.name === 'string' ? args.name : `M_Test_${rawCase.index}`,
            path: args.path
          }
        };
      }
      return { ...base, skipReason: 'Material scenario not supported by automated runner' };
    }
    case 'Niagara Tools': {
      if (!payloadValue) return { ...base, skipReason: 'No JSON payload provided' };
      if (!payloadValue.action) {
        return { ...base, skipReason: 'Missing action for Niagara scenario' };
      }
      if (Array.isArray(payloadValue.location) && payloadValue.location.length === 3) {
        payloadValue.location = { x: payloadValue.location[0], y: payloadValue.location[1], z: payloadValue.location[2] };
      }
      return {
        ...base,
        toolName: 'create_effect',
        arguments: payloadValue
      };
    }
    case 'Level Tools': {
      if (!payloadValue) return { ...base, skipReason: 'No JSON payload provided' };
      if (!payloadValue.action) {
        return { ...base, skipReason: 'Missing action for level scenario' };
      }
      return {
        ...base,
        toolName: 'manage_level',
        arguments: payloadValue
      };
    }
    case 'Sequence Tools': {
      if (!payloadValue) return { ...base, skipReason: 'No JSON payload provided' };
      if (!payloadValue.action) return { ...base, skipReason: 'Missing action in payload' };
      return {
        ...base,
        toolName: 'manage_sequence',
        arguments: payloadValue
      };
    }
    case 'UI Tools': {
      if (!payloadValue) return { ...base, skipReason: 'No JSON payload provided' };
      if (!payloadValue.action) return { ...base, skipReason: 'Missing action in payload' };
      return {
        ...base,
        toolName: 'system_control',
        arguments: payloadValue
      };
    }
    case 'Physics Tools': {
      if (!payloadValue) return { ...base, skipReason: 'No JSON payload provided' };
      if (payloadValue.action === 'apply_force') {
        return {
          ...base,
          toolName: 'control_actor',
          arguments: payloadValue
        };
      }
      return { ...base, skipReason: 'Physics scenario not mapped to consolidated tools' };
    }
    case 'Landscape Tools': {
      if (!payloadValue) return { ...base, skipReason: 'No JSON payload provided' };
      if (!payloadValue.action) return { ...base, skipReason: 'Missing action in payload' };
      return {
        ...base,
        toolName: 'build_environment',
        arguments: payloadValue
      };
    }
    case 'Build Environment Tools': {
      if (!payloadValue) return { ...base, skipReason: 'No JSON payload provided' };
      if (!payloadValue.action) return { ...base, skipReason: 'Missing action in payload' };
      return {
        ...base,
        toolName: 'build_environment',
        arguments: payloadValue
      };
    }
    case 'Performance Tools': {
      if (!payloadValue) return { ...base, skipReason: 'No JSON payload provided' };
      if (!payloadValue.action) return { ...base, skipReason: 'Missing action in payload' };
      if (scenarioLower.includes('engine quit')) {
        return {
          ...base,
          skipReason: 'Skipping engine quit to keep Unreal session alive during test run'
        };
      }
      return {
        ...base,
        toolName: 'system_control',
        arguments: payloadValue
      };
    }
    case 'System Control Tools': {
      if (!payloadValue) return { ...base, skipReason: 'No JSON payload provided' };
      if (!payloadValue.action) return { ...base, skipReason: 'Missing action in payload' };
      if (payloadValue.action === 'engine_quit' || payloadValue.action === 'engine_start') {
        return {
          ...base,
          skipReason: 'Skipping engine process management during automated run'
        };
      }
      return {
        ...base,
        toolName: 'system_control',
        arguments: payloadValue
      };
    }
    case 'Automation Bridge': {
      if (!payloadValue) return { ...base, skipReason: 'No JSON payload provided' };
      return {
        ...base,
        toolName: 'execute_python',
        arguments: { ...payloadValue }
      };
    }
    case 'Debug Tools': {
      if (!payloadValue) return { ...base, skipReason: 'No JSON payload provided' };
      if (!payloadValue.action) {
        payloadValue.action = 'debug_shape';
      }
      return {
        ...base,
        toolName: 'create_effect',
        arguments: payloadValue
      };
    }
    case 'Remote Control Preset Tools': {
      if (!payloadValue) return { ...base, skipReason: 'No JSON payload provided' };
      if (!payloadValue.action) return { ...base, skipReason: 'Missing action in payload' };
      return {
        ...base,
        toolName: 'manage_rc',
        arguments: payloadValue
      };
    }
    case 'Asset Boundary Tests': {
      if (!payloadValue) return { ...base, skipReason: 'No JSON payload provided' };
      return { ...base, toolName: 'manage_asset', arguments: payloadValue };
    }
    case 'Actor Boundary Tests': {
      if (!payloadValue) return { ...base, skipReason: 'No JSON payload provided' };
      const args = { ...payloadValue };
      if (Array.isArray(args.location) && args.location.length === 3) {
        args.location = { x: args.location[0], y: args.location[1], z: args.location[2] };
      }
      if (Array.isArray(args.rotation) && args.rotation.length === 3) {
        args.rotation = { pitch: args.rotation[0], yaw: args.rotation[1], roll: args.rotation[2] };
      }
      if (Array.isArray(args.scale) && args.scale.length === 3) {
        args.scale = { x: args.scale[0], y: args.scale[1], z: args.scale[2] };
      }
      if (Array.isArray(args.force) && args.force.length === 3) {
        args.force = { x: args.force[0], y: args.force[1], z: args.force[2] };
      }
      return { ...base, toolName: 'control_actor', arguments: args };
    }
    case 'Editor Control Boundary Tests': {
      if (!payloadValue) return { ...base, skipReason: 'No JSON payload provided' };
      return { ...base, toolName: 'control_editor', arguments: payloadValue };
    }
    case 'Level Management Boundary Tests': {
      if (!payloadValue) return { ...base, skipReason: 'No JSON payload provided' };
      return { ...base, toolName: 'manage_level', arguments: payloadValue };
    }
    case 'Animation Boundary Tests': {
      if (!payloadValue) return { ...base, skipReason: 'No JSON payload provided' };
      return { ...base, toolName: 'animation_physics', arguments: payloadValue };
    }
    case 'Blueprint Boundary Tests': {
      if (!payloadValue) return { ...base, skipReason: 'No JSON payload provided' };
      return { ...base, toolName: 'manage_blueprint', arguments: payloadValue };
    }
    case 'Effects Boundary Tests': {
      if (!payloadValue) return { ...base, skipReason: 'No JSON payload provided' };
      const args = { ...payloadValue };
      if (Array.isArray(args.location) && args.location.length === 3) {
        args.location = { x: args.location[0], y: args.location[1], z: args.location[2] };
      }
      if (Array.isArray(args.start) && args.start.length === 3) {
        args.start = { x: args.start[0], y: args.start[1], z: args.start[2] };
      }
      if (Array.isArray(args.end) && args.end.length === 3) {
        args.end = { x: args.end[0], y: args.end[1], z: args.end[2] };
      }
      return { ...base, toolName: 'create_effect', arguments: args };
    }
    case 'Environment Building Boundary Tests': {
      if (!payloadValue) return { ...base, skipReason: 'No JSON payload provided' };
      const args = { ...payloadValue };
      if (Array.isArray(args.location) && args.location.length === 3) {
        args.location = { x: args.location[0], y: args.location[1], z: args.location[2] };
      }
      return { ...base, toolName: 'build_environment', arguments: args };
    }
    case 'System Control Boundary Tests': {
      if (!payloadValue) return { ...base, skipReason: 'No JSON payload provided' };
      return { ...base, toolName: 'system_control', arguments: payloadValue };
    }
    case 'Sequence Boundary Tests': {
      if (!payloadValue) return { ...base, skipReason: 'No JSON payload provided' };
      return { ...base, toolName: 'manage_sequence', arguments: payloadValue };
    }
    case 'Remote Control Boundary Tests': {
      if (!payloadValue) return { ...base, skipReason: 'No JSON payload provided' };
      return { ...base, toolName: 'manage_rc', arguments: payloadValue };
    }
    case 'Python Execution Boundary Tests': {
      if (!payloadValue) return { ...base, skipReason: 'No JSON payload provided' };
      return { ...base, toolName: 'execute_python', arguments: payloadValue };
    }
    case 'Inspection Boundary Tests': {
      if (!payloadValue) return { ...base, skipReason: 'No JSON payload provided' };
      return { ...base, toolName: 'inspect', arguments: payloadValue };
    }
    case 'Cross-Tool Integration Tests': 
    case 'Stress Test Scenarios':
    case 'Error Recovery Tests': {
      if (!payloadValue) return { ...base, skipReason: 'No JSON payload provided' };
      // These require multi-step execution or special handling
      return { ...base, skipReason: 'Multi-step test scenario - requires custom test implementation' };
    }
    default:
      return { ...base, skipReason: `Unknown tool group '${rawCase.groupName}'` };
  }
}

function evaluateExpectation(testCase, response) {
  const lowerExpected = testCase.expected.toLowerCase();
  const containsFailure = failureKeywords.some((word) => lowerExpected.includes(word));
  const containsSuccess = successKeywords.some((word) => lowerExpected.includes(word));

  const structuredSuccess = typeof response.structuredContent?.success === 'boolean'
    ? response.structuredContent.success
    : undefined;
  const actualSuccess = structuredSuccess ?? !response.isError;

  const expectedFailure = containsFailure && !containsSuccess;
  const passed = expectedFailure ? !actualSuccess : !!actualSuccess;
  let reason;
  if (response.isError) {
    reason = response.content?.map((entry) => ('text' in entry ? entry.text : JSON.stringify(entry))).join('\n');
  } else if (response.structuredContent) {
    reason = JSON.stringify(response.structuredContent);
  } else if (response.content?.length) {
    reason = response.content.map((entry) => ('text' in entry ? entry.text : JSON.stringify(entry))).join('\n');
  } else {
    reason = 'No structured response returned';
  }

  return { passed, reason };
}

function formatResultLine(testCase, status, detail, durationMs) {
  const durationText = typeof durationMs === 'number' ? ` (${durationMs.toFixed(1)} ms)` : '';
  return `[${status.toUpperCase()}] ${testCase.groupName} #${testCase.index} – ${testCase.scenario}${durationText}${detail ? ` => ${detail}` : ''}`;
}

async function persistResults(results) {
  await fs.mkdir(reportsDir, { recursive: true });
  const serializable = results.map((result) => ({
    group: result.groupName,
    caseId: result.caseId,
    index: result.index,
    scenario: result.scenario,
    toolName: result.toolName,
    arguments: result.arguments,
    status: result.status,
    durationMs: result.durationMs,
    detail: result.detail
  }));
  await fs.writeFile(resultsPath, JSON.stringify({
    generatedAt: new Date().toISOString(),
    docPath,
    results: serializable
  }, null, 2));
}

function summarize(results) {
  const totals = results.reduce((acc, result) => {
    acc.total += 1;
    acc[result.status] = (acc[result.status] ?? 0) + 1;
    return acc;
  }, { total: 0, passed: 0, failed: 0, skipped: 0 });

  console.log('\nSummary');
  console.log('=======');
  console.log(`Total cases processed: ${totals.total}`);
  console.log(`Passed: ${totals.passed}`);
  console.log(`Failed: ${totals.failed}`);
  console.log(`Skipped: ${totals.skipped}`);
  console.log(`Results written to: ${resultsPath}`);
}

function generateBenchmarkReport(results) {
  const passedResults = results.filter(r => r.status === 'passed' && r.durationMs);
  
  if (passedResults.length === 0) {
    console.log('\nNo performance data available for benchmarking.');
    return;
  }

  const durations = passedResults.map(r => r.durationMs).sort((a, b) => a - b);
  const sum = durations.reduce((a, b) => a + b, 0);
  const avg = sum / durations.length;
  const median = durations[Math.floor(durations.length / 2)];
  const min = durations[0];
  const max = durations[durations.length - 1];
  const p95 = durations[Math.floor(durations.length * 0.95)];
  const p99 = durations[Math.floor(durations.length * 0.99)];

  console.log('\nPerformance Benchmark');
  console.log('====================');
  console.log(`Total operations: ${passedResults.length}`);
  console.log(`Average: ${avg.toFixed(2)} ms`);
  console.log(`Median: ${median.toFixed(2)} ms`);
  console.log(`Min: ${min.toFixed(2)} ms`);
  console.log(`Max: ${max.toFixed(2)} ms`);
  console.log(`95th percentile: ${p95.toFixed(2)} ms`);
  console.log(`99th percentile: ${p99.toFixed(2)} ms`);

  // Group by tool
  const byTool = {};
  passedResults.forEach(r => {
    if (!byTool[r.toolName]) byTool[r.toolName] = [];
    byTool[r.toolName].push(r.durationMs);
  });

  console.log('\nBy Tool:');
  Object.entries(byTool).forEach(([tool, times]) => {
    const toolAvg = times.reduce((a, b) => a + b, 0) / times.length;
    console.log(`  ${tool}: ${toolAvg.toFixed(2)} ms avg (${times.length} ops)`);
  });
}

function normalizeWindowsPath(value) {
  if (typeof value !== 'string') return value;
  return value.replace(/\\+/g, '\\').replace(/\/+/g, '\\');
}

function hydratePlaceholders(value) {
  if (typeof value === 'string') {
    return value
      .replaceAll('{{FBX_DIR}}', defaultFbxDir)
      .replaceAll('{{FBX_TEST_MODEL}}', defaultFbxFile);
  }
  if (Array.isArray(value)) {
    return value.map((entry) => hydratePlaceholders(entry));
  }
  if (value && typeof value === 'object') {
    return Object.fromEntries(Object.entries(value).map(([key, val]) => [key, hydratePlaceholders(val)]));
  }
  return value;
}

async function ensureFbxDirectory() {
  if (!defaultFbxDir) return;
  try {
    await fs.mkdir(defaultFbxDir, { recursive: true });
  } catch (err) {
    console.warn(`Unable to ensure FBX directory '${defaultFbxDir}':`, err);
  }
}

main().catch((err) => {
  console.error('Unexpected error during test execution:', err);
  process.exitCode = 1;
});
