#!/usr/bin/env node
// Task 41 live idempotency probe (native /mcp).
//
// Proves the execute-path dedup end to end against a running editor, which the
// ledger unit/automation tests cannot: a create replayed under the same key
// returns the SAME receipt (verbatim, so exactly one mutation happened), and the
// same key reused with different params is refused IDEMPOTENCY_CONFLICT.
//
// Setup mirrors scripts/qa/task40-security-matrix.README: live UE editor, native
// MCP on 3000, bRequireCapabilityToken=True, CapabilityToken=mcp-test-loopback-token.
// Run: node scripts/qa/task41-idempotency-probe.cjs --out <file>

const http = require('node:http');

const NATIVE_PORT = Number(process.env.MCP_QA_NATIVE_PORT || 3000);
const TOKEN = process.env.MCP_QA_TOKEN || 'mcp-test-loopback-token';
const FOLDER = '/Game/QAIdemProbe';
const NAME_A = 'M_QAIdemProbeA';
const NAME_B = 'M_QAIdemProbeB';
const ASSET_A = `${FOLDER}/${NAME_A}`;
const CREATE_GRANT = { capability: 'material.create_material', acknowledge: 'explicit' };
const DELETE_GRANT = { capability: 'asset.delete_asset', acknowledge: 'elevated' };

const outArg = process.argv.indexOf('--out');
const OUT = outArg >= 0 ? process.argv[outArg + 1] : null;
const results = [];
const record = (name, expected, actual, pass, extra) =>
  results.push({ name, expected, actual, pass, ...(extra || {}) });

function post(body, headers, timeoutMs = 15000) {
  return new Promise((resolve) => {
    const payload = JSON.stringify(body);
    const req = http.request({
      host: '127.0.0.1', port: NATIVE_PORT, path: '/mcp', method: 'POST',
      headers: Object.fromEntries(Object.entries({
        'Content-Type': 'application/json',
        Accept: 'application/json, text/event-stream',
        'Content-Length': Buffer.byteLength(payload),
        ...headers
      }).filter(([, v]) => v !== undefined && v !== null))
    }, (res) => {
      let d = '';
      res.on('data', (c) => (d += c));
      res.on('end', () => { let j = null; try { j = JSON.parse(d); } catch { /* non-JSON */ } resolve({ status: res.statusCode, headers: res.headers, body: d, json: j }); });
    });
    req.setTimeout(timeoutMs, () => { req.destroy(); resolve({ status: 0, body: 'TIMEOUT', json: null }); });
    req.on('error', (e) => resolve({ status: -1, body: String(e), json: null }));
    req.end(payload);
  });
}

async function init() {
  const res = await post({
    jsonrpc: '2.0', id: 1, method: 'initialize',
    params: { protocolVersion: '2025-06-18', capabilities: {}, clientInfo: { name: 'task41-idempotency-probe', version: '1.0.0' } }
  }, { 'X-MCP-Capability-Token': TOKEN });
  return res.headers?.['mcp-session-id'] || res.json?.result?.sessionId;
}

function exec(sid, tool, action, params, consent, idempotencyKey) {
  const args = { operation: 'execute', tool, action, params: params || {} };
  if (consent) args.consent = consent;
  if (idempotencyKey) args.options = { idempotencyKey };
  return post({ jsonrpc: '2.0', id: 2, method: 'tools/call', params: { name: 'unreal', arguments: args } },
    { 'Mcp-Session-Id': sid, 'X-MCP-Capability-Token': TOKEN });
}

// A streamed create answers as SSE (multiple `data:` frames), a replay/refusal as
// plain JSON. Both carry the same field names, so a raw-body regex reads either
// shape robustly — the same approach the Task 40 native harness relies on. Every
// frame of one response shares the request's correlationId.
const raw = (res) => String(res.body || '');
const status = (res) => {
  if (res.status !== 200) return `HTTP_${res.status}`;
  const err = raw(res).match(/"(?:errorCode|gatewayCode)":"([A-Z_]+)"/);
  if (err) return err[1];
  return /"status":"success"|"success":true/.test(raw(res)) ? 'success' : 'UNKNOWN';
};
const errorCode = (res) => { const m = raw(res).match(/"(?:errorCode|gatewayCode)":"([A-Z_]+)"/); return m ? m[1] : null; };
const corr = (res) => { const m = raw(res).match(/"correlationId":"([0-9A-Fa-f-]+)"/); return m ? m[1] : null; };

async function main() {
  const sid = await init();
  if (!sid) { record('INIT', 'a session id', 'none — is the editor + native MCP up?', false); return; }

  await exec(sid, 'manage_asset', 'delete_asset', { paths: [ASSET_A] }, DELETE_GRANT); // pre-clean
  await exec(sid, 'manage_asset', 'delete_asset', { paths: [`${FOLDER}/${NAME_B}`] }, DELETE_GRANT);

  const r1 = await exec(sid, 'manage_asset', 'create_material', { name: NAME_A, path: FOLDER, save: true }, CREATE_GRANT, 'idem-k1');
  record('IP-1 first keyed create executes', 'success', status(r1), status(r1) === 'success', { correlationId: corr(r1) });

  const r2 = await exec(sid, 'manage_asset', 'create_material', { name: NAME_A, path: FOLDER, save: true }, CREATE_GRANT, 'idem-k1');
  // Decisive: a NON-deduped re-create of an existing asset would error; a success
  // whose correlationId equals the first proves the recorded receipt was replayed
  // verbatim, so the handler ran exactly once.
  const replayed = status(r2) === 'success' && corr(r2) !== null && corr(r2) === corr(r1);
  record('IP-2 identical replay returns the SAME receipt (exactly one mutation)',
    `success AND correlationId == ${corr(r1)}`, `${status(r2)} corr=${corr(r2)}`, replayed);

  const rc = await exec(sid, 'manage_asset', 'create_material', { name: NAME_B, path: FOLDER, save: true }, CREATE_GRANT, 'idem-k1');
  record('IP-3 same key + different params is refused', 'IDEMPOTENCY_CONFLICT', errorCode(rc) || status(rc),
    errorCode(rc) === 'IDEMPOTENCY_CONFLICT');

  const rk = await exec(sid, 'manage_asset', 'create_material', { name: NAME_B, path: FOLDER, save: true }, CREATE_GRANT, 'idem-k2');
  // A different key for those different params is a distinct slot, so it really
  // dispatches (success), proving dedup is keyed, not global.
  record('IP-4 a different key is a distinct slot and dispatches', 'success', status(rk), status(rk) === 'success');

  await exec(sid, 'manage_asset', 'delete_asset', { paths: [ASSET_A] }, DELETE_GRANT);
  await exec(sid, 'manage_asset', 'delete_asset', { paths: [`${FOLDER}/${NAME_B}`] }, DELETE_GRANT);
  await new Promise((resolve) => {
    const req = http.request({ host: '127.0.0.1', port: NATIVE_PORT, path: '/mcp', method: 'DELETE',
      headers: { 'Mcp-Session-Id': sid, 'X-MCP-Capability-Token': TOKEN } }, (res) => { res.resume(); res.on('end', resolve); });
    req.setTimeout(5000, () => { req.destroy(); resolve(); });
    req.on('error', () => resolve());
    req.end();
  });
}

main().then(() => {
  const passed = results.filter((r) => r.pass).length;
  for (const r of results) console.log(`${r.pass ? 'PASS' : 'FAIL'}  ${r.name}  [expected ${r.expected} | actual ${r.actual}]`);
  console.log(`\n${passed}/${results.length} passed`);
  if (OUT) require('node:fs').writeFileSync(OUT, JSON.stringify(results, null, 2));
  process.exit(passed === results.length && results.length > 0 ? 0 : 1);
}).catch((e) => { console.error(e); process.exit(2); });
