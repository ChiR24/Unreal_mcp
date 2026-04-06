#!/usr/bin/env node
/**
 * generate-tool-schemas.mjs
 *
 * Reads consolidated-tool-definitions.ts (via tsx), extracts
 * {name, description, inputSchema} for every tool, and writes
 * plugins/McpAutomationBridge/Resources/MCP/tool-schemas.json
 *
 * Usage:  npx tsx scripts/generate-tool-schemas.mjs
 */

import { mkdirSync, writeFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { platform } from 'node:os';
import { fileURLToPath } from 'node:url';

if (platform() === 'win32') {
  console.error('This script is not supported on Windows. Use macOS or Linux.');
  process.exit(1);
}

const __dirname = dirname(fileURLToPath(import.meta.url));
const ROOT = resolve(__dirname, '..');

// Dynamic import of the TS source — tsx handles transpilation
const { consolidatedToolDefinitions } = await import(
  resolve(ROOT, 'src/tools/consolidated-tool-definitions.ts')
);

// Extract what the C++ plugin needs: name, description, category, inputSchema
const tools = consolidatedToolDefinitions.map((t) => ({
  name: t.name,
  description: t.description,
  category: t.category || 'utility',
  inputSchema: t.inputSchema,
}));

const output = { tools };
const json = JSON.stringify(output, null, 2);

const outPath = resolve(
  ROOT,
  'plugins/McpAutomationBridge/Resources/MCP/tool-schemas.json'
);
mkdirSync(dirname(outPath), { recursive: true });
writeFileSync(outPath, json + '\n', 'utf-8');

console.log(`Written ${tools.length} tool schemas to ${outPath}`);
