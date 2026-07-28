#!/usr/bin/env node
// Task 43 live preview / compensation probe (native /mcp).
//
// Proves end to end against a running editor the two claims that unit and
// automation tests cannot reach, because both are about what the EDITOR did:
//
//   (a) options.preview:true on a real DESTRUCTIVE capability (asset.delete_asset,
//       effect read from describe at runtime) is refused UNSUPPORTED_PREVIEW and
//       performs ZERO editor work - in both directions: the delete target is
//       still there afterwards, and a refused preview:true create leaves no asset
//       behind.
//
//   (b) control_editor.save_all - the one capability that actually constructs an
//       FMcpCompensationReceipt - reports PARTIAL completion honestly when a
//       multi-package save is non-atomic, and its report matches what is really
//       on disk.
//
// Both oracles are INDEPENDENT of the call being judged:
//   (a) a separate manage_asset.list read whose data.assets array is parsed. The
//       refusing call's own response is never trusted, so a forged "refused"
//       cannot pass - the asset must genuinely be present/absent.
//   (b) the filesystem. The receipt says which packages were written and which
//       were not; this probe stats those exact files. A receipt that claims a
//       save landed when no file exists fails here.
//
// The partial save is produced without touching the plugin: two packages are
// created dirty (save:false marks the package dirty and skips the save), then one
// target DIRECTORY is made read-only immediately before save_all, so that package
// cannot be written while the other can. Nothing is mocked - the save genuinely
// half-fails.
//
// Setup mirrors scripts/qa/task40-security-matrix.README: live UE editor, native
// MCP on 3000, bRequireCapabilityToken=True, CapabilityToken=mcp-test-loopback-token.
//
// LAUNCH THE EDITOR WITH -unattended. Without it the editor blocks inside
// LoadDefaultMapAtStartup (Map_Check on a World Partition map raises a handled
// ensure and waits for a UI headless mode never presents), so the game thread
// never reaches its tick loop, ProcessPendingAutomationRequests never drains, and
// every queued execute hangs until the client gives up.
//   UnrealEditor-Cmd <project> -nosplash -NullRHI -NoSound -unattended -stdout
//
// Run: node scripts/qa/task43-preview-compensation-probe.cjs --out <file>

const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');

const NATIVE_PORT = Number(process.env.MCP_QA_NATIVE_PORT || 3000);
const TOKEN = process.env.MCP_QA_TOKEN || 'mcp-test-loopback-token';
const CONTENT_DIR = process.env.MCP_QA_CONTENT_DIR || '/data/Game/MCPtest/Content';

const PREVIEW_FOLDER = '/Game/QA43Preview';
const NAME_DELETE_TARGET = 'M_QA43DeleteTarget';   // exists; a previewed delete must NOT remove it
const NAME_PREVIEW_CREATE = 'M_QA43PreviewCreate'; // previewed create must NOT bring it into being
const ASSET_DELETE_TARGET = `${PREVIEW_FOLDER}/${NAME_DELETE_TARGET}`;
const ASSET_PREVIEW_CREATE = `${PREVIEW_FOLDER}/${NAME_PREVIEW_CREATE}`;

const SAVE_OK_FOLDER = '/Game/QA43SaveOk';         // writable at save time -> save lands
const SAVE_BLOCKED_FOLDER = '/Game/QA43SaveBlocked'; // read-only at save time -> save cannot land
const NAME_SAVE_OK = 'M_QA43SaveOk';
const NAME_SAVE_BLOCKED = 'M_QA43SaveBlocked';
const ASSET_SAVE_OK = `${SAVE_OK_FOLDER}/${NAME_SAVE_OK}`;
const ASSET_SAVE_BLOCKED = `${SAVE_BLOCKED_FOLDER}/${NAME_SAVE_BLOCKED}`;

// Every editor call is queued to the game thread, so the timeout has to tolerate
// a busy editor; save_all over a cold project is the slowest call here. A queue
// stall must surface as a real failure, not as a client-side timeout.
const EXEC_TIMEOUT_MS = 180000;
const SESSION_TIMEOUT_MS = 30000;
const READY_DEADLINE_MS = 120000;

const outArg = process.argv.indexOf('--out');
const OUT = outArg >= 0 ? process.argv[outArg + 1] : null;
const results = [];
const meta = {
  probe: 'task43-preview-compensation-probe',
  nativePort: NATIVE_PORT,
  execTimeoutMs: EXEC_TIMEOUT_MS,
  contentDir: CONTENT_DIR,
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
    params: { protocolVersion: '2025-06-18', capabilities: {}, clientInfo: { name: 'task43-preview-compensation-probe', version: '1.0.0' } }
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

// A streamed execute answers as SSE, a refusal as plain JSON. The machine-readable
// payload is `result.structuredContent`; content[0].text carries the human string,
// which on the SUCCESS path also embeds the same JSON but on an ERROR path is only
// "Error [CODE]: message" with no JSON at all. Reading structuredContent first is
// what makes error payloads (refusal guidance, compensation blocks) readable -
// parsing only content[0].text silently returns null for every failure response
// and would make an oracle that depends on it permanently inconclusive.
const raw = (res) => String(res.body || '');
function payload(res) {
  const body = raw(res);
  const frames = body.split('\n')
    .filter((line) => line.startsWith('data: '))
    .map((line) => line.slice('data: '.length).trim());
  for (const frame of (frames.length ? frames : [body])) {
    let env = null;
    try { env = JSON.parse(frame); } catch { continue; }
    const structured = env?.result?.structuredContent;
    if (structured && typeof structured === 'object') return structured;
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

/** First value under `key` anywhere in the parsed payload, wherever the envelope puts it. */
function findDeep(node, key, depth = 0) {
  if (!node || typeof node !== 'object' || depth > 8) return null;
  if (!Array.isArray(node) && Object.prototype.hasOwnProperty.call(node, key)) return node[key];
  for (const value of Array.isArray(node) ? node : Object.values(node)) {
    const hit = findDeep(value, key, depth + 1);
    if (hit) return hit;
  }
  return null;
}

/** Where in the envelope that value actually sits - recorded so the evidence names the real location. */
function findDeepPath(node, key, prefix = '', depth = 0) {
  if (!node || typeof node !== 'object' || depth > 8) return null;
  if (!Array.isArray(node) && Object.prototype.hasOwnProperty.call(node, key)) return `${prefix}/${key}`;
  for (const [k, value] of Object.entries(node)) {
    const hit = findDeepPath(value, key, `${prefix}/${k}`, depth + 1);
    if (hit) return hit;
  }
  return null;
}

// INDEPENDENT ORACLE. One reading of a separate asset.list read: true/false when
// the listing came back and its assets array could be searched, null when the read
// is unusable - so an unusable oracle is reported inconclusive rather than
// silently passing. NOTE: the read action is manage_asset 'list' and its `path`
// is required; 'list_assets' does not exist and answers UNKNOWN_ACTION, which
// would make this oracle permanently inconclusive.
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
// every chance to surface a late creation. Both directions get the same window,
// and the verdict is the last conclusive reading.
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

// The consent envelope and declared effect each capability actually carries, read
// from the gateway's own describe rather than hardcoded, so a contract change
// surfaces here instead of masquerading as a policy refusal.
async function discoverCapability(sid, tool, action, fallback) {
  const res = await gateway(sid, { operation: 'describe', tool, action }, SESSION_TIMEOUT_MS);
  const env = payload(res);
  const grant = env?.consentGrant;
  const ok = grant && typeof grant.capability === 'string' && typeof grant.acknowledge === 'string';
  return {
    grant: ok ? grant : fallback,
    discovered: Boolean(ok),
    capability: env?.capability ?? null,
    effect: env?.effect ?? findDeep(env, 'effect')
  };
}

/** Disk path of a /Game package, i.e. where the receipt claims a save landed. */
const diskPathOf = (gamePath) => path.join(CONTENT_DIR, `${gamePath.replace(/^\/Game\//, '')}.uasset`);
const onDisk = (gamePath) => fs.existsSync(diskPathOf(gamePath));

async function main() {
  const sid = await init();
  if (!sid) { record('INIT', 'a session id', 'none - is the editor + native MCP up?', false); return; }

  // Readiness gate AND warm-up. The first queued call absorbs the asset-registry
  // scan; until one succeeds the editor cannot answer anything, and a timeout then
  // would say nothing about preview or compensation.
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
  meta.warmUpMs = warm.ms;

  const createCap = await discoverCapability(sid, 'manage_asset', 'create_material', { capability: 'material.create_material', acknowledge: 'explicit' });
  const deleteCap = await discoverCapability(sid, 'manage_asset', 'delete_asset', { capability: 'asset.delete_asset', acknowledge: 'elevated' });
  const saveCap = await discoverCapability(sid, 'control_editor', 'save_all', { capability: 'control_editor.save_all', acknowledge: 'explicit' });
  const CREATE_GRANT = createCap.grant;
  const DELETE_GRANT = deleteCap.grant;
  const SAVE_GRANT = saveCap.grant;
  meta.discovered = {
    readAction: "manage_asset.list (asset.list) - list_assets does not exist (UNKNOWN_ACTION)",
    create: createCap, delete: deleteCap, saveAll: saveCap
  };

  // ---------------------------------------------------------------- probe (a)
  await exec(sid, 'manage_asset', 'delete_asset', { paths: [ASSET_DELETE_TARGET, ASSET_PREVIEW_CREATE] }, DELETE_GRANT, null); // pre-clean

  // The destructive preview needs something real to destroy, so establish the
  // target first and confirm it exists with the same independent read. Without
  // this, "still present" afterwards would prove nothing.
  const seed = await exec(sid, 'manage_asset', 'create_material',
    { name: NAME_DELETE_TARGET, path: PREVIEW_FOLDER, save: true }, CREATE_GRANT, null);
  const seeded = await assetExists(sid, PREVIEW_FOLDER, NAME_DELETE_TARGET);
  record('PV-0 delete target really exists before the previewed delete (precondition)',
    'asset present', seeded.verdict === null ? 'ORACLE_INCONCLUSIVE' : (seeded.verdict ? 'asset present' : 'asset absent'),
    seeded.verdict === true, { createStatus: status(seed), oracle: seeded.readings });

  // PV-1: preview:true on a DESTRUCTIVE capability is refused, not faked.
  const previewDelete = await exec(sid, 'manage_asset', 'delete_asset',
    { paths: [ASSET_DELETE_TARGET] }, DELETE_GRANT, { preview: true });
  record('PV-1 preview:true on a DESTRUCTIVE capability is refused UNSUPPORTED_PREVIEW',
    'UNSUPPORTED_PREVIEW', errorCode(previewDelete) || status(previewDelete),
    errorCode(previewDelete) === 'UNSUPPORTED_PREVIEW',
    { capability: deleteCap.capability, declaredEffect: deleteCap.effect, refusalMessage: message(previewDelete), ms: previewDelete.ms });

  // PV-2: and it destroyed nothing, proven independently.
  const survived = await assetExists(sid, PREVIEW_FOLDER, NAME_DELETE_TARGET);
  record('PV-2 refused destructive preview deleted NOTHING (independent list oracle)',
    'asset still present', survived.verdict === null ? 'ORACLE_INCONCLUSIVE' : (survived.verdict ? 'asset still present' : 'asset GONE - preview destroyed it'),
    survived.verdict === true, { oracle: survived.readings });

  // PV-3/PV-4: the other direction - a previewed create must not bring an asset
  // into being. This is the oracle that reads "absent".
  const previewCreate = await exec(sid, 'manage_asset', 'create_material',
    { name: NAME_PREVIEW_CREATE, path: PREVIEW_FOLDER, save: true }, CREATE_GRANT, { preview: true });
  record('PV-3 preview:true on a create is refused UNSUPPORTED_PREVIEW',
    'UNSUPPORTED_PREVIEW', errorCode(previewCreate) || status(previewCreate),
    errorCode(previewCreate) === 'UNSUPPORTED_PREVIEW', { refusalMessage: message(previewCreate), ms: previewCreate.ms });

  const createdByPreview = await assetExists(sid, PREVIEW_FOLDER, NAME_PREVIEW_CREATE);
  record('PV-4 refused preview created NO asset (independent list oracle)',
    'asset absent', createdByPreview.verdict === null ? 'ORACLE_INCONCLUSIVE' : (createdByPreview.verdict ? 'asset present - preview mutated' : 'asset absent'),
    createdByPreview.verdict === false, { oracle: createdByPreview.readings });

  // PV-5: the refusal carries executable recovery guidance - the caller's own call
  // minus the option that cannot be honored.
  const guidance = payload(previewCreate);
  const nextCall = findDeep(guidance, 'nextCall');
  const nextCallOk = Boolean(nextCall) && nextCall.operation === 'execute' &&
    nextCall.tool === 'manage_asset' && String(nextCall.action || '').includes('create_material') &&
    !(nextCall.options && Object.prototype.hasOwnProperty.call(nextCall.options, 'preview'));
  record('PV-5 refusal returns an executable nextCall with options.preview removed',
    'execute/manage_asset/create_material without options.preview',
    nextCall ? JSON.stringify(nextCall).slice(0, 220) : 'no nextCall in refusal',
    nextCallOk, { suggestions: findDeep(guidance, 'suggestions') });

  // PV-6: narrowness control. The same create with preview:false really executes,
  // so PV-3 was refused by the preview gate and not by a broken capability.
  const realCreate = await exec(sid, 'manage_asset', 'create_material',
    { name: NAME_PREVIEW_CREATE, path: PREVIEW_FOLDER, save: true }, CREATE_GRANT, { preview: false });
  const reallyCreated = await assetExists(sid, PREVIEW_FOLDER, NAME_PREVIEW_CREATE);
  record('PV-6 same call with preview:false executes for real (refusal is narrow)',
    'success + asset present',
    `${status(realCreate)} + ${reallyCreated.verdict === null ? 'ORACLE_INCONCLUSIVE' : (reallyCreated.verdict ? 'asset present' : 'asset absent')}`,
    status(realCreate) === 'success' && reallyCreated.verdict === true, { oracle: reallyCreated.readings, ms: realCreate.ms });

  // PV-7: and the destructive capability itself works unpreviewed, so PV-1's
  // refusal was the preview gate rather than a delete that cannot delete.
  const realDelete = await exec(sid, 'manage_asset', 'delete_asset',
    { paths: [ASSET_DELETE_TARGET] }, DELETE_GRANT, null);
  const deleted = await assetExists(sid, PREVIEW_FOLDER, NAME_DELETE_TARGET);
  record('PV-7 same delete without preview really deletes (control)',
    'success + asset absent',
    `${status(realDelete)} + ${deleted.verdict === null ? 'ORACLE_INCONCLUSIVE' : (deleted.verdict ? 'asset present' : 'asset absent')}`,
    status(realDelete) === 'success' && deleted.verdict === false, { oracle: deleted.readings });

  // ---------------------------------------------------------------- probe (b)
  // Flush whatever the editor already had dirty, so the partial state below is
  // attributable to the two packages this probe creates.
  const flush = await exec(sid, 'control_editor', 'save_all', {}, SAVE_GRANT, null);
  meta.baselineFlush = { status: status(flush), state: findDeep(payload(flush), 'compensation')?.state ?? null, ms: flush.ms };

  await exec(sid, 'manage_asset', 'delete_asset', { paths: [ASSET_SAVE_OK, ASSET_SAVE_BLOCKED] }, DELETE_GRANT, null); // pre-clean

  // Two dirty, unsaved packages. save:false marks the package dirty and skips the
  // save, so save_all is what will try to write them.
  const dirtyOk = await exec(sid, 'manage_asset', 'create_material',
    { name: NAME_SAVE_OK, path: SAVE_OK_FOLDER, save: false }, CREATE_GRANT, null);
  const dirtyBlocked = await exec(sid, 'manage_asset', 'create_material',
    { name: NAME_SAVE_BLOCKED, path: SAVE_BLOCKED_FOLDER, save: false }, CREATE_GRANT, null);

  const blockedDir = path.join(CONTENT_DIR, SAVE_BLOCKED_FOLDER.replace(/^\/Game\//, ''));
  const okDir = path.join(CONTENT_DIR, SAVE_OK_FOLDER.replace(/^\/Game\//, ''));
  let chmodApplied = false;
  try {
    fs.mkdirSync(blockedDir, { recursive: true });
    fs.mkdirSync(okDir, { recursive: true });
    // Read-only DIRECTORY: creating an entry inside needs write permission on the
    // directory, so this package physically cannot be written while the other can.
    fs.chmodSync(blockedDir, 0o555);
    chmodApplied = true;
  } catch (e) { meta.chmodError = String(e); }

  meta.partialSetup = {
    dirtyOkStatus: status(dirtyOk), dirtyBlockedStatus: status(dirtyBlocked),
    blockedDir, blockedDirMadeReadOnly: chmodApplied,
    okOnDiskBefore: onDisk(ASSET_SAVE_OK), blockedOnDiskBefore: onDisk(ASSET_SAVE_BLOCKED)
  };
  record('CP-0 two dirty packages staged, neither yet on disk (precondition)',
    'both creates succeed, neither file exists, blocked dir read-only',
    `${status(dirtyOk)}/${status(dirtyBlocked)}, onDisk ok=${meta.partialSetup.okOnDiskBefore} blocked=${meta.partialSetup.blockedOnDiskBefore}, chmod=${chmodApplied}`,
    status(dirtyOk) === 'success' && status(dirtyBlocked) === 'success' &&
    !meta.partialSetup.okOnDiskBefore && !meta.partialSetup.blockedOnDiskBefore && chmodApplied);

  const partial = await exec(sid, 'control_editor', 'save_all', {}, SAVE_GRANT, null);
  const partialEnv = payload(partial);
  const comp = findDeep(partialEnv, 'compensation');
  const stepIds = (list) => (Array.isArray(list) ? list.map((s) => String(s?.step ?? s)) : []);
  const completedSteps = stepIds(comp?.completed);
  const notCompletedSteps = stepIds(comp?.notCompleted);
  const okStep = `save:${ASSET_SAVE_OK}`;
  const blockedStep = `save:${ASSET_SAVE_BLOCKED}`;
  meta.partialReceipt = comp || null;
  meta.partialStatus = status(partial);
  // Named explicitly because it is not top-level: a client reading only `data`
  // never sees it on this path.
  meta.partialCompensationLocation = findDeepPath(partialEnv, 'compensation');

  record('CP-1 non-atomic multi-package save reports state "partial"', 'partial',
    comp ? String(comp.state) : 'no compensation block in the receipt',
    comp?.state === 'partial', { saveAllStatus: status(partial), ms: partial.ms });

  record('CP-2 receipt names the package that landed AND the one that did not',
    `completed contains ${okStep}; notCompleted contains ${blockedStep}`,
    `completed=[${completedSteps.join(', ')}] notCompleted=[${notCompletedSteps.join(', ')}]`,
    completedSteps.includes(okStep) && notCompletedSteps.includes(blockedStep));

  // The honesty oracle: the filesystem, not the receipt. A receipt claiming a save
  // landed when nothing is on disk (or claiming one failed when it did land) dies
  // here.
  const okReallyOnDisk = onDisk(ASSET_SAVE_OK);
  const blockedReallyOnDisk = onDisk(ASSET_SAVE_BLOCKED);
  record('CP-3 the receipt matches the DISK: completed step is durable, not-completed step wrote nothing',
    'completed file exists, notCompleted file absent',
    `${diskPathOf(ASSET_SAVE_OK)} exists=${okReallyOnDisk}; ${diskPathOf(ASSET_SAVE_BLOCKED)} exists=${blockedReallyOnDisk}`,
    okReallyOnDisk === true && blockedReallyOnDisk === false);

  record('CP-4 no rollback is promised for durable work', 'atomic:false and rollback:"unavailable"',
    comp ? `atomic=${comp.atomic} rollback=${JSON.stringify(comp.rollback)}` : 'no compensation block',
    comp?.atomic === false && comp?.rollback === 'unavailable', { rollbackReason: comp?.rollbackReason });

  const callerAction = String(comp?.callerAction || '');
  record('CP-5 partial receipt tells the caller exactly how to finish',
    'non-empty callerAction naming the remaining work + a compensating capability',
    callerAction ? `callerAction(${callerAction.length} chars), compensating=[${(comp?.compensatingCapabilities || []).join(', ')}]` : 'empty callerAction',
    callerAction.length > 0 && Array.isArray(comp?.compensatingCapabilities) && comp.compensatingCapabilities.length > 0,
    { callerAction, compensatingCapabilities: comp?.compensatingCapabilities });

  // Cleanup. Restore the directory first: while it is read-only nothing inside it
  // can be written or removed.
  try { if (chmodApplied) fs.chmodSync(blockedDir, 0o755); } catch (e) { meta.chmodRestoreError = String(e); }
  const finalSave = await exec(sid, 'control_editor', 'save_all', {}, SAVE_GRANT, null);
  const del = await exec(sid, 'manage_asset', 'delete_asset',
    { paths: [ASSET_DELETE_TARGET, ASSET_PREVIEW_CREATE, ASSET_SAVE_OK, ASSET_SAVE_BLOCKED] }, DELETE_GRANT, null);
  const leftovers = {};
  for (const [folder, name] of [[PREVIEW_FOLDER, NAME_DELETE_TARGET], [PREVIEW_FOLDER, NAME_PREVIEW_CREATE],
    [SAVE_OK_FOLDER, NAME_SAVE_OK], [SAVE_BLOCKED_FOLDER, NAME_SAVE_BLOCKED]]) {
    leftovers[`${folder}/${name}`] = (await readAsset(sid, folder, name)).verdict;
  }
  for (const dir of [okDir, blockedDir, path.join(CONTENT_DIR, PREVIEW_FOLDER.replace(/^\/Game\//, ''))]) {
    try { fs.rmSync(dir, { recursive: true, force: true }); } catch (e) { meta.rmErrors = [...(meta.rmErrors || []), String(e)]; }
  }
  meta.cleanup = {
    finalSaveStatus: status(finalSave), deleteStatus: status(del), deleteMessage: message(del),
    assetsRemaining: leftovers,
    verifiedClean: Object.values(leftovers).every((v) => v === false),
    diskRemaining: { ok: onDisk(ASSET_SAVE_OK), blocked: onDisk(ASSET_SAVE_BLOCKED) }
  };
  await request('DELETE', null, { 'Mcp-Session-Id': sid, 'X-MCP-Capability-Token': TOKEN }, 5000);
}

main().then(() => {
  const passed = results.filter((r) => r.pass).length;
  for (const r of results) console.log(`${r.pass ? 'PASS' : 'FAIL'}  ${r.name}  [expected ${r.expected} | actual ${r.actual}]`);
  console.log(`\n${passed}/${results.length} passed`);
  if (meta.cleanup) console.log(`cleanup verified clean: ${meta.cleanup.verifiedClean}`);
  if (OUT) fs.writeFileSync(OUT, JSON.stringify({ ...meta, finishedAt: new Date().toISOString(), passed, total: results.length, results }, null, 2));
  process.exit(passed === results.length && results.length > 0 ? 0 : 1);
}).catch((e) => { console.error(e); process.exit(2); });
