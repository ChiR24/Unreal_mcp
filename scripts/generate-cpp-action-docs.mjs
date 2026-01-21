#!/usr/bin/env node
/**
 * Auto-generate C++ action reference documentation from handler files
 * 
 * Usage: npm run docs:generate-cpp
 * Output: docs/cpp-action-reference.md
 */
import { readFileSync, writeFileSync, readdirSync, mkdirSync, existsSync } from 'fs';
import { dirname, join, basename } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const handlersDir = join(__dirname, '..', 'plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private');
const outputPath = join(__dirname, '..', 'docs', 'cpp-action-reference.md');

// Variable names that typically hold action strings
const ACTION_VARIABLES = new Set([
  'subaction',
  'lowersubaction',
  'lowersub',
  'lower',
  'actiontype',
  'action',
  'operation',
  'lowerpredicate',
  'lowertype',
  'lowerquality',
  'lowermode',
  'effectiveaction',  // Used in MetaSoundHandlers and other handlers with action normalization
]);

// Map C++ handler names to TS tool names
const HANDLER_TO_TOOL = {
  'AnimationAuthoring': ['animation_physics'],
  'ControlRig': ['animation_physics'],
  'PhysicsDestruction': ['animation_physics'],
  'Animation': ['animation_physics'],
  'AssetWorkflow': ['manage_asset'],
  'AssetQuery': ['manage_asset'],
  'BlueprintGraph': ['manage_asset'],
  'BlueprintCreation': ['manage_asset'],
  'Blueprint': ['manage_asset'],
  'SCS': ['manage_asset'],
  'MaterialGraph': ['manage_asset'],
  'MetaSound': ['manage_asset', 'manage_audio'],
  'Control': ['control_actor', 'control_editor'],
  'Property': ['control_actor'],
  'Level': ['manage_level'],
  'LevelStructure': ['manage_level'],
  'PCG': ['manage_level'],
  'WorldPartition': ['manage_level'],
  'Lighting': ['manage_lighting'],
  'PostProcess': ['manage_lighting'],
  'Environment': ['build_environment'],
  'Landscape': ['build_environment'],
  'Foliage': ['build_environment'],
  'Water': ['build_environment'],
  'Weather': ['build_environment'],
  'Effect': ['manage_effect'],
  'Niagara': ['manage_effect'],
  'NiagaraAuthoring': ['manage_effect'],
  'NiagaraAdvanced': ['manage_effect'],
  'NiagaraGraph': ['manage_effect'],
  'Sequence': ['manage_sequence'],
  'Sequencer': ['manage_sequence'],
  'SequencerConsolidated': ['manage_sequence'],
  'MovieRender': ['manage_sequence'],
  'Audio': ['manage_audio'],
  'AudioAuthoring': ['manage_audio'],
  'AudioMiddleware': ['manage_audio'],
  'Character': ['manage_character'],
  'Interaction': ['manage_character'],
  'Inventory': ['manage_character'],
  'Combat': ['manage_combat'],
  'GAS': ['manage_combat'],
  'AI': ['manage_ai'],
  'AINPC': ['manage_ai'],
  'BehaviorTree': ['manage_ai'],
  'Navigation': ['manage_ai'],
  'Widget': ['manage_widget_authoring'],
  'WidgetAuthoring': ['manage_widget_authoring'],
  'Ui': ['manage_ui'],
  'Networking': ['manage_networking'],
  'Sessions': ['manage_networking'],
  'GameFramework': ['manage_networking'],
  'Volume': ['manage_volumes'],
  'Spline': ['manage_volumes'],
  'Data': ['manage_data'],
  'Modding': ['manage_data'],
  'Build': ['manage_build'],
  'Testing': ['manage_build'],
  'EditorUtilities': ['manage_editor_utilities'],
  'EditorFunction': ['manage_editor_utilities'],
  'GameplaySystems': ['manage_gameplay_systems'],
  'GameplayPrimitives': ['manage_gameplay_primitives'],
  'CharacterAvatar': ['manage_character_avatar'],
  'AssetPlugins': ['manage_asset_plugins'],
  'UtilityPlugins': ['manage_asset_plugins'],
  'LiveLink': ['manage_livelink'],
  'XRPlugins': ['manage_xr'],
  'VirtualProduction': ['manage_xr'],
  'Accessibility': ['manage_accessibility'],
  'Geometry': ['manage_geometry'],
  'Skeleton': ['manage_skeleton'],
  'Media': ['manage_skeleton'],
  'Texture': ['manage_material_authoring'],
  'MaterialAuthoring': ['manage_material_authoring'],
  'Performance': ['manage_performance'],
  'Render': ['manage_performance'],
  'MotionDesign': ['manage_motion_design'],
  'Log': ['control_editor'],
  'Input': ['control_editor'],
  'Debug': ['control_editor'],
  'Insights': ['control_editor'],
  'Test': ['manage_build'],
  'Modern': ['control_editor'],
};

function extractActionsFromFile(filePath) {
  const content = readFileSync(filePath, 'utf-8');
  const lines = content.split('\n');
  const actions = [];
  const seenActions = new Set();

  for (let lineNum = 0; lineNum < lines.length; lineNum++) {
    const line = lines[lineNum];
    const regex = /(\w+)\s*==\s*TEXT\s*\(\s*"([a-z_][a-z0-9_]*)"\s*\)/gi;
    let match;
    while ((match = regex.exec(line)) !== null) {
      const variableName = match[1].toLowerCase();
      const actionName = match[2];
      if (ACTION_VARIABLES.has(variableName) && !seenActions.has(actionName)) {
        seenActions.add(actionName);
        actions.push({
          action: actionName,
          line: lineNum + 1,
          variableName: match[1],
        });
      }
    }
  }

  return actions;
}

function parseHandlerFile(filePath) {
  const fileName = basename(filePath);
  if (!fileName.includes('Handlers.cpp')) return null;

  const content = readFileSync(filePath, 'utf-8');
  const lineCount = content.split('\n').length;
  const actions = extractActionsFromFile(filePath);

  const handlerName = fileName
    .replace('McpAutomationBridge_', '')
    .replace('Handlers.cpp', '')
    .replace('.cpp', '');

  return {
    fileName,
    handlerName,
    actions: actions.map(a => a.action),
    lineCount,
  };
}

function mapHandlerToTool(handlerName) {
  return HANDLER_TO_TOOL[handlerName] || ['unknown'];
}

function generateDocs() {
  if (!existsSync(handlersDir)) {
    console.error(`Handler directory not found: ${handlersDir}`);
    process.exit(1);
  }

  const files = readdirSync(handlersDir).filter(f => f.endsWith('.cpp'));
  const handlers = [];
  const allActions = new Set();
  const actionToHandler = new Map();

  for (const file of files) {
    const filePath = join(handlersDir, file);
    const handlerInfo = parseHandlerFile(filePath);
    if (handlerInfo && handlerInfo.actions.length > 0) {
      handlers.push(handlerInfo);
      for (const action of handlerInfo.actions) {
        allActions.add(action);
        const existing = actionToHandler.get(action) || [];
        existing.push(handlerInfo.handlerName);
        actionToHandler.set(action, existing);
      }
    }
  }

  handlers.sort((a, b) => b.actions.length - a.actions.length);

  const lines = [
    '# C++ Action Reference',
    '',
    '> Auto-generated from C++ handler files. Do not edit manually.',
    '',
    `Generated: ${new Date().toISOString()}`,
    '',
    '## Summary',
    '',
    `| Metric | Count |`,
    `|--------|-------|`,
    `| Handler Files | ${handlers.length} |`,
    `| Total Actions | ${allActions.size} |`,
    `| Lines of Code | ${handlers.reduce((sum, h) => sum + h.lineCount, 0).toLocaleString()} |`,
    '',
    '## Table of Contents',
    '',
  ];

  for (const handler of handlers) {
    const anchor = handler.handlerName.toLowerCase().replace(/[^a-z0-9]/g, '-');
    const tools = mapHandlerToTool(handler.handlerName).join(', ');
    lines.push(`- [${handler.handlerName}](#${anchor}) (${handler.actions.length} actions) → ${tools}`);
  }
  lines.push('', '---', '');

  for (const handler of handlers) {
    const tools = mapHandlerToTool(handler.handlerName);
    lines.push(`## ${handler.handlerName}`);
    lines.push('');
    lines.push(`**File:** \`${handler.fileName}\``);
    lines.push('');
    lines.push(`**Lines:** ${handler.lineCount.toLocaleString()}`);
    lines.push('');
    lines.push(`**Maps to TS Tool(s):** ${tools.map(t => `\`${t}\``).join(', ')}`);
    lines.push('');
    lines.push(`**Actions (${handler.actions.length}):**`);
    lines.push('');
    lines.push('| Action | Status |');
    lines.push('|--------|--------|');
    for (const action of handler.actions.sort()) {
      lines.push(`| \`${action}\` | Implemented |`);
    }
    lines.push('', '---', '');
  }

  lines.push('## Alphabetical Action Index');
  lines.push('');
  lines.push('| Action | Handler(s) |');
  lines.push('|--------|------------|');
  const sortedActions = Array.from(allActions).sort();
  for (const action of sortedActions) {
    const hs = actionToHandler.get(action) || [];
    lines.push(`| \`${action}\` | ${hs.join(', ')} |`);
  }
  lines.push('');

  mkdirSync(dirname(outputPath), { recursive: true });
  writeFileSync(outputPath, lines.join('\n'));
  console.log(`Generated: ${outputPath}`);
  console.log(`Handler files processed: ${handlers.length}`);
  console.log(`Total C++ actions: ${allActions.size}`);
}

generateDocs();
