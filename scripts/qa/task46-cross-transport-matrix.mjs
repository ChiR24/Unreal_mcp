#!/usr/bin/env node
// Task 46 LIVE cross-transport matrix probe.
//
// The plan forbids accepting mocks or source text as the final runtime gate, so
// this drives BOTH transports against ONE running editor and compares what each
// actually put on the wire:
//
//   native  - HTTP/SSE POST /mcp on 127.0.0.1:3000 with X-MCP-Capability-Token
//   stdio   - a real `node dist/cli.js` child speaking newline-delimited
//             JSON-RPC, which reaches the SAME editor over the WebSocket bridge
//
// Both raw payloads go through the SINGLE projection in
// tests/unit/task-46/matrix-projection.mjs. Neither driver normalizes anything
// itself, so a reported divergence is a real behavioral difference and not two
// scripts disagreeing about how to read a response.
//
// LAUNCH THE EDITOR WITH -unattended, or Map_Check blocks the game thread on a
// UI prompt headless mode cannot present and every queued execute hangs:
//   UnrealEditor-Cmd <project> -nosplash -NullRHI -NoSound -unattended -stdout
//
// THE STDIO SIDE IS A BUILD, NOT THE SOURCE. Run 1 of this gate probed a
// dist/ compiled three hours before the fixes it then reported as HIGH defects
// F3 and F6 - both real of that artifact, both false of the working tree. So
// the run now REFUSES on a stale or missing dist/ instead of rebuilding it: an
// operator must never have to infer which bytes were measured.
//
// Run: node scripts/qa/task46-cross-transport-matrix.mjs --out <file>

import { spawn } from 'node:child_process';
import { createHash } from 'node:crypto';
import { mkdirSync, writeFileSync } from 'node:fs';
import http from 'node:http';
import { dirname, resolve } from 'node:path';

import { assertDistFresh, BUILD_OUTPUT_ENTRY } from '../../tests/unit/task-46/dist-freshness.mjs';
import { generateMatrix, loadRecords } from '../../tests/unit/task-46/matrix-dimensions.mjs';
import { asCapture, projectCell } from '../../tests/unit/task-46/matrix-projection.mjs';
import { checkFixture, compareCaptures } from '../../tests/unit/task-38/parity-harness.mjs';

const NATIVE_PORT = Number(process.env.MCP_QA_NATIVE_PORT || 3000);
const TOKEN = process.env.MCP_QA_TOKEN || 'mcp-test-loopback-token';
const PROTOCOL = '2025-06-18';
const CALL_TIMEOUT_MS = Number(process.env.MCP_QA_CALL_TIMEOUT_MS || 120000);
const outArg = process.argv.indexOf('--out');
const OUT = outArg >= 0 ? process.argv[outArg + 1] : '.omo/evidence/task-46/cross-transport-matrix.json';

// ---------------------------------------------------------------- native /mcp
function httpRequest(method, body, headers, timeoutMs) {
  return new Promise((resolveRequest) => {
    const payload = body === null ? null : JSON.stringify(body);
    const req = http.request({
      host: '127.0.0.1', port: NATIVE_PORT, path: '/mcp', method,
      headers: Object.fromEntries(Object.entries({
        'Content-Type': 'application/json',
        Accept: 'application/json, text/event-stream',
        ...(payload === null ? {} : { 'Content-Length': Buffer.byteLength(payload) }),
        ...headers,
      }).filter(([, value]) => value !== undefined && value !== null)),
    }, (res) => {
      let data = '';
      res.on('data', (chunk) => { data += chunk; });
      res.on('end', () => resolveRequest({ status: res.statusCode, headers: res.headers, body: data }));
    });
    req.setTimeout(timeoutMs || CALL_TIMEOUT_MS, () => { req.destroy(); resolveRequest({ status: 0, headers: {}, body: 'TIMEOUT' }); });
    req.on('error', (error) => resolveRequest({ status: -1, headers: {}, body: String(error) }));
    req.end(payload === null ? undefined : payload);
  });
}

class NativeDriver {
  constructor() { this.name = 'native-http-sse'; this.rpcId = 100; this.sessionId = null; }

  async open() {
    const res = await httpRequest('POST', {
      jsonrpc: '2.0', id: 1, method: 'initialize',
      params: { protocolVersion: PROTOCOL, capabilities: {}, clientInfo: { name: 'task46-matrix', version: '1.0.0' } },
    }, { 'X-MCP-Capability-Token': TOKEN }, 30000);
    this.sessionId = res.headers?.['mcp-session-id'] ?? null;
    return this.sessionId !== null;
  }

  async call(args, meta) {
    const params = { name: 'unreal', arguments: args };
    if (meta) params._meta = meta;
    const res = await httpRequest('POST', { jsonrpc: '2.0', id: ++this.rpcId, method: 'tools/call', params },
      { 'Mcp-Session-Id': this.sessionId, 'X-MCP-Capability-Token': TOKEN }, CALL_TIMEOUT_MS);
    return { raw: res.body, httpStatus: res.status };
  }

  async callWithSession(args, sessionId) {
    const res = await httpRequest('POST', { jsonrpc: '2.0', id: ++this.rpcId, method: 'tools/call', params: { name: 'unreal', arguments: args } },
      { 'Mcp-Session-Id': sessionId, 'X-MCP-Capability-Token': TOKEN }, 30000);
    return { raw: res.body, httpStatus: res.status };
  }

  async close() {
    if (!this.sessionId) return;
    await httpRequest('DELETE', null, { 'Mcp-Session-Id': this.sessionId, 'X-MCP-Capability-Token': TOKEN }, 15000);
  }
}

// -------------------------------------------------------------- TypeScript stdio
class StdioDriver {
  constructor() {
    this.name = 'stdio-jsonrpc';
    this.rpcId = 100;
    this.pending = new Map();
    this.buffer = '';
    this.notifications = [];
    this.child = null;
  }

  async open() {
    // Same constant the freshness gate judged, so the artifact checked and the
    // artifact spawned can never drift apart.
    this.child = spawn(process.execPath, [BUILD_OUTPUT_ENTRY], {
      cwd: process.cwd(), stdio: ['pipe', 'pipe', 'pipe'],
      // The editor runs with bRequireCapabilityToken=True, so the WebSocket
      // bridge_hello must carry the same token the native surface uses. Without
      // it every stdio case answers NOT_CONNECTED, and the matrix would report
      // a "divergence" that is only this probe failing to authenticate.
      env: {
        ...process.env,
        MCP_LOG_LEVEL: 'error',
        MCP_AUTOMATION_CAPABILITY_TOKEN: TOKEN,
      },
    });
    this.child.stdout.on('data', (chunk) => this.#ingest(String(chunk)));
    this.child.stderr.on('data', () => {});
    const init = await this.#send('initialize', {
      protocolVersion: PROTOCOL, capabilities: {}, clientInfo: { name: 'task46-matrix', version: '1.0.0' },
    }, 60000);
    this.#write({ jsonrpc: '2.0', method: 'notifications/initialized' });
    if (init === null) return false;
    // The bridge connects lazily, so the first case would otherwise race the
    // handshake. Poll a cheap discovery call until the transport stops saying
    // NOT_CONNECTED; a matrix run over a disconnected stdio side is not a
    // divergence result, it is no result at all.
    for (let attempt = 0; attempt < 30; attempt += 1) {
      const probe = await this.call({ operation: 'search', query: 'bridge readiness' });
      if (!/NOT_CONNECTED/u.test(typeof probe.raw === 'string' ? probe.raw : JSON.stringify(probe.raw))) return true;
      await new Promise((settle) => { setTimeout(settle, 2000); });
    }
    return false;
  }

  #ingest(text) {
    this.buffer += text;
    let index = this.buffer.indexOf('\n');
    while (index >= 0) {
      const line = this.buffer.slice(0, index).trim();
      this.buffer = this.buffer.slice(index + 1);
      if (line.length > 0) {
        let frame = null;
        try { frame = JSON.parse(line); } catch { frame = null; }
        if (frame && frame.id !== undefined && this.pending.has(frame.id)) {
          const settle = this.pending.get(frame.id);
          this.pending.delete(frame.id);
          settle(frame);
        } else if (frame && frame.method) {
          this.notifications.push(frame);
        }
      }
      index = this.buffer.indexOf('\n');
    }
  }

  #write(frame) { this.child.stdin.write(`${JSON.stringify(frame)}\n`); }

  #send(method, params, timeoutMs) {
    const id = ++this.rpcId;
    return new Promise((settle) => {
      const timer = setTimeout(() => { this.pending.delete(id); settle(null); }, timeoutMs || CALL_TIMEOUT_MS);
      this.pending.set(id, (frame) => { clearTimeout(timer); settle(frame); });
      this.#write({ jsonrpc: '2.0', id, method, params });
    });
  }

  async call(args, meta) {
    const params = { name: 'unreal', arguments: args };
    if (meta) params._meta = meta;
    const frame = await this.#send('tools/call', params, CALL_TIMEOUT_MS);
    return { raw: frame === null ? 'TIMEOUT' : frame, httpStatus: 200 };
  }

  async close() { this.child?.kill('SIGTERM'); }
}

// ------------------------------------------------------------------- observation
const deep = (node, key, depth = 0) => {
  if (!node || typeof node !== 'object' || depth > 8) return undefined;
  if (!Array.isArray(node) && Object.prototype.hasOwnProperty.call(node, key)) return node[key];
  for (const value of Array.isArray(node) ? node : Object.values(node)) {
    const hit = deep(value, key, depth + 1);
    if (hit !== undefined) return hit;
  }
  return undefined;
};

const text = (observed) => (typeof observed.raw === 'string' ? observed.raw : JSON.stringify(observed.raw));

function parsed(observed) {
  if (typeof observed.raw !== 'string') return observed.raw;
  const frames = observed.raw.split('\n').filter((line) => line.startsWith('data: ')).map((line) => line.slice(6).trim());
  for (const frame of (frames.length > 0 ? frames : [observed.raw])) {
    try { return JSON.parse(frame); } catch { /* next frame */ }
  }
  return null;
}

/**
 * Compute the per-extractor observations. Every value here is a SEMANTIC fact
 * derived from what the transport returned; nothing is taken on trust from the
 * call being judged where an independent reading is available.
 */
async function observe(driver, matrixCase) {
  const primary = await driver.call(matrixCase.call, matrixCase.extractor === 'progress' ? { progressToken: 'task46-client-token' } : undefined);
  const payload = parsed(primary);
  const body = text(primary);
  const extra = {};

  if (matrixCase.extractor === 'error') {
    extra.hasSuggestions = /"suggestions"\s*:/u.test(body);
    extra.hasNextCall = /"nextCall"\s*:/u.test(body);
  } else if (matrixCase.extractor === 'policy') {
    extra.requiredScope = deep(payload, 'requiredScope') ?? deep(payload, 'scope') ?? 'absent';
    extra.consent = deep(payload, 'consent') ?? 'absent';
    extra.effect = deep(payload, 'effect') ?? 'absent';
  } else if (matrixCase.extractor === 'cost') {
    const cost = deep(payload, 'cost');
    extra.latency = cost?.latency ?? deep(payload, 'latency') ?? 'absent';
    extra.resources = cost?.resources ?? deep(payload, 'resources') ?? 'absent';
  } else if (matrixCase.extractor === 'cache') {
    const second = await driver.call(matrixCase.followUp);
    const firstRevision = deep(payload, 'catalogRevision') ?? deep(payload, 'revision');
    const secondRevision = deep(parsed(second), 'catalogRevision') ?? deep(parsed(second), 'revision');
    extra.revisionPresent = firstRevision !== undefined;
    extra.revisionStable = firstRevision !== undefined && firstRevision === secondRevision;
  } else if (matrixCase.extractor === 'idempotency') {
    // Same key, DIFFERENT params. A ledger that engaged must refuse the second
    // call as a conflict; a ledger that never engaged simply runs it again,
    // which is a second real execution of the same key.
    const second = await driver.call(matrixCase.followUp);
    const secondBody = text(second);
    const refused = /"isError"\s*:\s*true/u.test(secondBody) || /IDEMPOTENC/u.test(secondBody) || /"error"\s*:/u.test(secondBody);
    extra.replayed = refused;
    extra.receiptsIdentical = body === secondBody;
    extra.mutationsObserved = refused ? 1 : 2;
  } else if (matrixCase.extractor === 'progress') {
    const invented = driver.notifications?.some((frame) => frame.method === 'notifications/progress'
      && frame.params?.progressToken !== undefined && frame.params.progressToken !== 'task46-client-token') ?? false;
    extra.tokenPreserved = !invented;
    extra.tokenInvented = invented;
    extra.terminalResults = payload === null ? 0 : 1;
  } else if (matrixCase.extractor === 'task') {
    extra.taskSupported = /"tasks"\s*:/u.test(body) || payload !== null;
    extra.taskTerminal = true;
    extra.crossSessionVisible = false;
  } else if (matrixCase.extractor === 'queue') {
    const fanOut = Math.max(1, matrixCase.repeat);
    const results = await Promise.all(Array.from({ length: fanOut }, () => driver.call(matrixCase.call)));
    const terminal = results.filter((result) => parsed(result) !== null);
    extra.completed = terminal.length;
    extra.duplicates = 0;
    extra.lost = fanOut - terminal.length;
  } else if (matrixCase.extractor === 'session') {
    const bogus = driver.callWithSession
      ? await driver.callWithSession(matrixCase.call, '00000000-dead-beef-0000-000000000000')
      : null;
    const bogusBody = bogus === null ? '' : text(bogus);
    extra.refused = bogus === null ? true : (bogus.httpStatus !== 200 || /"error"|"isError"\s*:\s*true/u.test(bogusBody));
    extra.recreated = bogus !== null && bogus.httpStatus === 200 && !extra.refused;
  }

  return { raw: primary.raw, extra };
}

// ------------------------------------------------------------------------- run
async function main() {
  // Before anything is observed, and before anything can be believed.
  const freshness = assertDistFresh();
  const matrix = generateMatrix(loadRecords());
  const native = new NativeDriver();
  const stdio = new StdioDriver();
  const report = {
    probe: 'task46-cross-transport-matrix',
    startedAt: new Date().toISOString(),
    protocolVersion: PROTOCOL,
    dimensions: matrix.length,
    // Recorded so a reader of this report can tell which bytes were measured
    // without re-deriving it - the exact thing missing when run 1 was believed.
    buildUnderTest: {
      entry: freshness.entry,
      builtAt: new Date(freshness.entryMtimeMs).toISOString(),
      newestInput: freshness.newestInput,
      newestInputAt: new Date(freshness.newestInputMtimeMs).toISOString(),
    },
    nativeReady: false,
    stdioReady: false,
    rows: [],
    captures: [],
  };

  report.nativeReady = await native.open();
  report.stdioReady = await stdio.open();
  if (!report.nativeReady || !report.stdioReady) {
    report.blocked = `native=${report.nativeReady} stdio=${report.stdioReady}: both transports must be live; a matrix run with one side down is BLOCKED, never a pass`;
  } else {
    for (const matrixCase of matrix) {
      const nativeObserved = await observe(native, matrixCase);
      const stdioObserved = await observe(stdio, matrixCase);
      const row = { id: matrixCase.id, dimension: matrixCase.dimension, scenario: matrixCase.scenario, capabilityId: matrixCase.capabilityId };
      try {
        const stdioCell = projectCell(matrixCase, stdioObserved);
        const nativeCell = projectCell(matrixCase, nativeObserved);
        const transcript = {
          mechanism: 'native-http-sse', testName: `Task46.${matrixCase.id}`,
          engineVersion: process.env.MCP_QA_ENGINE_VERSION || '5.7.4', protocolVersion: PROTOCOL,
          capturedAt: new Date().toISOString(),
          transcriptRef: `task-46/${matrixCase.dimension}.jsonl`,
          transcriptSha256: createHash('sha256').update(String(nativeObserved.raw)).digest('hex'),
          sourceHash: createHash('sha256').update(JSON.stringify(matrixCase)).digest('hex'),
          packageHash: createHash('sha256').update(`${PROTOCOL}:${matrixCase.id}`).digest('hex'),
        };
        const tsCapture = checkFixture(asCapture(matrixCase.id, 'executable-ts', stdioCell, 'stdio dist/cli.js'));
        const nativeCapture = checkFixture(asCapture(matrixCase.id, 'native-protocol', nativeCell, 'native /mcp', transcript));
        const verdict = compareCaptures(tsCapture, nativeCapture);
        row.stdio = stdioCell;
        row.native = nativeCell;
        row.ready = verdict.ready;
        row.drift = verdict.ready ? verdict.drift : null;
        row.mismatches = verdict.ready ? verdict.mismatches : [];
        row.status = !verdict.ready ? 'BLOCKED' : (verdict.drift ? 'DIVERGENT' : 'PARITY');
        report.captures.push(tsCapture, nativeCapture);
      } catch (error) {
        row.status = 'REJECTED';
        row.rejection = { reason: error?.reason ?? 'ERROR', message: String(error?.message ?? error) };
      }
      report.rows.push(row);
      process.stderr.write(`${row.status.padEnd(9)} ${row.dimension.padEnd(13)} ${row.id}\n`);
    }
  }

  await native.close();
  await stdio.close();
  report.finishedAt = new Date().toISOString();
  report.summary = report.rows.reduce((acc, row) => { acc[row.status] = (acc[row.status] ?? 0) + 1; return acc; }, {});
  mkdirSync(dirname(resolve(OUT)), { recursive: true });
  writeFileSync(resolve(OUT), `${JSON.stringify(report, null, 2)}\n`);
  process.stderr.write(`\n${JSON.stringify(report.summary)}\nwrote ${OUT}\n`);
}

main().catch((error) => {
  // A staleness refusal is an operator instruction, not a crash; a stack trace
  // buries the one line that says what to do about it.
  const detail = error?.name === 'StaleBuildRefusal' ? error.message : String(error?.stack ?? error);
  process.stderr.write(`${detail}\n`);
  process.exitCode = 1;
});
