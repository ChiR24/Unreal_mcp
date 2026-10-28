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

/**
 * Record aliases written <parent tool>.<action> that name no {tool, action} pair: the gateway serves that tool and
 * action as the record (FMcpCanonicalRecordIndex::FindByAliasPair), so a case calling one is checked against the
 * record's own parameters. "tool\taction" -> that record's input property names; an alias two records share serves
 * neither and is left out.
 */
export function readAliasPairs(registryPath = canonicalRegistryJsonPath) {
  const registry = JSON.parse(fs.readFileSync(registryPath, 'utf8'));
  const records = Array.isArray(registry.records) ? registry.records : [];
  const parents = new Set(records.map((record) => record?.routing?.parentTool));
  const legacy = new Set(records.flatMap((record) => (record.legacyIds ?? []).map((pair) => `${pair.tool}\t${pair.action}`)));
  const owners = new Map();
  for (const record of records) {
    for (const alias of Array.isArray(record.aliases) ? record.aliases : []) {
      const [tool, action, ...rest] = String(alias).split('.');
      const key = `${tool}\t${action}`;
      if (rest.length > 0 || !parents.has(tool) || legacy.has(key)) continue;
      owners.set(key, [...(owners.get(key) ?? []), new Set(Object.keys(record.schemas?.input?.properties ?? {}))]);
    }
  }
  return new Map([...owners].filter(([, sets]) => sets.length === 1).map(([key, sets]) => [key, sets[0]]));
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
