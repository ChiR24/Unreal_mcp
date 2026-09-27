import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const auditFile = fileURLToPath(import.meta.url);
const testsDir = path.dirname(auditFile);

export const repoRoot = path.resolve(testsDir, '..');
export const testsRoot = path.join(repoRoot, 'tests/mcp-tools');
export const integrationSuitePath = path.join(repoRoot, 'tests/integration.mjs');
export const reportsDir = path.join(repoRoot, 'tests/reports');

// The audit measures the parent tool surface the server serves: the gateway
// manifest, generated from the parent definitions the records derive.
const gatewayManifestPath = path.join(repoRoot, 'src/gateway/gateway-manifest.generated.json');
const canonicalRegistryJsonPath = path.join(
  repoRoot,
  'src/tools/catalog/capabilities/generated/canonical-registry.generated.json'
);

/**
 * Folded legacy actions per parent tool. A folded family advertises one
 * action, but every name it replaced is still a callable {tool, action} pair,
 * so a static test case that exercises an old name covers a declared action,
 * not an extra one.
 */
export function readFoldedActionsByTool(registryPath = canonicalRegistryJsonPath) {
  const registry = JSON.parse(fs.readFileSync(registryPath, 'utf8'));
  const byTool = new Map();
  for (const record of Array.isArray(registry.records) ? registry.records : []) {
    const tool = record?.routing?.parentTool;
    if (typeof tool !== 'string') continue;
    for (const legacy of Array.isArray(record.legacyIds) ? record.legacyIds : []) {
      if (legacy?.folded === undefined || typeof legacy.action !== 'string') continue;
      if (!byTool.has(tool)) byTool.set(tool, new Set());
      byTool.get(tool).add(legacy.action);
    }
  }
  return byTool;
}

export function readRuntimeFacadeToolDefinitions(manifestPath = gatewayManifestPath) {
  const manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
  if (!Array.isArray(manifest.tools)) {
    throw new Error(`${manifestPath} declares no tools array`);
  }
  if (manifest.tools.length === 0) {
    throw new Error(
      `${manifestPath} declares zero parent tool definitions; `
      + 'refusing to run a vacuous audit against an empty generated surface.'
    );
  }
  return manifest.tools;
}
