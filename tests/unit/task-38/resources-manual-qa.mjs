// tests/unit/task-38/resources-manual-qa.mjs
// Task 38 lane A - MANUAL QA driver (outside Vitest). Drives the REAL production
// TypeScript resource engine (imported from the current dist/ build) for one
// happy catalog read and one unknown/stale error, normalizes both, and compares
// them against the native `/mcp` oracle values. Emits a BINARY verdict to
// .omo/evidence/task-38/resources-manual-qa.json: PASS only when the normalized
// values AND error codes match across transports. It now reports PASS: the Task 38
// remediation aligned the native runtime (McpResourceReadContent returns bounded
// revisioned data for the catalog and Classify returns RESOURCE_NOT_FOUND for an
// unknown uri), so it matches the real TS data and RESOURCE_NOT_FOUND.
//
// RUNTIME BLOCKER: the native side is inline oracle constants (transcribed from
// McpNativeTransportPrimitives.cpp), because the C++ /mcp surface cannot be
// executed here (no live editor / packaged plugin). This driver executes the TS
// side for real; the native side is modelled.
//
// Run: node tests/unit/task-38/resources-manual-qa.mjs   (uses the current dist/)

import { fileURLToPath } from 'node:url';
import { dirname, resolve } from 'node:path';
import { mkdirSync, writeFileSync } from 'node:fs';

import { ResourceReadRouter } from '../../../dist/resources/resource-read-router.js';
import { CapabilityResources, GatewayManifestCapabilitySource } from '../../../dist/resources/capability-resources.js';
import { EditorStateResources } from '../../../dist/resources/editor-state-resources.js';
import { KnowledgeResources } from '../../../dist/resources/knowledge-resources.js';
import { InMemoryRevisionProvider } from '../../../dist/server/mcp-primitives/resource-revision.js';

const here = dirname(fileURLToPath(import.meta.url));
const EVIDENCE = resolve(here, '../../../.omo/evidence/task-38/resources-manual-qa.json');

// --- Native `/mcp` oracle (transcribed from McpResourceReadContent.cpp) ---
const NATIVE_CATALOG_READ = {
  uri: 'ue://capability/catalog',
  mimeType: 'application/json',
  revision: 1,
  dataPresent: true, // native BuildReadBodyText returns {"revision":1,"data":{...}}
  dataKeys: ['capabilities', 'count', 'totalCount', 'truncated'],
};
const NATIVE_UNKNOWN_ERROR_CODE = 'RESOURCE_NOT_FOUND'; // Classify separates unknown from editor-state

// --- Real TS production engine (dist) with deterministic unavailable sources ---
function buildRouter() {
  const revisions = new InMemoryRevisionProvider();
  const editorSource = {
    isAvailable: async () => false,
    engineVersion: async () => null,
    pieActive: async () => false,
    currentLevel: async () => ({ name: 'None', path: 'None' }),
    selectedActors: async () => [],
  };
  const lookup = {
    isAvailable: async () => false,
    objectExists: async () => false,
    assetExists: async () => false,
  };
  return new ResourceReadRouter(
    new CapabilityResources(new GatewayManifestCapabilitySource(), revisions),
    new EditorStateResources(editorSource, revisions, 'ManualQaProject'),
    new KnowledgeResources(lookup, revisions),
  );
}

function normalizeRead(content) {
  let revision = typeof content.revision === 'number' ? content.revision : Number.NaN;
  let dataPresent = false;
  let dataKeys = [];
  try {
    const parsed = JSON.parse(String(content.text ?? ''));
    if (Number.isNaN(revision) && typeof parsed.revision === 'number') {
      revision = parsed.revision;
    }
    if (parsed.data !== null && typeof parsed.data === 'object') {
      dataPresent = true;
      dataKeys = Object.keys(parsed.data).sort();
    }
  } catch {
    // non-JSON text -> dataPresent stays false
  }
  return {
    uri: String(content.uri ?? ''),
    mimeType: String(content.mimeType ?? ''),
    revision,
    dataPresent,
    dataKeys,
  };
}

const deepEqual = (a, b) => JSON.stringify(a) === JSON.stringify(b);

async function main() {
  const router = buildRouter();

  // Case 1: one happy catalog read.
  const catalogResult = await router.read('ue://capability/catalog');
  const tsCatalog = normalizeRead(catalogResult.contents[0] ?? {});
  const catalogMatch = deepEqual(tsCatalog, NATIVE_CATALOG_READ);

  // Case 2: one unknown/stale error read.
  let tsUnknownCode = 'NONE';
  try {
    await router.read('ue://nope');
  } catch (error) {
    tsUnknownCode = error?.code ?? error?.name ?? 'UNKNOWN_ERROR';
  }
  const errorMatch = tsUnknownCode === NATIVE_UNKNOWN_ERROR_CODE;

  const verdict = catalogMatch && errorMatch ? 'PASS' : 'FAIL';
  const report = {
    task: 'task-38-lane-A-resources',
    kind: 'manual-qa',
    generatedAt: new Date().toISOString(),
    verdict,
    binaryPassCondition: 'PASS only when normalized catalog read AND unknown error code both match across transports',
    runtimeBlocker:
      'Native /mcp resource surface is C++ (McpNativeTransportPrimitives.cpp) and cannot be executed in-process; native side is a transcribed oracle. TS side is executed live from dist/.',
    cases: {
      happyCatalogRead: {
        uri: 'ue://capability/catalog',
        ts: tsCatalog,
        native: NATIVE_CATALOG_READ,
        match: catalogMatch,
        gap: catalogMatch ? null : 'catalog read data-shape drifted between TS and native (dataPresent/dataKeys diverge).',
      },
      unknownUriError: {
        uri: 'ue://nope',
        ts: { code: tsUnknownCode },
        native: { code: NATIVE_UNKNOWN_ERROR_CODE },
        match: errorMatch,
        gap: errorMatch ? null : 'unknown-uri error code drifted between TS and native (expected RESOURCE_NOT_FOUND on both).',
      },
    },
  };

  mkdirSync(dirname(EVIDENCE), { recursive: true });
  writeFileSync(EVIDENCE, `${JSON.stringify(report, null, 2)}\n`, 'utf8');
  process.stdout.write(`manual-qa verdict=${verdict} catalogMatch=${catalogMatch} errorMatch=${errorMatch}\n`);
  process.stdout.write(`evidence written: ${EVIDENCE}\n`);
  // The driver itself always exits 0; the binary verdict lives in the JSON. A
  // non-zero exit would conflate "driver crashed" with "parity mismatch".
}

main().catch((error) => {
  process.stderr.write(`manual-qa driver failed: ${error?.stack ?? error}\n`);
  process.exit(1);
});
