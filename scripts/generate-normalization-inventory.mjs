// Generator for the Task 5 normalization inventory artifact.
// Run: node --loader ts-node/esm scripts/generate-normalization-inventory.mjs [--check]
// --check: fail if the committed artifact would drift (no write).
import { fileURLToPath } from 'node:url';
import { dirname, resolve } from 'node:path';
import { generateInventory } from '../src/tools/catalog/capabilities/normalization/generate.ts';
import {
  artifactExists,
  readArtifact,
  stableStringify,
  writeArtifactAtomic,
} from '../src/tools/catalog/capabilities/normalization/io.ts';

const here = dirname(fileURLToPath(import.meta.url));
const ARTIFACT = resolve(here, '../src/tools/catalog/capabilities/normalization-inventory.json');

const checkMode = process.argv.slice(2).includes('--check');

const artifact = generateInventory();

if (checkMode) {
  if (!artifactExists(ARTIFACT)) {
    console.error('FAIL: committed artifact missing at', ARTIFACT);
    process.exit(1);
  }
  const committed = stableStringify(readArtifact(ARTIFACT));
  const current = stableStringify(artifact);
  if (committed !== current) {
    console.error('FAIL: artifact drift detected (--check). Regenerate to update.');
    process.exit(1);
  }
  console.log(
    `OK: artifact byte-stable, no drift (${artifact.occurrences.length} occurrences, ` +
      `${artifact.canonicalDefinitions.length} canonical definitions)`,
  );
  process.exit(0);
}

writeArtifactAtomic(ARTIFACT, artifact);
console.log(
  `OK: wrote ${ARTIFACT}\n` +
    `  occurrences=${artifact.occurrences.length} canonicalDefinitions=${artifact.canonicalDefinitions.length} ` +
    `contentSha256=${artifact.metadata.contentSha256.slice(0, 16)}...`,
);
