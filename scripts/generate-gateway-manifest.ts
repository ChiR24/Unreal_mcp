// scripts/generate-gateway-manifest.ts
// Deterministic generator for the neutral gateway manifest.
// Source of truth: the generated parent tool definitions (from capability records).
// Emits src/gateway/gateway-manifest.generated.json, which src/gateway/gateway-manifest.ts
// imports (compiled into dist) and the test harnesses read.
// Run: node --loader ts-node/esm scripts/generate-gateway-manifest.ts [--check]

import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { generatedParentToolDefinitions } from '../src/tools/catalog/capabilities/generated/parent-tool-definitions.generated.js';
import type { ToolDefinition } from '../src/tools/definitions/shared/tool-definition.js';
import { buildGatewayManifest } from './gateway-manifest/build.js';
import { type GeneratedTarget, reportDrift, writeGeneratedFiles } from './lib/generated-files.js';

export const prettyManifest = (defs: readonly ToolDefinition[]): string =>
  JSON.stringify(buildGatewayManifest(defs), null, 2);

function buildProductionTargets(root: string): GeneratedTarget[] {
  return [[resolve(root, 'src/gateway/gateway-manifest.generated.json'), `${prettyManifest(generatedParentToolDefinitions)}\n`]];
}

function main(): void {
  const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
  const targets = buildProductionTargets(root);

  if (process.argv.slice(2).includes('--check')) {
    if (reportDrift(targets, 'gateway-manifest', 'node --loader ts-node/esm scripts/generate-gateway-manifest.ts')) process.exitCode = 1;
    else console.log('[gateway-manifest] check: manifest artifacts are up to date.');
  } else {
    writeGeneratedFiles(targets);
    console.log(`[gateway-manifest] wrote ${targets.length} manifest artifacts.`);
  }
}

if (process.argv[1]?.endsWith('generate-gateway-manifest.ts')) main();
