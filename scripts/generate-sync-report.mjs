#!/usr/bin/env node
/**
 * Generate synchronization report between TS and C++ actions
 * 
 * Usage: npm run docs:sync-report
 * Output: docs/action-sync-report.md
 */
import { readFileSync, writeFileSync, readdirSync, mkdirSync, existsSync } from 'fs';
import { dirname, join, basename } from 'path';
import { fileURLToPath } from 'url';

// Import tool definitions
import { consolidatedToolDefinitions } from '../dist/tools/consolidated-tool-definitions.js';

const __dirname = dirname(fileURLToPath(import.meta.url));
const handlersDir = join(__dirname, '..', 'plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private');
const outputPath = join(__dirname, '..', 'docs', 'action-sync-report.md');

const ACTION_VARIABLES = new Set([
  'subaction', 'lowersubaction', 'lowersub', 'lower', 'actiontype',
  'action', 'operation', 'lowerpredicate', 'lowertype', 'lowerquality', 'lowermode',
  'effectiveaction',  // Used in MetaSoundHandlers and other handlers with action normalization
]);

// PREFIX NORMALIZATION
// TS uses prefixed actions (chaos_*, mw_*, bp_*, audio_*) while C++ uses unprefixed versions
// This map defines prefixes to strip for comparison and which tools they apply to
const PREFIX_NORMALIZATION = {
  'chaos_': ['animation_physics'],
  'mw_': ['manage_audio'],
  'bp_': ['manage_asset'],
  'blueprint_': ['manage_asset'],  // C++ uses blueprint_* while TS uses bp_*
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
  'bt_': ['manage_ai'],  // Behavior Tree actions use bt_* prefix in TS
  'net_': ['manage_networking'],
  'data_': ['manage_data'],
  'll_': ['manage_livelink'],
  'gas_': ['manage_gameplay_abilities', 'manage_attribute_sets', 'manage_gameplay_cues', 'test_gameplay_abilities'],
  // Inventory system prefixes
  'inv_': ['manage_character'],
  // Asset plugin prefixes
  'usd_': ['manage_asset_plugins'],
  'abc_': ['manage_asset_plugins'],
  'gltf_': ['manage_asset_plugins'],
  'ds_': ['manage_asset_plugins'],
  'hda_': ['manage_asset_plugins'],
  'sbsar_': ['manage_asset_plugins'],
  'ic_': ['manage_asset_plugins'],
  'util_': ['manage_asset_plugins'],  // TS uses util_* prefix for utility plugin actions
};

/**
 * Normalize action name by stripping known prefixes for comparison
 * @param {string} action - Action name to normalize
 * @param {string} toolName - Tool this action belongs to
 * @returns {string} Normalized action name
 */
function normalizeActionForComparison(action, toolName) {
  for (const [prefix, tools] of Object.entries(PREFIX_NORMALIZATION)) {
    if (tools.includes(toolName) && action.startsWith(prefix)) {
      return action.substring(prefix.length);
    }
  }
  return action;
}

/**
 * Check if two actions match after normalization
 * @param {string} tsAction - TS action name
 * @param {string} cppAction - C++ action name 
 * @param {string} toolName - Tool for context
 * @returns {boolean} Whether actions match
 */
function actionsMatch(tsAction, cppAction, toolName) {
  if (tsAction === cppAction) return true;
  
  const normalizedTs = normalizeActionForComparison(tsAction, toolName);
  const normalizedCpp = normalizeActionForComparison(cppAction, toolName);
  
  return normalizedTs === normalizedCpp || 
         tsAction === normalizedCpp || 
         normalizedTs === cppAction;
}

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

function extractCppActionsFromFile(filePath) {
  const content = readFileSync(filePath, 'utf-8');
  const actions = new Set();

  // Pattern 1: Variable == TEXT("action") - equality check
  const equalityRegex = /(\w+)\s*==\s*TEXT\s*\(\s*"([a-z_][a-z0-9_]*)"\s*\)/gi;
  let match;
  while ((match = equalityRegex.exec(content)) !== null) {
    const variableName = match[1].toLowerCase();
    const actionName = match[2];
    if (ACTION_VARIABLES.has(variableName)) {
      actions.add(actionName);
    }
  }

  // Pattern 2: Variable.Equals(TEXT("action"), ...) - Equals method call
  const equalsRegex = /(\w+)\.Equals\s*\(\s*TEXT\s*\(\s*"([a-z_][a-z0-9_]*)"\s*\)/gi;
  while ((match = equalsRegex.exec(content)) !== null) {
    const variableName = match[1].toLowerCase();
    const actionName = match[2];
    if (ACTION_VARIABLES.has(variableName)) {
      actions.add(actionName);
    }
  }

  // Pattern 3: ActionMatchesPattern(TEXT("action")) - helper function for action matching
  const actionMatchesPatternRegex = /ActionMatchesPattern\s*\(\s*TEXT\s*\(\s*"([a-z_][a-z0-9_]*)"\s*\)\s*\)/gi;
  while ((match = actionMatchesPatternRegex.exec(content)) !== null) {
    const actionName = match[1];
    actions.add(actionName);
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

  return Array.from(allActions);
}

function getTsActionsForTool(toolName) {
  for (const tool of consolidatedToolDefinitions) {
    if (tool.name === toolName) {
      return tool.inputSchema?.properties?.action?.enum || [];
    }
  }
  return [];
}

function analyzeSynchronization() {
  const results = [];
  const processedTools = new Set();

  for (const tool of consolidatedToolDefinitions) {
    if (processedTools.has(tool.name)) continue;
    processedTools.add(tool.name);

    const tsActions = getTsActionsForTool(tool.name);
    const cppActions = getCppActionsForTool(tool.name);
    
    // Build normalized lookup sets for C++ actions
    const cppSet = new Set(cppActions);
    const cppNormalizedMap = new Map(); // normalized -> original
    for (const cppAction of cppActions) {
      const normalized = normalizeActionForComparison(cppAction, tool.name);
      cppNormalizedMap.set(normalized, cppAction);
      cppNormalizedMap.set(cppAction, cppAction); // also map exact
    }
    
    // Find TS actions truly missing in C++ (considering normalization)
    const missingInCpp = [];
    const matchedTsActions = [];
    for (const tsAction of tsActions) {
      const hasExactMatch = cppSet.has(tsAction);
      const normalizedTs = normalizeActionForComparison(tsAction, tool.name);
      const hasNormalizedMatch = cppNormalizedMap.has(normalizedTs);
      
      if (hasExactMatch || hasNormalizedMatch) {
        matchedTsActions.push(tsAction);
      } else {
        missingInCpp.push(tsAction);
      }
    }
    
    // Build normalized lookup for TS actions
    const tsSet = new Set(tsActions);
    const tsNormalizedMap = new Map();
    for (const tsAction of tsActions) {
      const normalized = normalizeActionForComparison(tsAction, tool.name);
      tsNormalizedMap.set(normalized, tsAction);
      tsNormalizedMap.set(tsAction, tsAction);
    }
    
    // Find C++ actions truly missing in TS (considering normalization)
    const missingInTs = [];
    for (const cppAction of cppActions) {
      const hasExactMatch = tsSet.has(cppAction);
      const normalizedCpp = normalizeActionForComparison(cppAction, tool.name);
      const hasNormalizedMatch = tsNormalizedMap.has(normalizedCpp);
      
      if (!hasExactMatch && !hasNormalizedMatch) {
        missingInTs.push(cppAction);
      }
    }

    const syncPercentage = tsActions.length > 0
      ? Math.round(((tsActions.length - missingInCpp.length) / tsActions.length) * 100)
      : 100;

    results.push({
      toolName: tool.name,
      tsActions,
      cppActions,
      missingInCpp,
      missingInTs,
      matchedCount: matchedTsActions.length,
      syncPercentage,
    });
  }

  results.sort((a, b) => a.syncPercentage - b.syncPercentage);
  return results;
}

function generateReport() {
  const results = analyzeSynchronization();
  
  const totalTsActions = results.reduce((sum, r) => sum + r.tsActions.length, 0);
  const totalCppActions = results.reduce((sum, r) => sum + r.cppActions.length, 0);
  const totalMissingInCpp = results.reduce((sum, r) => sum + r.missingInCpp.length, 0);
  const totalMissingInTs = results.reduce((sum, r) => sum + r.missingInTs.length, 0);
  const totalMatched = results.reduce((sum, r) => sum + r.matchedCount, 0);
  const overallSync = Math.round(((totalTsActions - totalMissingInCpp) / totalTsActions) * 100);

  const lines = [
    '# TS/C++ Action Synchronization Report',
    '',
    '> Auto-generated. Compares TypeScript tool definitions with C++ handler implementations.',
    '> **Note:** Uses prefix normalization (chaos_*, mw_*, bp_*, etc.) to match actions across naming conventions.',
    '',
    `Generated: ${new Date().toISOString()}`,
    '',
    '## Executive Summary',
    '',
    '| Metric | Count |',
    '|--------|-------|',
    `| Total TS Actions | ${totalTsActions.toLocaleString()} |`,
    `| Total C++ Actions | ${totalCppActions.toLocaleString()} |`,
    `| Matched (TS→C++) | ${totalMatched.toLocaleString()} |`,
    `| Missing in C++ | ${totalMissingInCpp.toLocaleString()} |`,
    `| Extra in C++ | ${totalMissingInTs.toLocaleString()} |`,
    `| **Overall Sync** | **${overallSync}%** |`,
    '',
    '## Prefix Normalization Applied',
    '',
    'The following prefixes are stripped for comparison to handle naming convention differences:',
    '',
    '| TS Prefix | Applied To Tools |',
    '|-----------|------------------|',
    '| `chaos_*` | animation_physics |',
    '| `mw_*` | manage_audio |',
    '| `bp_*` | manage_asset |',
    '| `audio_*` | manage_audio |',
    '| `niagara_*` | manage_effect |',
    '| `seq_*`, `mrq_*` | manage_sequence |',
    '| `water_*`, `weather_*` | build_environment |',
    '',
    '## Sync Status by Tool',
    '',
    '| Tool | TS | C++ | Matched | Missing | Extra | Sync |',
    '|------|----|----|---------|---------|-------|------|',
  ];

  for (const result of results) {
    const status = result.syncPercentage === 100 ? '✅' :
                   result.syncPercentage >= 75 ? '🟡' :
                   result.syncPercentage >= 50 ? '🟠' : '🔴';
    lines.push(
      `| ${result.toolName} | ${result.tsActions.length} | ${result.cppActions.length} | ` +
      `${result.matchedCount} | ${result.missingInCpp.length} | ${result.missingInTs.length} | ${status} ${result.syncPercentage}% |`
    );
  }

  lines.push('', '---', '', '## Detailed Gap Analysis', '');

  for (const result of results) {
    if (result.missingInCpp.length === 0 && result.missingInTs.length === 0) continue;

    lines.push(`### ${result.toolName}`);
    lines.push('');
    lines.push(`**TS Actions:** ${result.tsActions.length} | **C++ Actions:** ${result.cppActions.length} | **Sync:** ${result.syncPercentage}%`);
    lines.push('');

    if (result.missingInCpp.length > 0) {
      lines.push(`#### Missing in C++ (${result.missingInCpp.length})`);
      lines.push('');
      lines.push('These actions are defined in TypeScript but have NO C++ implementation:');
      lines.push('');
      lines.push('```');
      for (const action of result.missingInCpp.sort()) {
        lines.push(action);
      }
      lines.push('```');
      lines.push('');
    }

    if (result.missingInTs.length > 0) {
      lines.push(`#### Extra in C++ (${result.missingInTs.length})`);
      lines.push('');
      lines.push('These actions are in C++ but NOT exposed in TypeScript:');
      lines.push('');
      lines.push('```');
      for (const action of result.missingInTs.sort()) {
        lines.push(action);
      }
      lines.push('```');
      lines.push('');
    }

    lines.push('---', '');
  }

  lines.push('## Implementation Priority', '');
  lines.push('### High Priority (Core Tools)', '');
  const coreMissing = results
    .filter(r => ['manage_asset', 'control_actor', 'control_editor', 'manage_level'].includes(r.toolName))
    .flatMap(r => r.missingInCpp.map(a => `${r.toolName}::${a}`));
  if (coreMissing.length > 0) {
    lines.push('```');
    coreMissing.forEach(a => lines.push(a));
    lines.push('```');
  } else {
    lines.push('*All core tool actions are implemented!*');
  }
  lines.push('');

  lines.push('### Medium Priority (Frequently Used)', '');
  const mediumMissing = results
    .filter(r => ['animation_physics', 'manage_effect', 'manage_sequence', 'manage_audio', 'manage_lighting'].includes(r.toolName))
    .flatMap(r => r.missingInCpp.slice(0, 10).map(a => `${r.toolName}::${a}`));
  if (mediumMissing.length > 0) {
    lines.push('```');
    mediumMissing.forEach(a => lines.push(a));
    lines.push('```');
  } else {
    lines.push('*All medium priority actions are implemented!*');
  }
  lines.push('');

  const xrMissing = results.find(r => r.toolName === 'manage_xr')?.missingInCpp.length || 0;
  const pluginMissing = results.find(r => r.toolName === 'manage_asset_plugins')?.missingInCpp.length || 0;
  lines.push('### Lower Priority (Plugin/Optional)', '');
  lines.push(`Tools like \`manage_xr\` (${xrMissing} missing) and \`manage_asset_plugins\` (${pluginMissing} missing) have many missing actions but are optional plugin integrations.`);
  lines.push('');

  mkdirSync(dirname(outputPath), { recursive: true });
  writeFileSync(outputPath, lines.join('\n'));
  console.log(`Generated: ${outputPath}`);
  console.log(`Tools analyzed: ${results.length}`);
  console.log(`Overall sync: ${overallSync}%`);
  console.log(`Total missing in C++: ${totalMissingInCpp}`);
}

generateReport();
