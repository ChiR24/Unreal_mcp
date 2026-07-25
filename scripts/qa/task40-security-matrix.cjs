// Task 40 live security matrix — the reproducible evidence harness.
//
// WHY THIS FILE EXISTS: the original Task 40 claimed a 20/20 live matrix but
// shipped no harness, so the claim could not be re-run. This is that harness,
// checked in, so any reviewer can reproduce the result.
//
// TWO ORACLES, deliberately not one:
//
//   reachedQueue(code)  — "did this get PAST the pre-queue gate?" A pre-queue
//     refusal returns a POLICY code; anything else means the request was
//     enqueued and answered by a post-queue handler. INVALID_ARGUMENT counts as
//     reached, because every INVALID_ARGUMENT emitter lives in Private/Domains/**
//     and only runs post-queue on the game thread. This is the oracle every
//     NEGATIVE case needs, and it is what the gate is actually claimed to do.
//
//   provablyDispatched(code) — the stricter oracle a POSITIVE control needs.
//     NOT_IMPLEMENTED means precisely that no editor work happened, and UNKNOWN
//     means the harness could not classify the answer at all; a positive control
//     that lands on either proves nothing and must NOT pass. Cycle 1 used the
//     loose oracle for both, so "authorized native execute REACHES editor work"
//     was passing on NOT_IMPLEMENTED.
//
// Strongest of all is the HAPPY PATH (HP-*): it asserts success:true on real
// editor work and then proves the mutation happened EXACTLY ONCE.
//
// EVERY negative assertion is paired with a POSITIVE CONTROL proving the
// identical shape DOES execute for an authorized principal, so a refusal can
// never be a vacuous pass (e.g. from a server that refuses everything).
//
// SETUP (see scripts/qa/task40-security-matrix.README for the exact ini block):
//   DefaultGame.ini must define the qa* scoped tokens, bRequireCapabilityToken
//   must be True for the main pass, and the editor must be running with the
//   native MCP transport on port 3000 and the WebSocket bridge on 8090.
//
// USAGE: node scripts/qa/task40-security-matrix.cjs [--out <file>] [--b4-only]

const fs = require('node:fs');
const http = require('node:http');
const path = require('node:path');

const WS_PORT = Number(process.env.MCP_QA_WS_PORT || 8090);
const NATIVE_PORT = Number(process.env.MCP_QA_NATIVE_PORT || 3000);
const WS_URL = `ws://127.0.0.1:${WS_PORT}`;

const LEGACY = 'mcp-test-loopback-token';
const READER = 'qa-reader-tok';
const WRITER = 'qa-writer-tok';
const DESTROYER = 'qa-destroyer-tok';
const PATHBOUND = 'qa-pathbound-tok';
const QUOTA = 'qa-quota-tok';
const PROJBOUND = 'qa-projbound-tok';
const BADADMIN = 'qa-badadmin-tok';
const ALL_SECRETS = [LEGACY, READER, WRITER, DESTROYER, PATHBOUND, QUOTA, PROJBOUND, BADADMIN, '/Game/QAAllowed'];

const POLICY_CODES = [
  'SCOPE_NOT_GRANTED', 'CONSENT_REQUIRED', 'PATH_NOT_PERMITTED',
  'QUOTA_EXCEEDED', 'COMMAND_BLOCKED', 'PROJECT_NOT_PERMITTED'
];

function loadWebSocket() {
  for (const candidate of ['ws', path.join(process.cwd(), 'node_modules', 'ws')]) {
    try {
      return require(candidate);
    } catch {
      /* try the next resolution root */
    }
  }
  throw new Error('the ws package is required: run npm install first');
}
const WebSocket = loadWebSocket();

const results = [];
function record(surface, name, expected, actual, pass, extra) {
  results.push({ surface, name, expected, actual, pass, ...(extra || {}) });
  console.log(`${pass ? 'PASS' : 'FAIL'} [${surface}] ${name} :: expected=${expected} actual=${actual}`);
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
let seq = 0;
const rid = () => `t40-${Date.now().toString(36)}-${++seq}`;

// ─── WebSocket bridge ───────────────────────────────────────────────────────
function connect(token, timeoutMs = 9000) {
  return new Promise((resolve) => {
    const ws = new WebSocket(WS_URL);
    let done = false;
    const finish = (v) => { if (!done) { done = true; resolve(v); } };
    const timer = setTimeout(() => { try { ws.close(); } catch { /* closing */ } finish({ outcome: 'TIMEOUT' }); }, timeoutMs);
    ws.on('open', () => ws.send(JSON.stringify({ type: 'bridge_hello', capabilityToken: token })));
    ws.on('message', (d) => {
      let m; try { m = JSON.parse(d.toString()); } catch { return; }
      if (m.type === 'bridge_ack') { clearTimeout(timer); finish({ outcome: 'ACK', ws, ack: m }); }
      else if (m.type === 'bridge_error') { clearTimeout(timer); try { ws.close(); } catch { /* closing */ } finish({ outcome: 'ERROR', error: m.error }); }
    });
    ws.on('error', () => { clearTimeout(timer); finish({ outcome: 'SOCKET_ERROR' }); });
    ws.on('close', () => { clearTimeout(timer); finish({ outcome: 'CLOSED' }); });
  });
}

function request(ws, env, timeoutMs = 9000) {
  return new Promise((resolve) => {
    let done = false;
    const finish = (v) => { if (!done) { done = true; ws.off('message', onMsg); resolve(v); } };
    const timer = setTimeout(() => finish({ outcome: 'TIMEOUT' }), timeoutMs);
    function onMsg(d) {
      let m; try { m = JSON.parse(d.toString()); } catch { return; }
      if (m.type === 'automation_response' && m.requestId === env.requestId) {
        clearTimeout(timer); finish({ outcome: 'RESPONSE', msg: m });
      }
    }
    ws.on('message', onMsg);
    ws.send(JSON.stringify(env));
  });
}

const code = (r) => (r.outcome !== 'RESPONSE' ? r.outcome : (r.msg.errorCode || r.msg.error || (r.msg.success ? 'SUCCESS' : 'UNKNOWN')));
const reachedQueue = (c) => !POLICY_CODES.includes(c) && c !== 'TIMEOUT';

// Codes that prove the gate was passed but prove NOTHING about editor work.
const INCONCLUSIVE_CODES = ['NOT_IMPLEMENTED', 'UNKNOWN', 'METHOD_NOT_FOUND'];
const provablyDispatched = (c) => reachedQueue(c) && !INCONCLUSIVE_CODES.includes(c);

// Deep search, because handler payloads sit at different depths on the two
// transports and the harness must not silently miss the field it is asserting.
function findKey(node, key) {
  if (node === null || typeof node !== 'object') return undefined;
  if (Array.isArray(node)) {
    for (const item of node) {
      const hit = findKey(item, key);
      if (hit !== undefined) return hit;
    }
    return undefined;
  }
  if (Object.prototype.hasOwnProperty.call(node, key)) return node[key];
  for (const value of Object.values(node)) {
    const hit = findKey(value, key);
    if (hit !== undefined) return hit;
  }
  return undefined;
}

async function consoleProbe(ws, marker, extraPayload) {
  const r = await request(ws, {
    type: 'automation_request', requestId: rid(), action: 'console_command',
    payload: { command: marker, ...(extraPayload || {}) }
  });
  // `reached` is the loose oracle a NEGATIVE needs (tightening it would let a
  // negative pass vacuously); `dispatched` is the strict one a POSITIVE needs.
  return {
    c: code(r), reached: reachedQueue(code(r)),
    dispatched: provablyDispatched(code(r)), raw: r.msg
  };
}

// ─── happy-path helpers ─────────────────────────────────────────────────────
function wsAsset(ws, subAction, params, consent) {
  const env = {
    type: 'automation_request', requestId: rid(), action: 'manage_asset',
    payload: { subAction, ...params }
  };
  if (consent) env.consent = consent;
  return request(ws, env, 20000);
}

// `paths` is the one delete parameter honoured on BOTH sides: it is declared in
// the capability schema (so native execute's additionalProperties:false accepts
// it) and read by HandleDeleteAssets. The schema's sibling `assetPath` is NOT
// honoured -- that handler reads `path` -- which is a pre-existing catalog
// defect reported separately, not something this matrix works around.
const deleteParams = (assetPath) => ({ paths: [assetPath] });

async function wsAssetExists(ws, assetPath) {
  const r = await wsAsset(ws, 'exists', { assetPath });
  return { found: findKey(r.msg || {}, 'exists'), c: code(r) };
}

// ─── native /mcp ────────────────────────────────────────────────────────────
function post(body, headers, timeoutMs = 12000) {
  return new Promise((resolve) => {
    const payload = JSON.stringify(body);
    const req = http.request({
      host: '127.0.0.1', port: NATIVE_PORT, path: '/mcp', method: 'POST',
      // Undefined values are dropped rather than passed to setHeader, which
      // throws. A failed `initialize` must show up as a FAILING CASE, never as
      // a harness crash that discards every result recorded so far.
      headers: Object.fromEntries(
        Object.entries({
          'Content-Type': 'application/json',
          Accept: 'application/json, text/event-stream',
          'Content-Length': Buffer.byteLength(payload),
          ...headers
        }).filter(([, v]) => v !== undefined && v !== null)
      )
    }, (res) => {
      let d = '';
      res.on('data', (c) => (d += c));
      res.on('end', () => { let j = null; try { j = JSON.parse(d); } catch { /* non-JSON body */ } resolve({ status: res.statusCode, headers: res.headers, body: d, json: j }); });
    });
    req.setTimeout(timeoutMs, () => { req.destroy(); resolve({ status: 0, headers: {}, body: 'TIMEOUT', json: null }); });
    req.on('error', (e) => resolve({ status: -1, headers: {}, body: String(e), json: null }));
    req.write(payload); req.end();
  });
}

function nCode(res) {
  if (res.status !== 200) return `HTTP_${res.status}`;
  const txt = res.body;
  const m = txt.match(/"errorCode":"([A-Z_]+)"/);
  if (m) return m[1];
  if (/"success":true/.test(txt)) return 'SUCCESS';
  const t = txt.match(/"code":"([A-Z_]+)"/);
  return t ? t[1] : 'UNKNOWN';
}

// The native counterpart of provablyDispatched, for the same reason: a native
// POSITIVE control that lands on NOT_IMPLEMENTED, UNKNOWN or an HTTP-level
// failure has proven nothing about editor work and must not pass.
const nDispatched = (c) =>
  !POLICY_CODES.includes(c) && !INCONCLUSIVE_CODES.includes(c) && !String(c).startsWith('HTTP_');

// The native surface may answer as plain JSON or as an SSE frame; both carry
// the same JSON-RPC envelope, and the tool payload is a JSON STRING inside it.
// Deliberately SYNCHRONOUS: `toolPayload` reads its result directly, so an
// accidental `async` here would hand every caller a Promise and make every
// field assertion below silently read `undefined`.
function bodyJson(res) {
  if (res.json) return res.json;
  const line = String(res.body).split('\n').find((l) => l.startsWith('data:'));
  if (!line) return null;
  try { return JSON.parse(line.slice(5).trim()); } catch { return null; }
}

function toolPayload(res) {
  const envelope = bodyJson(res);
  const text = envelope?.result?.content?.[0]?.text;
  if (typeof text === 'string') {
    try { return JSON.parse(text); } catch { return envelope?.result ?? null; }
  }
  return envelope?.result ?? null;
}

// Every session this run opens, so they can be torn down afterwards. The native
// transport caps concurrent sessions; leaking them makes the SECOND run of the
// harness fail with 429/400 on everything, which looks like a product fault.
const openSessions = [];

async function nativeInit(token) {
  const res = await post({
    jsonrpc: '2.0', id: 1, method: 'initialize',
    params: { protocolVersion: '2025-06-18', capabilities: {}, clientInfo: { name: 'task40-security-matrix', version: '1.0.0' } }
  }, token ? { 'X-MCP-Capability-Token': token } : {});
  const sid = res.headers['mcp-session-id'] || res.json?.result?.sessionId;
  if (sid) openSessions.push({ sid, token });
  return { res, sid };
}

function closeSessions() {
  return Promise.all(openSessions.splice(0).map(({ sid, token }) => new Promise((resolve) => {
    const headers = { 'Mcp-Session-Id': sid };
    if (token) headers['X-MCP-Capability-Token'] = token;
    const req = http.request(
      { host: '127.0.0.1', port: NATIVE_PORT, path: '/mcp', method: 'DELETE', headers },
      (res) => { res.resume(); res.on('end', resolve); }
    );
    req.setTimeout(5000, () => { req.destroy(); resolve(); });
    req.on('error', () => resolve());
    req.end();
  })));
}

function nativeExec(sid, token, tool, action, params, consent) {
  const args = { operation: 'execute', tool, action, params: params || {} };
  if (consent) args.consent = consent;
  const h = { 'Mcp-Session-Id': sid };
  if (token) h['X-MCP-Capability-Token'] = token;
  return post({ jsonrpc: '2.0', id: 2, method: 'tools/call', params: { name: 'unreal', arguments: args } }, h);
}

function nativeMethod(sid, token, method, params) {
  const h = { 'Mcp-Session-Id': sid, 'MCP-Protocol-Version': '2025-06-18' };
  if (token) h['X-MCP-Capability-Token'] = token;
  return post({ jsonrpc: '2.0', id: 3, method, params: params || {} }, h);
}

// ─── Blocker 4: a presented-but-unresolvable token must be refused ──────────
// Run with bRequireCapabilityToken=False so the ONLY thing that can refuse the
// handshake is the presented-token rule this blocker added.
async function runB4Only() {
  const bad = await connect('definitely-not-a-configured-token');
  record('ws', 'B4-1 non-empty unresolvable token refused even when no token is REQUIRED',
    'INVALID_CAPABILITY_TOKEN', bad.error || bad.outcome, bad.error === 'INVALID_CAPABILITY_TOKEN');

  const none = await connect('');
  const na = none.ack?.authority || {};
  record('ws', 'B4-2 POSITIVE CONTROL: no-token loopback still binds admin',
    'ACK profile=loopback scopes=[admin]', `${none.outcome} profile=${na.profile} scopes=[${na.scopes}]`,
    none.outcome === 'ACK' && na.profile === 'loopback' && na.scopes?.join(',') === 'admin');

  const good = await connect(READER);
  const ga = good.ack?.authority || {};
  record('ws', 'B4-3 POSITIVE CONTROL: a VALID scoped token still authenticates',
    'ACK profile=scoped:qareader', `${good.outcome} profile=${ga.profile}`,
    good.outcome === 'ACK' && ga.profile === 'scoped:qareader');

  const nBad = await nativeInit('definitely-not-a-configured-token');
  record('native', 'B4-4 native refuses a non-empty unresolvable token with no token required',
    '401', String(nBad.res.status), nBad.res.status === 401);

  const nNone = await nativeInit(undefined);
  record('native', 'B4-5 POSITIVE CONTROL: native no-token loopback initialize still succeeds',
    '200 + session', `${nNone.res.status} + ${nNone.sid ? 'session' : 'NO-SESSION'}`,
    nNone.res.status === 200 && !!nNone.sid);

  for (const c of [none, good]) { try { c.ws && c.ws.close(); } catch { /* closing */ } }
}

// The plan's happy-path acceptance criterion: "a correctly scoped and consented
// request executes ONCE". Cycle 1 had zero cases with success:true, so the
// SUCCESS branch of the oracle had never fired and double-dispatch was untested
// in both directions. This proves the mutation happened, and happened once.
async function runHappyPath(ws, grant, createGrant) {
  const folder = '/Game/QAHappyProbe';
  const assetPath = `${folder}/M_QAHappyProbe`;

  // Best-effort pre-clean so a rerun after a mid-way failure still starts empty.
  await wsAsset(ws, 'delete_asset', deleteParams(assetPath), grant);

  const created = await wsAsset(
    ws, 'create_material', { name: 'M_QAHappyProbe', path: folder, save: true }, createGrant);
  record('ws', 'HP-1 HAPPY PATH: a correctly scoped request EXECUTES (success:true)',
    'SUCCESS', code(created), code(created) === 'SUCCESS', { message: created.msg?.message });

  const before = await wsAssetExists(ws, assetPath);
  record('ws', 'HP-2 the created asset is really there (independent re-query)',
    'exists=true', `exists=${before.found} (${before.c})`, before.found === true);

  const deleted = await wsAsset(ws, 'delete_asset', deleteParams(assetPath), grant);
  record('ws', 'HP-3 the consented destructive request EXECUTES (success:true)',
    'SUCCESS', code(deleted), code(deleted) === 'SUCCESS', { message: deleted.msg?.message });

  const after = await wsAssetExists(ws, assetPath);
  record('ws', 'HP-4 EXACTLY ONCE (a): the delete really took effect',
    'exists=false', `exists=${after.found} (${after.c})`, after.found === false);

  // A second IDENTICAL dispatch must not succeed again. If it did, the same
  // envelope would be replayable into a second mutation — which is precisely
  // the regression a pre-queue gate in front of retained post-queue validation
  // could introduce, and which nothing else in this matrix would notice.
  const replay = await wsAsset(ws, 'delete_asset', deleteParams(assetPath), grant);
  const replayCode = code(replay);
  // A classified refusal, not merely "anything but SUCCESS": a TIMEOUT or an
  // unclassified answer would otherwise satisfy this without proving anything.
  record('ws', 'HP-5 EXACTLY ONCE (b): an identical re-dispatch does NOT delete again',
    'a classified non-SUCCESS answer', replayCode,
    replayCode !== 'SUCCESS' && replayCode !== 'TIMEOUT' && replayCode !== 'UNKNOWN',
    { message: replay.msg?.message });

  const stillGone = await wsAssetExists(ws, assetPath);
  record('ws', 'HP-6 EXACTLY ONCE (c): state is unchanged by the replay',
    'exists=false', `exists=${stillGone.found}`, stillGone.found === false);
}

async function runMain() {
  // ===== the consent grant is READ FROM describe, never hardcoded =====
  // If describe advertises a grant the gate then rejects, the documented
  // discovery -> execute loop is broken and every HP case below fails.
  const initGrantSession = await nativeInit(LEGACY);
  const describeGrant = async (tool, action) => {
    const res = await post({
      jsonrpc: '2.0', id: 9, method: 'tools/call',
      params: { name: 'unreal', arguments: { operation: 'describe', tool, action } }
    }, { 'Mcp-Session-Id': initGrantSession.sid, 'X-MCP-Capability-Token': LEGACY });
    return findKey(toolPayload(res) ?? {}, 'consentGrant');
  };

  // Both acknowledgement levels, because they are separate branches of the
  // gate: delete_asset demands `elevated`, create_material only `explicit`.
  const emittedGrant = await describeGrant('manage_asset', 'delete_asset');
  record('native', 'HP-0 describe emits the exact consent grant execute requires',
    'consentGrant {capability: asset.delete_asset, acknowledge: elevated}',
    JSON.stringify(emittedGrant ?? null),
    !!emittedGrant && emittedGrant.capability === 'asset.delete_asset'
      && emittedGrant.acknowledge === 'elevated');
  const grant = emittedGrant ?? { capability: 'asset.delete_asset', acknowledge: 'elevated' };

  const emittedCreateGrant = await describeGrant('manage_asset', 'create_material');
  record('native', 'HP-0b describe emits the EXPLICIT grant for a write capability',
    'consentGrant {capability: material.create_material, acknowledge: explicit}',
    JSON.stringify(emittedCreateGrant ?? null),
    !!emittedCreateGrant && emittedCreateGrant.capability === 'material.create_material'
      && emittedCreateGrant.acknowledge === 'explicit');
  const createGrant = emittedCreateGrant ?? { capability: 'material.create_material', acknowledge: 'explicit' };

  // ===== handshake / authority =====
  for (const [tok, label] of [
    [BADADMIN, 'S01 scoped-Admin entry is invalid -> refused at handshake'],
    [PROJBOUND, 'S02 project-bound principal (wrong project) refused at handshake'],
    ['totally-wrong', 'S03 unknown token refused at handshake']
  ]) {
    const c = await connect(tok);
    record('ws', label, 'INVALID_CAPABILITY_TOKEN', c.error || c.outcome, c.error === 'INVALID_CAPABILITY_TOKEN');
  }

  const legacy = await connect(LEGACY);
  const la = legacy.ack?.authority || {};
  record('ws', 'S04 legacy token keeps admin authority, flagged deprecated',
    'profile=legacy scopes=[admin] deprecated=true',
    `profile=${la.profile} scopes=[${la.scopes}] deprecated=${la.deprecated}`,
    la.profile === 'legacy' && la.scopes?.join(',') === 'admin' && la.deprecated === true);

  const ackText = JSON.stringify(legacy.ack || {});
  const hits = ALL_SECRETS.filter((s) => ackText.includes(s));
  record('ws', 'S05 bridge_ack leaks NO token / path prefix / numeric limit', 'no secret substrings',
    `hits=[${hits}] keys=${Object.keys(la).sort().join(',')}`,
    hits.length === 0 && Object.keys(la).sort().join(',') === 'deprecated,pathRestricted,profile,projectRestricted,scopes,tokenRequired');

  const reader = await connect(READER);
  const ra = reader.ack?.authority || {};
  record('ws', 'S06 scoped reader authenticates with narrowed authority',
    'profile=scoped:qareader scopes=[read]', `profile=${ra.profile} scopes=[${ra.scopes}]`,
    ra.profile === 'scoped:qareader' && ra.scopes?.join(',') === 'read');

  const pathb = await connect(PATHBOUND);
  const pa = pathb.ack?.authority || {};
  const pathbAck = JSON.stringify(pathb.ack ?? {});
  record('ws', 'S07 path-bound principal advertises pathRestricted WITHOUT the prefix',
    'pathRestricted=true, prefix absent',
    `pathRestricted=${pa.pathRestricted} prefixLeaked=${pathbAck.includes('/Game/QAAllowed')}`,
    pa.pathRestricted === true && !pathbAck.includes('/Game/QAAllowed'));

  // ===== POSITIVE CONTROL FIRST =====
  const pos1 = await consoleProbe(legacy.ws, 'QAPOSCTRL_ADMIN_1');
  record('ws', 'S08 POSITIVE CONTROL: authorized console_command REACHES editor work',
    'handler-level code (post-dispatch)', `${pos1.c} dispatched=${pos1.dispatched}`, pos1.dispatched,
    { handlerMessage: pos1.raw?.message });

  const neg1 = await consoleProbe(reader.ws, 'QANEGCTRL_READER_1');
  record('ws', 'S09 read-only principal refused a WRITE action (console_command)',
    'SCOPE_NOT_GRANTED + never reached editor', `${neg1.c} reachedQueue=${neg1.reached}`,
    neg1.c === 'SCOPE_NOT_GRANTED' && !neg1.reached);

  const r2 = await request(reader.ws, { type: 'automation_request', requestId: rid(), action: 'no_such_action_qa_xyz', payload: {} });
  record('ws', 'S10 unknown action fails CLOSED (unmatched action demands admin)',
    'SCOPE_NOT_GRANTED', code(r2), code(r2) === 'SCOPE_NOT_GRANTED');

  const blocked = await consoleProbe(legacy.ws, 'quit');
  record('ws', 'S11 blocked console command refused pre-queue EVEN FOR ADMIN',
    'COMMAND_BLOCKED + never reached editor', `${blocked.c} reachedQueue=${blocked.reached}`,
    blocked.c === 'COMMAND_BLOCKED' && !blocked.reached);

  const blockedArr = await request(legacy.ws, { type: 'automation_request', requestId: rid(), action: 'console_command', payload: { commands: ['stat fps', 'exit'] } });
  record('ws', 'S12 blocked command hidden in a commands[] ARRAY is still caught',
    'COMMAND_BLOCKED', code(blockedArr), code(blockedArr) === 'COMMAND_BLOCKED');

  const blockedNested = await request(legacy.ws, { type: 'automation_request', requestId: rid(), action: 'console_command', payload: { options: { command: 'quit' } } });
  record('ws', 'S13 blocked command NESTED below the top level is still caught (N5)',
    'COMMAND_BLOCKED', code(blockedNested), code(blockedNested) === 'COMMAND_BLOCKED');

  // ===== BLOCKER 1: a decoy payload.action must not steer the demand =====
  const b1pos = await request(reader.ws, { type: 'automation_request', requestId: rid(), action: 'control_actor', payload: { subAction: 'find_by_class', className: 'StaticMeshActor' } });
  record('ws', 'B1-0 POSITIVE CONTROL: the read principal CAN run the read action it names',
    'reaches editor work', `${code(b1pos)} dispatched=${provablyDispatched(code(b1pos))}`, provablyDispatched(code(b1pos)));

  const b1decoy = await request(reader.ws, {
    type: 'automation_request', requestId: rid(), action: 'system_control',
    payload: { subAction: 'console_command', action: 'find_by_class', command: 'QA_B1_DECOY' }
  });
  record('ws', 'B1-1 decoy payload.action does NOT downgrade a write action to read',
    'SCOPE_NOT_GRANTED + never reached editor',
    `${code(b1decoy)} reachedQueue=${reachedQueue(code(b1decoy))}`,
    code(b1decoy) === 'SCOPE_NOT_GRANTED' && !reachedQueue(code(b1decoy)));

  const b1decoy2 = await request(reader.ws, {
    type: 'automation_request', requestId: rid(), action: 'console_command',
    payload: { action: 'find_by_class', command: 'QA_B1_DECOY2' }
  });
  record('ws', 'B1-2 decoy payload.action with NO subAction still cannot lower the demand',
    'SCOPE_NOT_GRANTED', code(b1decoy2), code(b1decoy2) === 'SCOPE_NOT_GRANTED');

  const b1cross = await request(reader.ws, {
    type: 'automation_request', requestId: rid(), action: 'manage_level',
    payload: { subAction: 'find_by_class' }
  });
  record('ws', 'B1-3 an action that does not belong to the named parent fails CLOSED',
    'SCOPE_NOT_GRANTED', code(b1cross), code(b1cross) === 'SCOPE_NOT_GRANTED');

  // ===== consent =====
  const destroyer = await connect(DESTROYER);
  const d1 = await request(destroyer.ws, { type: 'automation_request', requestId: rid(), action: 'manage_asset', payload: { subAction: 'delete_asset', assetPath: '/Game/QAProbeAsset' } });
  record('ws', 'S14 destructive action without consent refused', 'CONSENT_REQUIRED', code(d1), code(d1) === 'CONSENT_REQUIRED');

  const d2 = await request(destroyer.ws, { type: 'automation_request', requestId: rid(), action: 'manage_asset', payload: { subAction: 'delete_asset', assetPath: '/Game/QAProbeAsset' }, consent: { capability: 'control_actor.delete', acknowledge: 'elevated' } });
  record('ws', 'S15 ADVERSARIAL consent naming a DIFFERENT capability is rejected', 'CONSENT_REQUIRED', code(d2), code(d2) === 'CONSENT_REQUIRED');

  const d3 = await request(destroyer.ws, { type: 'automation_request', requestId: rid(), action: 'manage_asset', payload: { subAction: 'delete_asset', assetPath: '/Game/QAProbeAsset' }, consent: { capability: 'asset.delete_asset', acknowledge: 'explicit' } });
  record('ws', 'S16 ADVERSARIAL weaker explicit ack cannot satisfy an elevated demand', 'CONSENT_REQUIRED', code(d3), code(d3) === 'CONSENT_REQUIRED');

  const d4 = await request(destroyer.ws, { type: 'automation_request', requestId: rid(), action: 'manage_asset', payload: { subAction: 'delete_asset', assetPath: '/Game/QAProbeAsset' }, consent: { capability: 'asset.delete_asset', acknowledge: 'elevated' } });
  record('ws', 'S17 POSITIVE CONTROL: the correct grant lets the destructive action through the gate',
    'not a policy refusal', `${code(d4)} dispatched=${provablyDispatched(code(d4))}`, provablyDispatched(code(d4)));

  // ===== BLOCKER 2: path confinement survives aliasing =====
  const p0 = await consoleProbe(pathb.ws, 'QAPOSCTRL_PATHBOUND_OK');
  record('ws', 'B2-0 POSITIVE CONTROL: path-bound principal with NO path in payload is allowed',
    'reaches editor work', `${p0.c} dispatched=${p0.dispatched}`, p0.dispatched);

  const pIn = await consoleProbe(pathb.ws, 'QAPATH_IN', { assetPath: '/Game/QAAllowed/Inside' });
  record('ws', 'B2-0b POSITIVE CONTROL: a path INSIDE the allowed prefix is permitted',
    'reaches editor work', `${pIn.c} dispatched=${pIn.dispatched}`, pIn.dispatched);

  const pInAlias = await consoleProbe(pathb.ws, 'QAPATH_IN_ALIAS', { assetPath: '/Content/QAAllowed/Inside' });
  record('ws', 'B2-0c POSITIVE CONTROL: a /Content alias INSIDE the prefix is permitted (not blanket-denied)',
    'reaches editor work', `${pInAlias.c} dispatched=${pInAlias.dispatched}`, pInAlias.dispatched);

  const cases = [
    ['B2-1 /Content alias outside the prefix is confined', { assetPath: '/Content/Forbidden/Thing' }],
    ['B2-2 backslash \\Content\\ alias is confined', { assetPath: '\\Content\\Forbidden\\Thing' }],
    ['B2-3 lower-case /content alias is confined', { assetPath: '/content/Forbidden/Thing' }],
    ['B2-4 bare relative path is confined', { folderPath: 'Forbidden/Thing' }],
    ['B2-5 bare single token under a path key is confined', { packagePath: 'Forbidden' }],
    ['B2-6 doubled separators do not evade confinement', { assetPath: '//Content//Forbidden' }],
    ['B2-7 traversal out of the allowed prefix is refused', { assetPath: '/Game/QAAllowed/../Forbidden' }],
    ['B2-8 sibling prefix /Game/QAAllowedOther is outside /Game/QAAllowed', { assetPath: '/Game/QAAllowedOther/Thing' }],
    ['B2-9 alias smuggled under an unusual KEY NAME is still gated', { zzz_unusual_key: '/Content/Forbidden/Smuggled' }],
    ['B2-10 alias nested deep in the payload is still gated', { nested: { deeper: { assetPath: '/Content/Forbidden/Deep' } } }]
  ];
  for (const [label, extra] of cases) {
    const probe = await consoleProbe(pathb.ws, `QA_${label.slice(0, 5).replace(/[^A-Za-z0-9]/g, '')}`, extra);
    record('ws', label, 'PATH_NOT_PERMITTED + never reached editor',
      `${probe.c} reachedQueue=${probe.reached}`, probe.c === 'PATH_NOT_PERMITTED' && !probe.reached);
  }

  // ===== quota =====
  const q = await connect(QUOTA);
  const qc = [];
  for (let i = 0; i < 3; i++) {
    qc.push(code(await request(q.ws, { type: 'automation_request', requestId: rid(), action: 'control_actor', payload: { subAction: 'find_by_class', className: 'StaticMeshActor' } }, 6000)));
  }
  record('ws', 'S18 quota exhausted after the configured 2/min', 'QUOTA_EXCEEDED on the 3rd', qc.join(','), qc[2] === 'QUOTA_EXCEEDED', { attempts: qc });

  try { q.ws.close(); } catch { /* closing */ }
  await sleep(1000);
  const q2 = await connect(QUOTA);
  const q4 = code(await request(q2.ws, { type: 'automation_request', requestId: rid(), action: 'control_actor', payload: { subAction: 'find_by_class', className: 'StaticMeshActor' } }, 6000));
  record('ws', 'S19 quota SURVIVES reconnect on a fresh socket (principal-wide, not per-socket)', 'QUOTA_EXCEEDED', q4, q4 === 'QUOTA_EXCEEDED');

  const posAfter = await consoleProbe(legacy.ws, 'QAPOSCTRL_ADMIN_2');
  record('ws', 'S20 POSITIVE CONTROL: an UNRELATED principal is unaffected by another principal quota',
    'reaches editor work', `${posAfter.c} dispatched=${posAfter.dispatched}`, posAfter.dispatched);

  // ===== happy path (runs on the admin socket, before teardown) =====
  await runHappyPath(legacy.ws, grant, createGrant);

  for (const c of [legacy, reader, destroyer, pathb, q2]) { try { c.ws && c.ws.close(); } catch { /* closing */ } }

  // ===== native /mcp =====
  const initReader = await nativeInit(READER);
  record('native', 'S21 initialize with a scoped token establishes a session', '200 + session',
    `${initReader.res.status} + ${initReader.sid ? 'session' : 'NO-SESSION'}`,
    initReader.res.status === 200 && !!initReader.sid);

  const initLegacy = await nativeInit(LEGACY);
  // S22 used to target console_command, which the native execute path answers
  // with NOT_IMPLEMENTED — a code whose MEANING is "no editor work happened", so
  // the positive control passed vacuously. It now targets an implemented
  // capability and demands success:true, i.e. real editor work.
  const nativeFolder = '/Game/QAHappyProbeNative';
  const nativeAsset = `${nativeFolder}/M_QAHappyProbeNative`;
  await nativeExec(initLegacy.sid, LEGACY, 'manage_asset', 'delete_asset', deleteParams(nativeAsset), grant);

  const nPos = await nativeExec(initLegacy.sid, LEGACY, 'manage_asset', 'create_material',
    { name: 'M_QAHappyProbeNative', path: nativeFolder, save: true }, createGrant);
  record('native', 'S22 POSITIVE CONTROL: authorized native execute performs REAL editor work',
    'SUCCESS', nCode(nPos), nCode(nPos) === 'SUCCESS' && nPos.status === 200);

  const nDelOnce = await nativeExec(initLegacy.sid, LEGACY, 'manage_asset', 'delete_asset',
    deleteParams(nativeAsset), grant);
  record('native', 'HP-7 HAPPY PATH (native): the consented destructive request EXECUTES',
    'SUCCESS', nCode(nDelOnce), nCode(nDelOnce) === 'SUCCESS');

  // The native receipt reports `data.success` but does not surface the handler's
  // own `exists` boolean, so the authoritative answer here is the receipt
  // message. (That the native receipt drops handler detail fields the WebSocket
  // response carries is a separate gap, reported with this run.)
  const nExists = await nativeExec(initLegacy.sid, LEGACY, 'manage_asset', 'exists', { assetPath: nativeAsset });
  const nExistsPayload = toolPayload(nExists) ?? {};
  const nExistsFlag = findKey(nExistsPayload, 'exists');
  const nExistsMessage = String(findKey(nExistsPayload, 'message') ?? '');
  record('native', 'HP-8 EXACTLY ONCE (native a): the delete really took effect',
    'the re-query reports the asset is gone',
    `exists=${nExistsFlag} message="${nExistsMessage}"`,
    nExistsFlag === false || /does not exist/iu.test(nExistsMessage));

  const nReplay = await nativeExec(initLegacy.sid, LEGACY, 'manage_asset', 'delete_asset',
    deleteParams(nativeAsset), grant);
  const nReplayCode = nCode(nReplay);
  record('native', 'HP-9 EXACTLY ONCE (native b): an identical re-dispatch does NOT delete again',
    'a classified non-SUCCESS answer', nReplayCode,
    nReplayCode !== 'SUCCESS' && nReplayCode !== 'UNKNOWN' && !nReplayCode.startsWith('HTTP_'));

  const nNeg = await nativeExec(initReader.sid, READER, 'system_control', 'console_command', { command: 'QANATIVENEG1' });
  record('native', 'S23 read-only principal refused a WRITE action', 'SCOPE_NOT_GRANTED', nCode(nNeg), nCode(nNeg) === 'SCOPE_NOT_GRANTED');

  const nDecoy = await post({
    jsonrpc: '2.0', id: 2, method: 'tools/call',
    params: { name: 'unreal', arguments: { operation: 'execute', tool: 'system_control', action: 'console_command', params: { action: 'find_by_class', command: 'QA_B1_NATIVE_DECOY' } } }
  }, { 'Mcp-Session-Id': initReader.sid, 'X-MCP-Capability-Token': READER });
  // The native surface resolves the capability id server-side and validates
  // params against that capability's declared schema, so an undeclared `action`
  // decoy is refused at validation and never reaches the demand at all. Either
  // refusal closes the bypass; being ADMITTED is the only failing outcome.
  const nDecoyCode = nCode(nDecoy);
  record('native', 'B1-4 decoy params.action cannot get a read principal past a write action',
    'refused (INVALID_PARAMS at schema validation, or SCOPE_NOT_GRANTED at the gate)',
    nDecoyCode, nDecoyCode === 'INVALID_PARAMS' || nDecoyCode === 'SCOPE_NOT_GRANTED');

  const swap = await nativeExec(initReader.sid, LEGACY, 'system_control', 'console_command', { command: 'QANATIVESWAP' });
  record('native', 'S24 token swap on an ESTABLISHED session refused', '401', String(swap.status), swap.status === 401);

  const nBlocked = await nativeExec(initLegacy.sid, LEGACY, 'system_control', 'console_command', { command: 'quit' });
  record('native', 'S25 blocked console command refused pre-queue', 'COMMAND_BLOCKED', nCode(nBlocked), nCode(nBlocked) === 'COMMAND_BLOCKED');

  const initDes = await nativeInit(DESTROYER);
  const nDel = await nativeExec(initDes.sid, DESTROYER, 'manage_asset', 'delete_asset', { assetPath: '/Game/QAProbeAsset' });
  record('native', 'S26 destructive action without consent refused', 'CONSENT_REQUIRED', nCode(nDel), nCode(nDel) === 'CONSENT_REQUIRED');

  const nDelOk = await nativeExec(initDes.sid, DESTROYER, 'manage_asset', 'delete_asset', { assetPath: '/Game/QAProbeAsset' }, { capability: 'asset.delete_asset', acknowledge: 'elevated' });
  // "not a consent refusal" alone also accepted NOT_IMPLEMENTED and UNKNOWN, so
  // this control could pass on an answer that proves no editor work happened.
  record('native', 'B3-1 POSITIVE CONTROL: consent supplied as the DECLARED sibling is accepted',
    'accepted AND provably dispatched', `${nCode(nDelOk)} dispatched=${nDispatched(nCode(nDelOk))}`,
    nDispatched(nCode(nDelOk)) && nCode(nDelOk) !== 'CONSENT_REQUIRED' && nCode(nDelOk) !== 'INVALID_CONSENT');

  // ===== BLOCKER 6: MCP primitives are not an ungated read channel =====
  const initWriter = await nativeInit(WRITER);
  const readerList = await nativeMethod(initReader.sid, READER, 'resources/list');
  record('native', 'B6-0 POSITIVE CONTROL: a read principal CAN list resources',
    '200 + resources', `${readerList.status}/${nCode(readerList)}`,
    readerList.status === 200 && /"resources"/.test(readerList.body));

  const writerList = await nativeMethod(initWriter.sid, WRITER, 'resources/list');
  record('native', 'B6-1 a write-only principal is refused resources/list',
    'SCOPE_NOT_GRANTED', nCode(writerList), nCode(writerList) === 'SCOPE_NOT_GRANTED');

  const writerRead = await nativeMethod(initWriter.sid, WRITER, 'resources/read', { uri: 'ue://project' });
  record('native', 'B6-2 a write-only principal is refused resources/read',
    'SCOPE_NOT_GRANTED', nCode(writerRead), nCode(writerRead) === 'SCOPE_NOT_GRANTED');

  const writerTools = await nativeMethod(initWriter.sid, WRITER, 'tools/list');
  record('native', 'B6-3 a write-only principal is refused tools/list',
    'SCOPE_NOT_GRANTED', nCode(writerTools), nCode(writerTools) === 'SCOPE_NOT_GRANTED');

  const readerTools = await nativeMethod(initReader.sid, READER, 'tools/list');
  record('native', 'B6-4 POSITIVE CONTROL: a read principal CAN list tools',
    '200 + unreal tool', `${readerTools.status}/${/"unreal"/.test(readerTools.body)}`,
    readerTools.status === 200 && /"unreal"/.test(readerTools.body));

  const unknown = await nativeMethod(initReader.sid, READER, 'definitely/not/a/method');
  // Asserting only "not a policy code" also accepted UNKNOWN, so the case could
  // pass on an answer the harness never classified. Demand the actual JSON-RPC
  // method-not-found error (-32601) instead.
  const unknownIsMethodNotFound =
    /-32601/.test(unknown.body) || nCode(unknown) === 'METHOD_NOT_FOUND';
  record('native', 'B6-5 an unknown method is METHOD-NOT-FOUND (-32601), not a policy refusal',
    'jsonrpc -32601 and no policy code',
    `${nCode(unknown)} methodNotFound=${unknownIsMethodNotFound}`,
    unknownIsMethodNotFound && !POLICY_CODES.includes(nCode(unknown)));

  // ===== parity + secret scan =====
  const parity = nCode(nNeg) === 'SCOPE_NOT_GRANTED' && neg1.c === 'SCOPE_NOT_GRANTED'
    && nCode(nBlocked) === 'COMMAND_BLOCKED' && blocked.c === 'COMMAND_BLOCKED'
    && nCode(nDel) === 'CONSENT_REQUIRED' && code(d1) === 'CONSENT_REQUIRED';
  record('both', 'S27 TYPED PARITY: identical code strings on ws and native for the same violation',
    'ws==native for scope/command/consent',
    `ws=[${neg1.c},${blocked.c},${code(d1)}] native=[${nCode(nNeg)},${nCode(nBlocked)},${nCode(nDel)}]`, parity);

  const bodies = [nNeg.body, nBlocked.body, nDel.body, swap.body, writerList.body, writerTools.body, nDecoy.body].join('|');
  const nSecrets = ALL_SECRETS.filter((s) => bodies.includes(s));
  record('native', 'S28 native refusal receipts leak no token / path prefix', 'no secret substrings',
    `hits=[${nSecrets}]`, nSecrets.length === 0);
}

async function main() {
  const args = process.argv.slice(2);
  const outIndex = args.indexOf('--out');
  const outFile = outIndex >= 0 ? args[outIndex + 1] : null;

  if (args.includes('--b4-only')) {
    await runB4Only();
  } else {
    await runMain();
  }

  await closeSessions();

  const passed = results.filter((r) => r.pass).length;
  console.log(`\n==== ${passed}/${results.length} PASSED ====`);
  if (outFile) fs.writeFileSync(outFile, JSON.stringify(results, null, 2));
  process.exit(passed === results.length ? 0 : 1);
}

main().catch((e) => { console.error('HARNESS ERROR', e); process.exit(2); });
