#!/usr/bin/env node
// Task 59 — the TypeScript gateway's own answer to the same census question, so
// the live native reading has something to be compared against.
//
// WHY THIS IS A SEPARATE, EXPLICITLY LABELLED ARTIFACT. The live probe found that
// the native `/mcp` surface names far fewer actions for some parent tools than
// the catalogue declares. That reading is only interesting if we know whether the
// catalogue's own server agrees with the catalogue — otherwise "native named 4 of
// 77" could equally mean the records are wrong, and blaming a transport for a
// contract defect is the wrong bug either way.
//
// WHAT THIS RUN IS AND IS NOT. `describe` is a CONTRACT query: the TypeScript
// gateway answers it from the generated registry, and `gateway-availability.ts`
// documents that it deliberately does not probe an editor at discovery time. So
// this census needs no Unreal, and it is run with the bridge in mock mode purely
// so the server will start. NOTHING here is a live reading, nothing here claims
// an editor was exercised, and the document says so in a field rather than in a
// comment nobody downstream will read. The live half is the native probe's.
//
// Run: node scripts/qa/task59-transport-census.mjs [--out FILE]

import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname } from 'node:path';

import { StdioDriver } from '../../tests/unit/task-49/live-driver-stdio.mjs';
import { censusTool } from '../../tests/unit/task-59/capability-verdicts.mjs';

const argOf = (name, fallback) => {
  const index = process.argv.indexOf(name);
  return index >= 0 ? process.argv[index + 1] : fallback;
};

const REPO = process.cwd();
const OUT = argOf('--out', '.omo/evidence/task-59/ts-contract-census.json');
const log = (line) => { process.stderr.write(`${line}\n`); };

/** Records address some actions with a family prefix; the surface names the bare action. */
const bare = (name) => name.slice(name.lastIndexOf('.') + 1);

async function main() {
  const registry = JSON.parse(readFileSync(
    `${REPO}/src/tools/catalog/capabilities/generated/canonical-registry.generated.json`, 'utf8',
  ));
  /** @type {Map<string, Set<string>>} */
  const declaredByTool = new Map();
  for (const record of registry.records) {
    const tool = record.parent?.parent ?? null;
    if (tool === null) continue;
    if (!declaredByTool.has(tool)) declaredByTool.set(tool, new Set());
    /** @type {Set<string>} */ (declaredByTool.get(tool)).add(String(record.id).split('.').slice(1).join('.'));
  }

  const driver = /** @type {any} */ (new StdioDriver({
    cwd: REPO,
    clientName: 'task59-transport-census',
    env: { ...process.env, MOCK_UNREAL_CONNECTION: 'true', MCP_LOG_LEVEL: 'error' },
  }));
  const started = await driver.start();
  if (started.ok !== true) {
    writeOut({
      censused: false,
      reason: 'STDIO_SERVER_DID_NOT_START',
      detail: String(started.reason),
    });
    process.exitCode = 1;
    return;
  }

  /** @type {Array<Record<string, unknown>>} */
  const perTool = [];
  for (const tool of [...declaredByTool.keys()].sort()) {
    const census = await censusTool(driver, tool);
    const declared = [...(/** @type {Set<string>} */ (declaredByTool.get(tool)))].sort();
    const runtimeSet = new Set([...census.names, ...census.names.map(bare)]);
    const declaredSet = new Set([...declared, ...declared.map(bare)]);
    const missing = declared.filter((action) => !runtimeSet.has(action) && !runtimeSet.has(bare(action)));
    const extra = census.names.filter((action) => !declaredSet.has(action) && !declaredSet.has(bare(action)));
    perTool.push({
      tool,
      declaredCount: declared.length,
      runtimeCount: census.names.length,
      reportedCount: census.reportedCount ?? null,
      censusMethod: census.method,
      pages: census.pages.length,
      missingAtRuntime: missing,
      extraAtRuntime: extra,
      names: census.names,
    });
    log(`${tool}: declared ${declared.length}, ts-runtime ${census.names.length}`
      + `${missing.length > 0 ? `, MISSING ${missing.length}` : ''}${extra.length > 0 ? `, EXTRA ${extra.length}` : ''}`);
  }

  await driver.close();
  writeOut({
    censused: true,
    surface: 'typescript stdio gateway (node dist/cli.js)',
    isLiveEditorEvidence: false,
    whatThisIsNot: 'NOT a live reading. `describe` is answered from the generated registry, so this census '
      + 'needs no Unreal and was taken with MOCK_UNREAL_CONNECTION=true purely so the server would start. '
      + 'It is the CONTRACT side of the comparison; nothing here claims an editor was exercised.',
    generatedAt: new Date().toISOString(),
    totals: {
      declared: registry.records.length,
      runtimeNamed: perTool.reduce((sum, row) => sum + Number(row.runtimeCount), 0),
      toolsWithMissing: perTool.filter((row) => /** @type {any[]} */ (row.missingAtRuntime).length > 0).length,
      toolsWithExtra: perTool.filter((row) => /** @type {any[]} */ (row.extraAtRuntime).length > 0).length,
    },
    perTool,
  });
}

function writeOut(document) {
  mkdirSync(dirname(`${REPO}/${OUT}`), { recursive: true });
  writeFileSync(`${REPO}/${OUT}`, `${JSON.stringify(document, null, 2)}\n`);
  log(`wrote ${OUT}`);
}

main().catch((error) => {
  log(String(error?.stack ?? error));
  process.exitCode = 1;
});
