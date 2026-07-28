#!/usr/bin/env node
// Task 42 live stale-state precondition probe (native /mcp).
//
// Proves end to end against a running editor what the unit and automation tests
// cannot: a mutating execute carrying a STALE options.expectedRevisions pin is
// refused STALE_STATE and performs ZERO editor work, while the identical call
// without a pin really does mutate.
//
// The refusal is checked with an INDEPENDENT oracle - a separate asset.list read
// whose data.assets array is parsed, never the mutating call's own response - so a
// forged "refused" response cannot pass: the asset must genuinely not exist.
//
// Setup mirrors scripts/qa/task40-security-matrix.README: live UE editor, native
// MCP on 3000, bRequireCapabilityToken=True, CapabilityToken=mcp-test-loopback-token.
//
// LAUNCH THE EDITOR WITH -unattended. Without it the editor blocks inside
// LoadDefaultMapAtStartup (Map_Check on a World Partition map raises a handled
// ensure and waits for a UI that headless mode never presents), so the game
// thread never reaches its tick loop, ProcessPendingAutomationRequests never
// drains, and every queued execute hangs until the client gives up. Only the
// pre-queue refusals (SP-5/SP-6) can answer in that state.
//   UnrealEditor-Cmd <project> -nosplash -NullRHI -NoSound -unattended -stdout
//
// Run: node scripts/qa/task42-stale-state-probe.cjs --out <file>

const http = require('node:http');

const NATIVE_PORT = Number(process.env.MCP_QA_NATIVE_PORT || 3000);
const TOKEN = process.env.MCP_QA_TOKEN || 'mcp-test-loopback-token';
const FOLDER = '/Game/QAStaleProbe';
const NAME_STALE = 'M_QAStaleRefused';
const NAME_OK = 'M_QAStaleApplied';
const ASSET_STALE = `${FOLDER}/${NAME_STALE}`;
const ASSET_OK = `${FOLDER}/${NAME_OK}`;

// Every editor call is queued to the game thread, so the timeout has to tolerate
// a busy editor. Measured on UE 5.7.4: asset.list ~4s cold / ~60ms warm,
// create_material ~100ms. 15s was survivable but left no margin; a queue stall
// must surface as a real failure, not as a client-side timeout.
const EXEC_TIMEOUT_MS = 180000;
const SESSION_TIMEOUT_MS = 30000;
const READY_DEADLINE_MS = 120000;

const outArg = process.argv.indexOf('--out');
const OUT = outArg >= 0 ? process.argv[outArg + 1] : null;
const results = [];
const meta = {
  probe: 'task42-stale-state-probe',
  nativePort: NATIVE_PORT,
  execTimeoutMs: EXEC_TIMEOUT_MS,
  // Recorded because it is the reproduction precondition: without -unattended the
  // editor never reaches its tick loop and no queued execute can ever answer.
  editorLaunch: 'UnrealEditor-Cmd <project> -nosplash -NullRHI -NoSound -unattended -nocrashreports -stdout',
  startedAt: new Date().toISOString()
};
const record = (name, expected, actual, pass, extra) =>
  results.push({ name, expected, actual, pass, ...(extra || {}) });

function request(method, body, headers, timeoutMs) {
  const startedAt = Date.now();
  return new Promise((resolve) => {
    const payload = body === null ? null : JSON.stringify(body);
    const req = http.request({
      host: '127.0.0.1', port: NATIVE_PORT, path: '/mcp', method,
      headers: Object.fromEntries(Object.entries({
        'Content-Type': 'application/json',
        Accept: 'application/json, text/event-stream',
        ...(payload === null ? {} : { 'Content-Length': Buffer.byteLength(payload) }),
        ...headers
      }).filter(([, v]) => v !== undefined && v !== null))
    }, (res) => {
      let d = '';
      res.on('data', (c) => (d += c));
      res.on('end', () => { let j = null; try { j = JSON.parse(d); } catch { /* SSE, not JSON */ } resolve({ status: res.statusCode, headers: res.headers, body: d, json: j, ms: Date.now() - startedAt }); });
    });
    req.setTimeout(timeoutMs || EXEC_TIMEOUT_MS, () => { req.destroy(); resolve({ status: 0, body: 'TIMEOUT', json: null, ms: Date.now() - startedAt }); });
    req.on('error', (e) => resolve({ status: -1, body: String(e), json: null, ms: Date.now() - startedAt }));
    req.end(payload === null ? undefined : payload);
  });
}

const post = (body, headers, timeoutMs) => request('POST', body, headers, timeoutMs);

async function init() {
  const res = await post({
    jsonrpc: '2.0', id: 1, method: 'initialize',
    params: { protocolVersion: '2025-06-18', capabilities: {}, clientInfo: { name: 'task42-stale-state-probe', version: '1.0.0' } }
  }, { 'X-MCP-Capability-Token': TOKEN }, SESSION_TIMEOUT_MS);
  return res.headers?.['mcp-session-id'] || res.json?.result?.sessionId;
}

let rpcId = 1;
function gateway(sid, args, timeoutMs) {
  return post({ jsonrpc: '2.0', id: ++rpcId, method: 'tools/call', params: { name: 'unreal', arguments: args } },
    { 'Mcp-Session-Id': sid, 'X-MCP-Capability-Token': TOKEN }, timeoutMs);
}

function exec(sid, tool, action, params, consent, options) {
  const args = { operation: 'execute', tool, action, params: params || {} };
  if (consent) args.consent = consent;
  if (options) args.options = options;
  return gateway(sid, args, EXEC_TIMEOUT_MS);
}

// A streamed execute answers as SSE, a refusal as plain JSON; both wrap the tool
// payload in content[0].text as "<message>\n\n<receipt json>". Parse that JSON
// instead of regexing the raw body, so the oracle reads real fields (data.assets)
// rather than matching a name that could appear anywhere in the envelope.
const raw = (res) => String(res.body || '');
function payload(res) {
  const body = raw(res);
  const frames = body.split('\n')
    .filter((line) => line.startsWith('data: '))
    .map((line) => line.slice('data: '.length).trim());
  for (const frame of (frames.length ? frames : [body])) {
    let env = null;
    try { env = JSON.parse(frame); } catch { continue; }
    const text = env?.result?.content?.[0]?.text;
    if (typeof text !== 'string') continue;
    const brace = text.indexOf('{');
    if (brace < 0) continue;
    try { return JSON.parse(text.slice(brace)); } catch { /* not the receipt frame */ }
  }
  return null;
}
const status = (res) => {
  if (res.status !== 200) return `HTTP_${res.status}`;
  const err = raw(res).match(/"(?:errorCode|gatewayCode)":"([A-Z_]+)"|Error \[([A-Z_]+)\]/);
  if (err) return err[1] || err[2];
  return /"status":"success"|"success":true/.test(raw(res)) ? 'success' : 'UNKNOWN';
};
const errorCode = (res) => {
  const m = raw(res).match(/"(?:errorCode|gatewayCode)":"([A-Z_]+)"|Error \[([A-Z_]+)\]/);
  return m ? (m[1] || m[2]) : null;
};
const message = (res) => {
  const env = payload(res);
  if (env && typeof env.message === 'string') return env.message;
  const m = raw(res).match(/Error \[[A-Z_]+\]: ([^"\\]+)/);
  return m ? m[1] : null;
};

// INDEPENDENT ORACLE. One reading of a separate asset.list read: true/false when
// the listing came back and its assets array could be searched, null when the
// read is unusable - so an unusable oracle is reported inconclusive rather than
// silently passing.
async function readAsset(sid, folder, assetName) {
  const res = await exec(sid, 'manage_asset', 'list', { path: folder }, null, null);
  const env = payload(res);
  const assets = env?.data?.assets ?? env?.receipt?.data?.assets;
  if (status(res) === 'success' && Array.isArray(assets)) {
    return { verdict: assets.some((a) => JSON.stringify(a).includes(assetName)), via: 'folder-listing', assetCount: assets.length };
  }
  // The folder read failed (a refused create never creates the folder either).
  // The parent listing is then conclusive in the negative direction only: no
  // folder means no asset inside it.
  const parent = folder.slice(0, folder.lastIndexOf('/')) || '/Game';
  const up = await exec(sid, 'manage_asset', 'list', { path: parent }, null, null);
  const upEnv = payload(up);
  const folders = upEnv?.data?.folders ?? upEnv?.receipt?.data?.folders;
  if (status(up) === 'success' && Array.isArray(folders)) {
    const present = folders.some((f) => String(f).replace(/\/$/, '') === folder);
    if (!present) return { verdict: false, via: 'parent-listing (folder absent)', assetCount: 0 };
  }
  return { verdict: null, via: `unusable (folder read ${status(res)}, parent read ${status(up)})`, assetCount: null };
}

// Poll the oracle: presence is sticky (a created asset does not vanish), so stop
// at the first `true`; absence is only trusted after the asset registry has had
// every chance to surface a late creation. Both directions therefore get the same
// window, and the verdict is the last conclusive reading.
async function assetExists(sid, folder, assetName, attempts = 5) {
  const readings = [];
  let last = null;
  for (let i = 0; i < attempts; i += 1) {
    const reading = await readAsset(sid, folder, assetName);
    readings.push(reading);
    if (reading.verdict !== null) last = reading;
    if (reading.verdict === true) break;
    if (i + 1 < attempts) await new Promise((r) => setTimeout(r, 1500));
  }
  return { verdict: last ? last.verdict : null, readings };
}

// The consent envelope each mutating capability actually demands, read from the
// gateway's own describe rather than hardcoded, so a contract change surfaces
// here instead of masquerading as a policy refusal.
async function discoverGrant(sid, action, fallback) {
  const res = await gateway(sid, { operation: 'describe', tool: 'manage_asset', action }, SESSION_TIMEOUT_MS);
  const env = payload(res);
  const grant = env?.consentGrant;
  const ok = grant && typeof grant.capability === 'string' && typeof grant.acknowledge === 'string';
  return { grant: ok ? grant : fallback, discovered: Boolean(ok), capability: env?.capability ?? null };
}

async function main() {
  const sid = await init();
  if (!sid) { record('INIT', 'a session id', 'none - is the editor + native MCP up?', false); return; }

  // Readiness gate AND warm-up. The first queued call absorbs the asset-registry
  // scan; until one succeeds the editor cannot answer anything, and a timeout
  // then would say nothing about the precondition gate.
  const readyStart = Date.now();
  let warm = null;
  for (;;) {
    warm = await exec(sid, 'manage_asset', 'list', { path: '/Game' }, null, null);
    if (status(warm) === 'success') break;
    if (Date.now() - readyStart > READY_DEADLINE_MS) {
      record('READY editor answers a queued read', 'success', `${status(warm)} after ${Date.now() - readyStart}ms`, false,
        { hint: 'queued executes never complete - launch the editor with -unattended' });
      return;
    }
    await new Promise((r) => setTimeout(r, 2000));
  }
  const live = payload(warm)?.liveRevisions ?? null;
  meta.warmUpMs = warm.ms;
  meta.liveRevisions = live;

  const createGrant = await discoverGrant(sid, 'create_material', { capability: 'material.create_material', acknowledge: 'explicit' });
  const deleteGrant = await discoverGrant(sid, 'delete_asset', { capability: 'asset.delete_asset', acknowledge: 'elevated' });
  const CREATE_GRANT = createGrant.grant;
  const DELETE_GRANT = deleteGrant.grant;
  meta.discovered = {
    readAction: 'manage_asset.list (asset.list) - list_assets does not exist (UNKNOWN_ACTION)',
    createConsentGrant: CREATE_GRANT, createGrantFromDescribe: createGrant.discovered,
    deleteConsentGrant: DELETE_GRANT, deleteGrantFromDescribe: deleteGrant.discovered
  };

  // A pin the live editor provably cannot be at: read the current selection
  // revision and offset it, so the refusal is caused by staleness and not by a
  // value the gate might treat as special.
  const STALE_PIN = (live && Number.isInteger(live.selection) ? live.selection : 1) + 100000;
  meta.stalePin = STALE_PIN;

  await exec(sid, 'manage_asset', 'delete_asset', { paths: [ASSET_STALE, ASSET_OK] }, DELETE_GRANT, null); // pre-clean

  // SP-1: a stale pin must refuse.
  const stale = await exec(sid, 'manage_asset', 'create_material',
    { name: NAME_STALE, path: FOLDER, save: true }, CREATE_GRANT,
    { expectedRevisions: { selection: STALE_PIN } });
  record('SP-1 stale pin is refused STALE_STATE', 'STALE_STATE', errorCode(stale) || status(stale),
    errorCode(stale) === 'STALE_STATE', { refusalMessage: message(stale), ms: stale.ms });

  // SP-2: and the refusal did ZERO editor work, proven independently.
  const staleExists = await assetExists(sid, FOLDER, NAME_STALE);
  record('SP-2 refused mutation left NO asset behind (independent list oracle)',
    'asset absent', staleExists.verdict === null ? 'ORACLE_INCONCLUSIVE' : (staleExists.verdict ? 'asset present' : 'asset absent'),
    staleExists.verdict === false, { oracle: staleExists.readings });

  // SP-3: control - the identical call without a pin really mutates, so SP-1 was
  // refused by the precondition gate and not by some unrelated failure.
  const applied = await exec(sid, 'manage_asset', 'create_material',
    { name: NAME_OK, path: FOLDER, save: true }, CREATE_GRANT, null);
  record('SP-3 same call without a pin executes', 'success', status(applied), status(applied) === 'success', { ms: applied.ms });

  const okExists = await assetExists(sid, FOLDER, NAME_OK);
  record('SP-4 unpinned mutation DID create the asset (independent list oracle)',
    'asset present', okExists.verdict === null ? 'ORACLE_INCONCLUSIVE' : (okExists.verdict ? 'asset present' : 'asset absent'),
    okExists.verdict === true, { oracle: okExists.readings });

  // SP-5: a malformed pin is a validation refusal, never coerced into staleState.
  const malformed = await exec(sid, 'manage_asset', 'create_material',
    { name: NAME_STALE, path: FOLDER, save: true }, CREATE_GRANT,
    { expectedRevisions: { selection: 0 } });
  record('SP-5 out-of-range pin refused OUT_OF_RANGE (not staleState)', 'OUT_OF_RANGE',
    errorCode(malformed) || status(malformed), errorCode(malformed) === 'OUT_OF_RANGE', { refusalMessage: message(malformed) });

  // SP-6: an unknown pin name is refused rather than silently ignored, which
  // would otherwise let a typo'd precondition through unguarded.
  const unknownPin = await exec(sid, 'manage_asset', 'create_material',
    { name: NAME_STALE, path: FOLDER, save: true }, CREATE_GRANT,
    { expectedRevisions: { selektion: 2 } });
  record('SP-6 unknown pin name refused UNSUPPORTED_OPTION', 'UNSUPPORTED_OPTION',
    errorCode(unknownPin) || status(unknownPin), errorCode(unknownPin) === 'UNSUPPORTED_OPTION', { refusalMessage: message(unknownPin) });

  // Cleanup, then verify it with the same independent read. delete_asset reports
  // DELETE_FAILED when any listed path is absent, so the listing - not the
  // delete's own verdict - is what proves the /Game tree is clean again.
  const del = await exec(sid, 'manage_asset', 'delete_asset', { paths: [ASSET_STALE, ASSET_OK] }, DELETE_GRANT, null);
  const leftoverStale = await readAsset(sid, FOLDER, NAME_STALE);
  const leftoverOk = await readAsset(sid, FOLDER, NAME_OK);
  meta.cleanup = {
    deleteStatus: status(del), deleteMessage: message(del),
    staleAssetRemains: leftoverStale.verdict, appliedAssetRemains: leftoverOk.verdict,
    verifiedClean: leftoverStale.verdict === false && leftoverOk.verdict === false
  };
  await request('DELETE', null, { 'Mcp-Session-Id': sid, 'X-MCP-Capability-Token': TOKEN }, 5000);
}

main().then(() => {
  const passed = results.filter((r) => r.pass).length;
  for (const r of results) console.log(`${r.pass ? 'PASS' : 'FAIL'}  ${r.name}  [expected ${r.expected} | actual ${r.actual}]`);
  console.log(`\n${passed}/${results.length} passed`);
  if (meta.cleanup) console.log(`cleanup verified clean: ${meta.cleanup.verifiedClean}`);
  if (OUT) require('node:fs').writeFileSync(OUT, JSON.stringify({ ...meta, finishedAt: new Date().toISOString(), passed, total: results.length, results }, null, 2));
  process.exit(passed === results.length && results.length > 0 ? 0 : 1);
}).catch((e) => { console.error(e); process.exit(2); });
