// scripts/generate-canonical-registry.ts
//
// Generates every artifact derived from the hand-authored capability records
// (records/aggregate.ts): the TS registry module and neutral JSON, the parent
// tool definitions, the routing and cost indexes, the docs, and the native
// C++ parent registry and capability shards. Generated files are never
// hand-edited.
//
// Run:
//   node --loader ts-node/esm scripts/generate-canonical-registry.ts [--check]
//
// --check fails (exit 1) if any generated file differs byte-for-byte from what
// the records produce.

import { reportDrift, writeGeneratedFiles } from './lib/generated-files.js';
import { buildTargets, type GeneratedTarget } from './canonical-registry/targets.js';

async function buildManifest(): Promise<GeneratedTarget[]> {
  // Imported dynamically: a static import of the record aggregate trips a
  // ts-node/esm evaluation-order quirk when this script is the entry point.
  const { ALL_CAPABILITY_RECORDS } = await import('../src/tools/catalog/capabilities/records/aggregate.js');
  return buildTargets({ records: ALL_CAPABILITY_RECORDS });
}

async function main(): Promise<void> {
  const targets = await buildManifest();
  if (process.argv.includes('--check')) {
    if (reportDrift(targets, 'canonical-registry', 'npm run registry:generate')) process.exitCode = 1;
    else console.log('[canonical-registry] check: all generated artifacts are up to date.');
    return;
  }
  writeGeneratedFiles(targets);
  console.log(`[canonical-registry] wrote ${targets.length} generated artifacts.`);
}

main().catch((error) => {
  console.error('[canonical-registry] FAILED:', error);
  process.exitCode = 1;
});
