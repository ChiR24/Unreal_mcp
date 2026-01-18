#!/usr/bin/env node
/**
 * Auto-generate action reference documentation from tool definitions
 * 
 * Usage: npm run docs:generate
 * Output: docs/action-reference.md
 */
import { writeFileSync, mkdirSync } from 'fs';
import { dirname, join } from 'path';
import { fileURLToPath } from 'url';

// Import tool definitions (use .js extension for ESM)
import { consolidatedToolDefinitions } from '../src/tools/consolidated-tool-definitions.js';

const __dirname = dirname(fileURLToPath(import.meta.url));
const outputPath = join(__dirname, '..', 'docs', 'action-reference.md');

interface ActionSchema {
  properties?: {
    action?: {
      enum?: string[];
      description?: string;
    };
    [key: string]: unknown;
  };
  required?: string[];
}

function generateDocs(): void {
  const lines: string[] = [
    '# Action Reference',
    '',
    '> Auto-generated from tool definitions. Do not edit manually.',
    '',
    `Generated: ${new Date().toISOString()}`,
    '',
    `Total Tools: ${consolidatedToolDefinitions.length}`,
    '',
    '## Table of Contents',
    '',
  ];

  // Count total actions
  let totalActions = 0;
  for (const tool of consolidatedToolDefinitions) {
    const schema = tool.inputSchema as ActionSchema;
    const actions = schema?.properties?.action?.enum || [];
    totalActions += actions.length;
  }
  lines.push(`Total Actions: ${totalActions}`);
  lines.push('');

  // Generate TOC (use Set to prevent duplicates)
  const seenTools = new Set<string>();
  for (const tool of consolidatedToolDefinitions) {
    if (seenTools.has(tool.name)) continue;
    seenTools.add(tool.name);
    const schema = tool.inputSchema as ActionSchema;
    const actions = schema?.properties?.action?.enum || [];
    const anchor = tool.name.replace(/_/g, '-');
    lines.push(`- [${tool.name}](#${anchor}) (${actions.length} actions)`);
  }
  lines.push('');
  lines.push('---');
  lines.push('');

  // Generate tool sections (use seenTools Set to prevent duplicates)
  seenTools.clear();
  for (const tool of consolidatedToolDefinitions) {
    if (seenTools.has(tool.name)) continue;
    seenTools.add(tool.name);
    lines.push(`## ${tool.name}`);
    lines.push('');
    if (tool.category) {
      lines.push(`**Category:** ${tool.category}`);
      lines.push('');
    }
    lines.push(tool.description || '*No description*');
    lines.push('');
    
    // Get actions from inputSchema
    const schema = tool.inputSchema as ActionSchema;
    const actions = schema?.properties?.action?.enum || [];
    
    if (actions.length > 0) {
      lines.push('### Actions');
      lines.push('');
      lines.push('| Action | Description |');
      lines.push('|--------|-------------|');
      for (const action of actions) {
        // Actions are just listed; descriptions would require deeper schema parsing
        lines.push(`| \`${action}\` | - |`);
      }
      lines.push('');
    } else {
      lines.push('*No actions defined*');
      lines.push('');
    }

    lines.push('---');
    lines.push('');
  }

  // Ensure docs directory exists
  mkdirSync(dirname(outputPath), { recursive: true });
  
  // Write file
  writeFileSync(outputPath, lines.join('\n'));
  // eslint-disable-next-line no-console
  console.log(`Generated: ${outputPath}`);
  // eslint-disable-next-line no-console
  console.log(`Tools documented: ${consolidatedToolDefinitions.length}`);
  // eslint-disable-next-line no-console
  console.log(`Total actions: ${totalActions}`);
}

generateDocs();
