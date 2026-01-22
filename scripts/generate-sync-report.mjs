#!/usr/bin/env node
/**
 * Generate 3-Way Synchronization Report: TS ↔ C++ ↔ Live MCP Server
 * 
 * This script compares actions across three sources:
 * 1. TypeScript definitions (static source)
 * 2. C++ handler implementations (static source)
 * 3. Live MCP server (runtime - what LLMs actually see)
 * 
 * Usage: npm run docs:sync-report
 * Output: docs/action-sync-report.md
 */
import { readFileSync, writeFileSync, readdirSync, mkdirSync, existsSync } from 'fs';
import { dirname, join, basename } from 'path';
import { fileURLToPath } from 'url';
import { spawn } from 'child_process';
import fs from 'fs/promises';

// MCP SDK imports for live server connection
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StdioClientTransport } from '@modelcontextprotocol/sdk/client/stdio.js';

// Import tool definitions (static TS)
import { consolidatedToolDefinitions } from '../dist/tools/consolidated-tool-definitions.js';

const __dirname = dirname(fileURLToPath(import.meta.url));
const repoRoot = join(__dirname, '..');
const handlersDir = join(__dirname, '..', 'plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private');
const outputPath = join(__dirname, '..', 'docs', 'action-sync-report.md');

const ACTION_VARIABLES = new Set([
  'subaction', 'lowersubaction', 'lowersub', 'lower', 'actiontype',
  'action', 'operation', 'lowerpredicate', 'lowertype', 'lowerquality', 'lowermode',
  'effectiveaction',
]);

// ============================================================================
// DYNAMIC FALSE POSITIVE DETECTION
// ============================================================================
// Instead of hardcoding every false positive, use pattern-based detection
// that identifies characteristics of non-action strings.

/**
 * Patterns that indicate a string is NOT a valid action name
 */
const FALSE_POSITIVE_PATTERNS = {
  // Tool name pattern: matches our tool naming convention
  toolNames: /^(manage|control|build|test)_[a-z_]+$/,
  
  // Primitive type names
  primitiveTypes: /^(float|int|int32|int64|double|bool|boolean|byte|string|text|name|object|class|void|auto)$/,
  
  // UE type patterns
  ueTypes: /^(actor|pawn|character|vector|rotator|transform|color|linear_color)$/,
  
  // Single generic verbs - REMOVED from filtering
  // These ARE valid actions when used as tool sub-actions (e.g., manage_level action:"load")
  // The old pattern was: /^(add|remove|get|set|list|create|delete|update|save|load|find|search|hide|show|play|stop|pause|resume)$/
  // Now we don't filter these because they're legitimate tool actions
  genericVerbs: /^$/, // Empty pattern - don't filter generic verbs
  
  // Audio/synth node types (MetaSound, etc.)
  audioNodes: /^(adsr|oscillator|envelope|filter|mixer|gain|delay|reverb|chorus|flanger|phaser|compressor|limiter|noise|decay|sine|saw|sawtooth|square|triangle|whitenoise|audioinput|audiooutput|floatinput|lowpass|highpass|bandpass|lpf|hpf|bpf|input|output|parameter)$/i,
  
  // Material expression patterns
  materialExpressions: /^(multiply|divide|add|subtract|lerp|linearinterpolate|clamp|power|pow|frac|fraction|oneminus|fresnel|constant|constant[234]vector|scalar|scalarparameter|vectorparameter|textureparameter|texturesample|texturesampleparameter2d|texcoord|texturecoordinate|uv|panner|vertexnormal|vertexnormalws|worldposition|reflectionvector|pixeldepth|depth|functioncall|materialfunctioncall|custom|customexpression|staticswitch|staticswitchparameter|switch|if|rgb|rgba|floatparam|boolparam|colorparam|float[234]|mul|div|sub|append|appendvector|hlsl)$/i,
  
  // View modes (editor visualization)
  viewModes: /^(lit|unlit|wireframe|shadercomplexity|lightcomplexity|lightingonly|lightmapdensity|reflectionoverride|stationarylightoverlap|detaillighting)$/,
  
  // GAS modifier operations
  gasModifiers: /^(additive|multiplicative|division|override)$/,
  
  // Locomotion/movement modes
  locomotionModes: /^(walk|walking|run|running|fly|flying|swim|swimming|fall|falling|none|crouch|crouching|prone|slide|sliding|idle|jump|jumping)$/,
  
  // Asset type categories (not actions)
  assetCategories: /^(blueprint|blueprints|material|materials|mesh|meshes|texture|textures|sound|sounds|audio|staticmesh|skeletalmesh|animation|animations|particle|particles|niagara|level|levels|widget|widgets)$/,
  
  // Quality presets
  qualityPresets: /^(high|medium|low|preview|epic|cinematic|ultra|custom)$/,
};

/**
 * Heuristic rules for detecting false positives
 */
const FALSE_POSITIVE_RULES = [
  // Rule 1: Too short (less than 3 chars) - likely abbreviation or enum value
  (action) => action.length < 3,
  
  // Rule 2: Single word without underscore AND is a common word
  // Valid actions typically have underscores (e.g., "create_actor", "spawn_blueprint")
  // Exception: Some valid actions are single words with specific prefixes
  (action) => {
    if (action.includes('_')) return false; // Has underscore, likely valid
    // Check against all pattern categories
    return Object.values(FALSE_POSITIVE_PATTERNS).some(pattern => pattern.test(action));
  },
  
  // Rule 3: Ends with common type suffixes suggesting it's a type, not action
  (action) => /^.+(oscillator|filter|generator|parameter|input|output|sample|sampler)$/i.test(action) && !action.includes('_'),
  
  // Rule 4: PascalCase without underscore (likely a class/type name, not an action)
  (action) => /^[A-Z][a-z]+[A-Z]/.test(action) && !action.includes('_'),
];

/**
 * Check if an action is a false positive using pattern matching and heuristics
 */
function isFalsePositive(action) {
  const lower = action.toLowerCase();
  
  // Check pattern matches
  for (const [category, pattern] of Object.entries(FALSE_POSITIVE_PATTERNS)) {
    if (pattern.test(lower)) {
      return true;
    }
  }
  
  // Check heuristic rules
  for (const rule of FALSE_POSITIVE_RULES) {
    if (rule(action)) {
      return true;
    }
  }
  
  return false;
}

/**
 * Filter out false positive actions from a list
 */
function filterFalsePositives(actions) {
  return actions.filter(action => !isFalsePositive(action));
}

// PREFIX NORMALIZATION
const PREFIX_NORMALIZATION = {
  'chaos_': ['animation_physics'],
  'mw_': ['manage_audio'],
  'bp_': ['manage_asset'],
  'blueprint_': ['manage_asset'],
  'audio_': ['manage_audio'],
  'anim_': ['animation_physics'],
  'vfx_': ['manage_effect'],
  'niagara_': ['manage_effect'],
  'seq_': ['manage_sequence'],
  'mrq_': ['manage_sequence'],
  'env_': ['build_environment'],
  'water_': ['build_environment'],
  'weather_': ['build_environment'],
  'xr_': ['manage_xr'],
  'vp_': ['manage_xr'],
  'geo_': ['manage_geometry'],
  'char_': ['manage_character', 'manage_character_avatar'],
  'ai_': ['manage_ai'],
  'nav_': ['manage_ai'],
  'bt_': ['manage_ai'],
  'net_': ['manage_networking'],
  'data_': ['manage_data'],
  'll_': ['manage_livelink'],
  'gas_': ['manage_gameplay_abilities', 'manage_attribute_sets', 'manage_gameplay_cues', 'test_gameplay_abilities'],
  'inv_': ['manage_character'],
  'usd_': ['manage_asset_plugins'],
  'abc_': ['manage_asset_plugins'],
  'gltf_': ['manage_asset_plugins'],
  'ds_': ['manage_asset_plugins'],
  'hda_': ['manage_asset_plugins'],
  'sbsar_': ['manage_asset_plugins'],
  'ic_': ['manage_asset_plugins'],
  'util_': ['manage_asset_plugins'],
};

// Map TS tools to C++ handler file patterns
const TOOL_TO_HANDLER_PATTERNS = {
  'manage_asset': ['AssetWorkflow', 'AssetQuery', 'BlueprintGraph', 'BlueprintCreation', 'Blueprint', 'SCS', 'MaterialGraph', 'MetaSound'],
  'control_actor': ['Control', 'Property'],
  'control_editor': ['Control', 'Input', 'Debug', 'Log', 'Insights', 'Modern'],
  'manage_level': ['Level', 'LevelStructure', 'PCG', 'WorldPartition'],
  'animation_physics': ['AnimationAuthoring', 'Animation', 'ControlRig', 'PhysicsDestruction'],
  'manage_effect': ['Effect', 'Niagara', 'NiagaraAuthoring', 'NiagaraAdvanced', 'NiagaraGraph'],
  'manage_sequence': ['Sequence', 'Sequencer', 'SequencerConsolidated', 'MovieRender'],
  'manage_audio': ['Audio', 'AudioAuthoring', 'AudioMiddleware', 'MetaSound'],
  'manage_lighting': ['Lighting', 'PostProcess'],
  'build_environment': ['Environment', 'Landscape', 'Foliage', 'Water', 'Weather'],
  'manage_character': ['Character', 'Interaction', 'Inventory'],
  'manage_combat': ['Combat', 'GAS'],
  'manage_ai': ['AI', 'AINPC', 'BehaviorTree', 'Navigation'],
  'manage_widget_authoring': ['Widget', 'WidgetAuthoring'],
  'manage_ui': ['Ui'],
  'manage_networking': ['Networking', 'Sessions', 'GameFramework'],
  'manage_volumes': ['Volume', 'Spline'],
  'manage_data': ['Data', 'Modding'],
  'manage_build': ['Build', 'Testing', 'Test'],
  'manage_editor_utilities': ['EditorUtilities', 'EditorFunction'],
  'manage_gameplay_systems': ['GameplaySystems'],
  'manage_gameplay_primitives': ['GameplayPrimitives'],
  'manage_character_avatar': ['CharacterAvatar'],
  'manage_asset_plugins': ['AssetPlugins', 'UtilityPlugins'],
  'manage_livelink': ['LiveLink'],
  'manage_xr': ['XRPlugins', 'VirtualProduction'],
  'manage_accessibility': ['Accessibility'],
  'manage_geometry': ['Geometry'],
  'manage_skeleton': ['Skeleton', 'Media'],
  'manage_material_authoring': ['MaterialAuthoring', 'Texture'],
  'manage_performance': ['Performance', 'Render'],
  'manage_motion_design': ['MotionDesign'],
  'manage_gameplay_abilities': ['GAS'],
  'manage_attribute_sets': ['GAS'],
  'manage_gameplay_cues': ['GAS'],
  'test_gameplay_abilities': ['GAS'],
  'manage_pipeline': [],
};

function normalizeActionForComparison(action, toolName) {
  for (const [prefix, tools] of Object.entries(PREFIX_NORMALIZATION)) {
    if (tools.includes(toolName) && action.startsWith(prefix)) {
      return action.substring(prefix.length);
    }
  }
  return action;
}

function extractCppActionsFromFile(filePath) {
  const content = readFileSync(filePath, 'utf-8');
  const actions = new Set();

  const equalityRegex = /(\w+)\s*==\s*TEXT\s*\(\s*"([a-z_][a-z0-9_]*)"\s*\)/gi;
  let match;
  while ((match = equalityRegex.exec(content)) !== null) {
    const variableName = match[1].toLowerCase();
    const actionName = match[2];
    if (ACTION_VARIABLES.has(variableName)) {
      actions.add(actionName);
    }
  }

  const equalsRegex = /(\w+)\.Equals\s*\(\s*TEXT\s*\(\s*"([a-z_][a-z0-9_]*)"\s*\)/gi;
  while ((match = equalsRegex.exec(content)) !== null) {
    const variableName = match[1].toLowerCase();
    const actionName = match[2];
    if (ACTION_VARIABLES.has(variableName)) {
      actions.add(actionName);
    }
  }

  const actionMatchesPatternRegex = /ActionMatchesPattern\s*\(\s*TEXT\s*\(\s*"([a-z_][a-z0-9_]*)"\s*\)\s*\)/gi;
  while ((match = actionMatchesPatternRegex.exec(content)) !== null) {
    actions.add(match[1]);
  }

  return Array.from(actions);
}

function getCppActionsForTool(toolName) {
  const patterns = TOOL_TO_HANDLER_PATTERNS[toolName] || [];
  const allActions = new Set();

  if (!existsSync(handlersDir)) return [];

  const files = readdirSync(handlersDir).filter(f => f.endsWith('.cpp'));
  
  for (const file of files) {
    const handlerName = file.replace('McpAutomationBridge_', '').replace('Handlers.cpp', '').replace('.cpp', '');
    
    if (patterns.some(p => handlerName.includes(p))) {
      const filePath = join(handlersDir, file);
      const actions = extractCppActionsFromFile(filePath);
      actions.forEach(a => allActions.add(a));
    }
  }

  // Filter out false positives
  return filterFalsePositives(Array.from(allActions));
}

function getTsActionsForTool(toolName) {
  for (const tool of consolidatedToolDefinitions) {
    if (tool.name === toolName) {
      return tool.inputSchema?.properties?.action?.enum || [];
    }
  }
  return [];
}

/**
 * Fetch tools from live MCP server via MCP protocol
 */
async function getLiveServerTools() {
  console.log('Starting live MCP server for 3-way comparison...');
  
  const serverCommand = 'node';
  const serverArgs = [join(repoRoot, 'dist', 'cli.js')];
  const serverEnv = {
    ...process.env,
    MOCK_UNREAL_CONNECTION: 'true',
    LOG_LEVEL: 'error'
  };

  let transport;
  let client;

  try {
    transport = new StdioClientTransport({
      command: serverCommand,
      args: serverArgs,
      cwd: repoRoot,
      stderr: 'pipe',
      env: serverEnv
    });

    client = new Client({
      name: 'sync-report-generator',
      version: '1.0.0'
    });

    await client.connect(transport);
    console.log('Connected to live MCP server');

    const toolsResponse = await client.listTools({});
    const tools = toolsResponse.tools || [];
    console.log(`Fetched ${tools.length} tools from live server`);

    // Build map of toolName -> actions
    const liveToolActions = new Map();
    for (const tool of tools) {
      const actions = tool.inputSchema?.properties?.action?.enum || [];
      liveToolActions.set(tool.name, actions);
    }

    return { tools, liveToolActions };
  } finally {
    if (client) {
      try { await client.close(); } catch { /* ignore */ }
    }
    if (transport) {
      try { await transport.close(); } catch { /* ignore */ }
    }
  }
}

function compareActionSets(setA, setB, toolName) {
  const aSet = new Set(setA);
  const bSet = new Set(setB);
  const aNormalizedMap = new Map();
  const bNormalizedMap = new Map();

  for (const a of setA) {
    const normalized = normalizeActionForComparison(a, toolName);
    aNormalizedMap.set(normalized, a);
    aNormalizedMap.set(a, a);
  }

  for (const b of setB) {
    const normalized = normalizeActionForComparison(b, toolName);
    bNormalizedMap.set(normalized, b);
    bNormalizedMap.set(b, b);
  }

  const onlyInA = [];
  const matched = [];
  for (const a of setA) {
    const hasExact = bSet.has(a);
    const normalized = normalizeActionForComparison(a, toolName);
    const hasNormalized = bNormalizedMap.has(normalized);
    if (hasExact || hasNormalized) {
      matched.push(a);
    } else {
      onlyInA.push(a);
    }
  }

  const onlyInB = [];
  for (const b of setB) {
    const hasExact = aSet.has(b);
    const normalized = normalizeActionForComparison(b, toolName);
    const hasNormalized = aNormalizedMap.has(normalized);
    if (!hasExact && !hasNormalized) {
      onlyInB.push(b);
    }
  }

  return { matched, onlyInA, onlyInB };
}

async function analyzeSynchronization(liveToolActions) {
  const results = [];
  const processedTools = new Set();

  for (const tool of consolidatedToolDefinitions) {
    if (processedTools.has(tool.name)) continue;
    processedTools.add(tool.name);

    const tsActions = getTsActionsForTool(tool.name);
    const cppActions = getCppActionsForTool(tool.name);
    const liveActions = liveToolActions.get(tool.name) || [];

    // TS vs C++
    const tsCpp = compareActionSets(tsActions, cppActions, tool.name);
    
    // TS vs Live
    const tsLive = compareActionSets(tsActions, liveActions, tool.name);
    
    // C++ vs Live
    const cppLive = compareActionSets(cppActions, liveActions, tool.name);

    const tsCppSync = tsActions.length > 0
      ? Math.round((tsCpp.matched.length / tsActions.length) * 100)
      : 100;

    const tsLiveSync = tsActions.length > 0
      ? Math.round((tsLive.matched.length / tsActions.length) * 100)
      : 100;

    results.push({
      toolName: tool.name,
      tsActions,
      cppActions,
      liveActions,
      tsCpp,
      tsLive,
      cppLive,
      tsCppSync,
      tsLiveSync,
    });
  }

  results.sort((a, b) => a.tsCppSync - b.tsCppSync);
  return results;
}

async function generateReport() {
  console.log('='.repeat(60));
  console.log('3-Way Sync Report Generator: TS ↔ C++ ↔ Live MCP');
  console.log('='.repeat(60));
  console.log('');

  // Fetch live server tools
  let liveToolActions;
  let liveToolCount = 0;
  let liveFailed = false;
  
  try {
    const liveData = await getLiveServerTools();
    liveToolActions = liveData.liveToolActions;
    liveToolCount = liveData.tools.length;
  } catch (error) {
    console.warn('Failed to connect to live MCP server:', error.message);
    console.warn('Proceeding with TS ↔ C++ comparison only');
    liveToolActions = new Map();
    liveFailed = true;
  }

  const results = await analyzeSynchronization(liveToolActions);
  
  // Calculate totals
  const totalTsActions = results.reduce((sum, r) => sum + r.tsActions.length, 0);
  const totalCppActions = results.reduce((sum, r) => sum + r.cppActions.length, 0);
  const totalLiveActions = results.reduce((sum, r) => sum + r.liveActions.length, 0);
  const totalTsCppMissing = results.reduce((sum, r) => sum + r.tsCpp.onlyInA.length, 0);
  const totalTsLiveMissing = results.reduce((sum, r) => sum + r.tsLive.onlyInA.length, 0);
  const totalTsCppMatched = results.reduce((sum, r) => sum + r.tsCpp.matched.length, 0);
  const totalTsLiveMatched = results.reduce((sum, r) => sum + r.tsLive.matched.length, 0);
  const overallTsCppSync = Math.round((totalTsCppMatched / totalTsActions) * 100);
  const overallTsLiveSync = Math.round((totalTsLiveMatched / totalTsActions) * 100);

  const lines = [
    '# 3-Way Action Synchronization Report',
    '',
    '> **TS ↔ C++ ↔ Live MCP Server**',
    '>',
    '> This report compares actions across three sources:',
    '> 1. **TS (Static)**: TypeScript tool definitions in source code',
    '> 2. **C++ (Static)**: C++ handler implementations in plugin',
    '> 3. **Live MCP (Runtime)**: What the actual running MCP server exposes to LLMs',
    '',
    `Generated: ${new Date().toISOString()}`,
    '',
  ];

  if (liveFailed) {
    lines.push('> ⚠️ **Warning**: Live MCP server connection failed. Live columns show N/A.');
    lines.push('');
  }

  lines.push(
    '## Executive Summary',
    '',
    '| Source | Tools | Actions |',
    '|--------|-------|---------|',
    `| TypeScript (Static) | ${results.length} | ${totalTsActions.toLocaleString()} |`,
    `| C++ Handlers (Static) | - | ${totalCppActions.toLocaleString()} |`,
    `| Live MCP Server | ${liveToolCount} | ${totalLiveActions.toLocaleString()} |`,
    '',
    '### Sync Metrics',
    '',
    '| Comparison | Matched | Missing | Sync % |',
    '|------------|---------|---------|--------|',
    `| TS → C++ | ${totalTsCppMatched.toLocaleString()} | ${totalTsCppMissing.toLocaleString()} | **${overallTsCppSync}%** |`,
    `| TS → Live | ${totalTsLiveMatched.toLocaleString()} | ${totalTsLiveMissing.toLocaleString()} | **${overallTsLiveSync}%** |`,
    '',
  );

  // Discrepancy detection
  const tsLiveDiscrepancies = results.filter(r => r.tsLive.onlyInA.length > 0 || r.tsLive.onlyInB.length > 0);
  if (tsLiveDiscrepancies.length > 0 && !liveFailed) {
    lines.push(
      '### ⚠️ TS vs Live Discrepancies',
      '',
      'Actions that differ between static TS definitions and live server exposure:',
      '',
    );
    for (const r of tsLiveDiscrepancies.slice(0, 5)) {
      if (r.tsLive.onlyInA.length > 0) {
        lines.push(`- **${r.toolName}**: ${r.tsLive.onlyInA.length} in TS but NOT in Live`);
      }
      if (r.tsLive.onlyInB.length > 0) {
        lines.push(`- **${r.toolName}**: ${r.tsLive.onlyInB.length} in Live but NOT in TS`);
      }
    }
    if (tsLiveDiscrepancies.length > 5) {
      lines.push(`- ... and ${tsLiveDiscrepancies.length - 5} more tools with discrepancies`);
    }
    lines.push('');
  }

  // Prefix normalization info
  lines.push(
    '## Prefix Normalization',
    '',
    'The following prefixes are stripped for comparison:',
    '',
    '| Prefix | Tools |',
    '|--------|-------|',
    '| `chaos_*` | animation_physics |',
    '| `mw_*`, `audio_*` | manage_audio |',
    '| `bp_*` | manage_asset |',
    '| `niagara_*` | manage_effect |',
    '| `seq_*`, `mrq_*` | manage_sequence |',
    '',
  );

  // Detailed sync table
  lines.push(
    '## Sync Status by Tool',
    '',
    '| Tool | TS | C++ | Live | TS→C++ | TS→Live |',
    '|------|----|----|------|--------|---------|',
  );

  for (const r of results) {
    const tsCppStatus = r.tsCppSync === 100 ? '✅' : r.tsCppSync >= 75 ? '🟡' : r.tsCppSync >= 50 ? '🟠' : '🔴';
    const tsLiveStatus = r.tsLiveSync === 100 ? '✅' : r.tsLiveSync >= 75 ? '🟡' : r.tsLiveSync >= 50 ? '🟠' : '🔴';
    const liveCount = liveFailed ? 'N/A' : r.liveActions.length;
    const tsLiveCol = liveFailed ? 'N/A' : `${tsLiveStatus} ${r.tsLiveSync}%`;
    lines.push(
      `| ${r.toolName} | ${r.tsActions.length} | ${r.cppActions.length} | ${liveCount} | ${tsCppStatus} ${r.tsCppSync}% | ${tsLiveCol} |`
    );
  }

  lines.push('', '---', '', '## Detailed Gap Analysis', '');

  // Show gaps for tools with issues
  for (const r of results) {
    const hasTsCppGap = r.tsCpp.onlyInA.length > 0 || r.tsCpp.onlyInB.length > 0;
    const hasTsLiveGap = !liveFailed && (r.tsLive.onlyInA.length > 0 || r.tsLive.onlyInB.length > 0);
    
    if (!hasTsCppGap && !hasTsLiveGap) continue;

    lines.push(`### ${r.toolName}`);
    lines.push('');
    lines.push(`| Source | Count |`);
    lines.push(`|--------|-------|`);
    lines.push(`| TS | ${r.tsActions.length} |`);
    lines.push(`| C++ | ${r.cppActions.length} |`);
    if (!liveFailed) {
      lines.push(`| Live | ${r.liveActions.length} |`);
    }
    lines.push('');

    // TS → C++ gaps
    if (r.tsCpp.onlyInA.length > 0) {
      lines.push(`#### TS → C++ Missing (${r.tsCpp.onlyInA.length})`);
      lines.push('');
      lines.push('Actions in TS but NOT implemented in C++:');
      lines.push('');
      lines.push('```');
      for (const action of r.tsCpp.onlyInA.sort().slice(0, 20)) {
        lines.push(action);
      }
      if (r.tsCpp.onlyInA.length > 20) {
        lines.push(`... and ${r.tsCpp.onlyInA.length - 20} more`);
      }
      lines.push('```');
      lines.push('');
    }

    if (r.tsCpp.onlyInB.length > 0) {
      lines.push(`#### C++ → TS Extra (${r.tsCpp.onlyInB.length})`);
      lines.push('');
      lines.push('Actions in C++ but NOT exposed in TS:');
      lines.push('');
      lines.push('```');
      for (const action of r.tsCpp.onlyInB.sort().slice(0, 20)) {
        lines.push(action);
      }
      if (r.tsCpp.onlyInB.length > 20) {
        lines.push(`... and ${r.tsCpp.onlyInB.length - 20} more`);
      }
      lines.push('```');
      lines.push('');
    }

    // TS → Live gaps (runtime discrepancies)
    if (!liveFailed && r.tsLive.onlyInA.length > 0) {
      lines.push(`#### ⚠️ TS → Live Missing (${r.tsLive.onlyInA.length})`);
      lines.push('');
      lines.push('**RUNTIME ISSUE**: Actions defined in TS but NOT exposed by live server:');
      lines.push('');
      lines.push('```');
      for (const action of r.tsLive.onlyInA.sort().slice(0, 20)) {
        lines.push(action);
      }
      if (r.tsLive.onlyInA.length > 20) {
        lines.push(`... and ${r.tsLive.onlyInA.length - 20} more`);
      }
      lines.push('```');
      lines.push('');
    }

    if (!liveFailed && r.tsLive.onlyInB.length > 0) {
      lines.push(`#### Live → TS Extra (${r.tsLive.onlyInB.length})`);
      lines.push('');
      lines.push('Actions in live server but NOT in TS definitions:');
      lines.push('');
      lines.push('```');
      for (const action of r.tsLive.onlyInB.sort().slice(0, 20)) {
        lines.push(action);
      }
      if (r.tsLive.onlyInB.length > 20) {
        lines.push(`... and ${r.tsLive.onlyInB.length - 20} more`);
      }
      lines.push('```');
      lines.push('');
    }

    lines.push('---', '');
  }

  // Implementation priority
  lines.push('## Implementation Priority', '');
  
  lines.push('### High Priority (Core Tools)', '');
  const coreTools = ['manage_asset', 'control_actor', 'control_editor', 'manage_level'];
  const coreMissing = results
    .filter(r => coreTools.includes(r.toolName))
    .flatMap(r => r.tsCpp.onlyInA.map(a => `${r.toolName}::${a}`));
  if (coreMissing.length > 0) {
    lines.push('```');
    coreMissing.slice(0, 20).forEach(a => lines.push(a));
    if (coreMissing.length > 20) lines.push(`... and ${coreMissing.length - 20} more`);
    lines.push('```');
  } else {
    lines.push('*All core tool actions are implemented!*');
  }
  lines.push('');

  lines.push('### Medium Priority (Frequently Used)', '');
  const mediumTools = ['animation_physics', 'manage_effect', 'manage_sequence', 'manage_audio', 'manage_lighting'];
  const mediumMissing = results
    .filter(r => mediumTools.includes(r.toolName))
    .flatMap(r => r.tsCpp.onlyInA.slice(0, 5).map(a => `${r.toolName}::${a}`));
  if (mediumMissing.length > 0) {
    lines.push('```');
    mediumMissing.forEach(a => lines.push(a));
    lines.push('```');
  } else {
    lines.push('*All medium priority actions are implemented!*');
  }
  lines.push('');

  // Generation metadata
  lines.push('## Metadata', '');
  lines.push('```json');
  lines.push(JSON.stringify({
    generatedAt: new Date().toISOString(),
    sources: {
      ts: { tools: results.length, actions: totalTsActions },
      cpp: { actions: totalCppActions },
      live: { tools: liveToolCount, actions: totalLiveActions, failed: liveFailed }
    },
    sync: {
      tsCpp: `${overallTsCppSync}%`,
      tsLive: liveFailed ? 'N/A' : `${overallTsLiveSync}%`
    }
  }, null, 2));
  lines.push('```');
  lines.push('');

  mkdirSync(dirname(outputPath), { recursive: true });
  writeFileSync(outputPath, lines.join('\n'));
  
  console.log('');
  console.log('='.repeat(60));
  console.log('Report Generated');
  console.log('='.repeat(60));
  console.log(`Output: ${outputPath}`);
  console.log(`Tools analyzed: ${results.length}`);
  console.log(`TS → C++ Sync: ${overallTsCppSync}%`);
  console.log(`TS → Live Sync: ${liveFailed ? 'N/A (server failed)' : overallTsLiveSync + '%'}`);
  console.log(`TS actions missing in C++: ${totalTsCppMissing}`);
  console.log('');
}

generateReport().catch(err => {
  console.error('Failed to generate report:', err);
  process.exit(1);
});
