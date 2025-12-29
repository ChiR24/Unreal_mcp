#!/usr/bin/env node
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(__dirname, '..');

function ok(msg) { console.log(`[ok] ${msg}`); }
function warn(msg) { console.warn(`[warn] ${msg}`); }
function fail(msg) { console.error(`[fail] ${msg}`); process.exitCode = 1; }

try {
  const pluginDir = path.resolve(repoRoot, 'plugins', 'McpAutomationBridge');
  if (fs.existsSync(pluginDir)) ok(`Plugin folder found: ${pluginDir}`); else fail(`Plugin folder missing: ${pluginDir}`);
  const uplugin = path.join(pluginDir, 'McpAutomationBridge.uplugin');
  if (fs.existsSync(uplugin)) ok('uplugin present'); else warn('uplugin missing');
  const port = process.env.MCP_AUTOMATION_WS_PORT || '8090';
  ok(`Configured port: ${port}`);
  console.log('Verification complete.');
} catch (e) {
  fail(e?.message || String(e));
}
