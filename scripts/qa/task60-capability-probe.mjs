#!/usr/bin/env node
// Task 60 — what does a LIVE UE 5.8.0 editor actually expose, and does it match
// what the records promise?
//
// The offline profile matrix (Task 52) can compute what SHOULD be reachable on a
// 5.8 profile from the contract alone. It cannot answer the question this task
// exists to ask, because the two ways the answer can be wrong are invisible to it:
//
//   a capability the records declare and the live engine WITHHOLDS is a support
//   claim the product cannot honour on the newest engine it advertises;
//
//   a capability the live engine OFFERS that the records gate away is worse. It
//   is the "claim without implementation" shape in reverse — the surface answers
//   for something the contract says is unavailable here, so a client that trusts
//   the contract is wrong about what it just called.
//
// So this probe reads the capability set out of the RUNNING gateway and diffs it
// against the registry. It is deliberately READ-ONLY: `describe` walks the
// progressive-disclosure tree the gateway already implements (tool -> action) and
// nothing here executes, mutates or configures. A capability census that mutated
// the editor would change the thing it was measuring, and it would have to run
// before the certification's own corpus rather than beside it.
//
// It NEVER launches an editor. It is pointed at a port another run owns and it
// opens exactly one session, which it deletes. If the port does not answer it
// says so and stops — inventing a census from the registry when the engine did
// not reply would produce a perfect match every time, which is precisely the
// reading that would prove nothing.
//
// Run: node scripts/qa/task60-capability-probe.mjs --native-port N [--out FILE]

import { readFileSync, mkdirSync, writeFileSync } from 'node:fs';
import { dirname } from 'node:path';

import { NativeDriver } from '../../tests/unit/task-49/live-driver-native.mjs';

const argOf = (name, fallback) => {
  const index = process.argv.indexOf(name);
  return index >= 0 ? process.argv[index + 1] : fallback;
};

const REPO = process.cwd();
const PORT = Number(argOf('--native-port', '0'));
const OUT = argOf('--out', '.omo/evidence/task-60/capability-probe.json');
const log = (line) => { process.stderr.write(`${line}\n`); };

/** Pull the text payload out of a gateway tool result, whatever shape it took. */
function payloadOf(response) {
  const result = response?.result;
  if (result === undefined || result === null) return null;
  const content = Array.isArray(result.content) ? result.content : [];
  const text = content.filter((entry) => entry?.type === 'text').map((entry) => String(entry.text)).join('\n');
  if (text.length === 0) return result.structuredContent ?? null;
  try {
    return JSON.parse(text);
  } catch {
    return { raw: text.slice(0, 4000) };
  }
}

/**
 * Every action name anywhere in a describe payload, without assuming the exact
 * envelope. The gateway's describe shape is progressive and has changed across
 * waves; a probe hard-coded to one nesting would report an empty census as a
 * clean one, which is the failure this whole file is built to avoid.
 */
function harvestActions(node, found = new Set()) {
  if (node === null || typeof node !== 'object') return found;
  if (Array.isArray(node)) {
    for (const entry of node) harvestActions(entry, found);
    return found;
  }
  for (const [key, value] of Object.entries(node)) {
    if (key === 'action' && typeof value === 'string') found.add(value);
    if (key === 'actions' && Array.isArray(value)) {
      for (const entry of value) {
        if (typeof entry === 'string') found.add(entry);
        else if (entry !== null && typeof entry === 'object' && typeof entry.name === 'string') found.add(entry.name);
        else if (entry !== null && typeof entry === 'object' && typeof entry.action === 'string') found.add(entry.action);
      }
    }
    harvestActions(value, found);
  }
  return found;
}

async function main() {
  if (!Number.isInteger(PORT) || PORT <= 0) {
    log('REFUSING: --native-port must name the port of an editor this run may read.');
    process.exitCode = 2;
    return;
  }

  const registry = JSON.parse(readFileSync(
    `${REPO}/src/tools/catalog/capabilities/generated/canonical-registry.generated.json`, 'utf8',
  ));
  /** @type {any[]} */
  const records = registry.records;

  // The contract's own view, grouped the way the gateway addresses them.
  /** @type {Map<string, Set<string>>} */
  const declaredByTool = new Map();
  for (const record of records) {
    const tool = record.parent?.parent ?? null;
    const action = String(record.id).split('.').slice(1).join('.');
    if (tool === null) continue;
    if (!declaredByTool.has(tool)) declaredByTool.set(tool, new Set());
    /** @type {Set<string>} */ (declaredByTool.get(tool)).add(action);
  }

  const driver = /** @type {any} */ (new NativeDriver({ port: PORT, clientName: 'task60-capability-probe' }));
  const opened = await driver.initialize();
  if (opened.ok !== true) {
    log(`the native /mcp surface on 127.0.0.1:${PORT} did not initialize (status ${String(opened.status)}).`);
    writeOut({
      probed: false,
      reason: 'NATIVE_MCP_DID_NOT_INITIALIZE',
      detail: `127.0.0.1:${PORT} answered status ${String(opened.status)}; no census was taken and none is inferred`,
      port: PORT,
    });
    process.exitCode = 1;
    return;
  }
  log(`initialized on 127.0.0.1:${PORT}, protocol ${String(opened.negotiatedVersion)}`);

  /** @type {Record<string, unknown>} */
  const census = {
    probed: true,
    port: PORT,
    negotiatedVersion: opened.negotiatedVersion ?? null,
    startedAt: new Date().toISOString(),
  };

  // 1. What tools does the surface publicly list? Must be exactly one: `unreal`.
  const listed = await driver.request('tools/list', {});
  const tools = (listed.response?.result?.tools ?? []).map((entry) => entry?.name);
  census.publicTools = tools;
  census.exactlyOneGatewayTool = tools.length === 1 && tools[0] === 'unreal';
  log(`tools/list -> ${JSON.stringify(tools)}`);

  // 2. The domain index, straight from the running gateway.
  const domainsCall = await driver.callTool({ operation: 'describe' }, { timeoutMs: 120_000 });
  census.describeRoot = payloadOf(domainsCall.response);

  // 3. Per canonical tool: the actions this ENGINE is willing to name.
  /** @type {Array<Record<string, unknown>>} */
  const perTool = [];
  for (const tool of [...declaredByTool.keys()].sort()) {
    const call = await driver.callTool({ operation: 'describe', tool }, { timeoutMs: 120_000 });
    const payload = payloadOf(call.response);
    const runtime = [...harvestActions(payload)].sort();
    const declared = [...(/** @type {Set<string>} */ (declaredByTool.get(tool)))].sort();
    const missingAtRuntime = declared.filter((action) => !runtime.includes(action));
    const extraAtRuntime = runtime.filter((action) => !declared.includes(action));
    perTool.push({
      tool,
      declaredCount: declared.length,
      runtimeCount: runtime.length,
      isError: call.response?.result?.isError === true,
      missingAtRuntime,
      extraAtRuntime,
      runtimeActions: runtime,
    });
    log(`describe ${tool}: declared ${declared.length}, runtime ${runtime.length}`
      + `${missingAtRuntime.length > 0 ? `, MISSING ${missingAtRuntime.length}` : ''}`
      + `${extraAtRuntime.length > 0 ? `, EXTRA ${extraAtRuntime.length}` : ''}`);
  }
  census.perTool = perTool;
  census.totals = {
    declared: records.length,
    runtimeNamed: perTool.reduce((sum, row) => sum + Number(row.runtimeCount), 0),
    toolsWithMissing: perTool.filter((row) => /** @type {any[]} */ (row.missingAtRuntime).length > 0).length,
    toolsWithExtra: perTool.filter((row) => /** @type {any[]} */ (row.extraAtRuntime).length > 0).length,
  };

  // 4. The editor-state gap, asked of the engine rather than assumed.
  //    12 records declare pie/simulate only. This editor is in `edit`, so a
  //    contract-honest surface must refuse them HERE — and refusing them for the
  //    right reason is the part a count cannot show.
  const pieOnly = records.filter((record) => {
    const states = record.availability?.editorStates ?? [];
    return states.length > 0 && !states.includes('edit');
  });
  /** @type {Array<Record<string, unknown>>} */
  const editorStateProbes = [];
  for (const record of pieOnly.slice(0, 3)) {
    const call = await driver.callTool({
      operation: 'describe',
      tool: record.parent?.parent,
      action: String(record.id).split('.').slice(1).join('.'),
    }, { timeoutMs: 120_000 });
    const payload = payloadOf(call.response);
    editorStateProbes.push({
      id: record.id,
      declaredStates: record.availability?.editorStates ?? [],
      describeAnswered: call.response?.result !== undefined,
      isError: call.response?.result?.isError === true,
      payloadSample: JSON.stringify(payload).slice(0, 600),
    });
  }
  census.editorStateProbes = editorStateProbes;
  census.pieOnlyDeclaredCount = pieOnly.length;

  const closed = await driver.close();
  const released = await driver.verifySessionReleased();
  census.session = { closed: closed.deleted === true, released: released.released === true, observed: released.observed };
  census.finishedAt = new Date().toISOString();
  writeOut(census);
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
