#!/usr/bin/env node
// scripts/qa/task-37-stdio-qa.mjs
// Task 37 runtime QA harness — stdio / in-memory MCP primitive matrix.
//
// AUTHORED BEFORE Task 37 IS IMPLEMENTED. It drives the built `dist/` server over
// an in-memory transport (mock Unreal connection) through the FULL primitive
// matrix Task 37 wires: resources list/read/templates, subscribe/updated/
// unsubscribe, prompts list/get, completion match + secret refusal, tasks/list
// method-not-found, tools/list single-gateway, plus adversarial modes. It NEVER
// needs a live Unreal editor.
//
// Modes:
//   --help / -h     usage, exit 0
//   --self-check    NO-OP readiness: print the matrix + emit the plan JSON, run
//                   NO feature assertion, exit 0. This is the default with no args.
//   --run           execute the REQUIRED matrix (feature assertions).
//   --adversarial   execute ONLY the adversarial matrix.
//   --all           required + adversarial.
//   --out=<path>    override JSON output path.
//   --json-only     print only the JSON document to stdout.
//
// Until Task 37 lands, run ONLY --help / --self-check (both exit 0). `--run` is
// expected to fail today because the SDK client asserts the server capabilities
// (resources.subscribe, prompts, completions) Task 37 adds — that failure is the
// point and is NOT part of this pre-implementation scaffold.
//
// SIZE_OK: single responsibility — the Task 37 primitive matrix runner. It is an
// indivisible ordered narrative + its adversarial siblings; splitting it across
// files would hide the flow it exists to document.

import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import { ResourceUpdatedNotificationSchema } from '@modelcontextprotocol/sdk/types.js';
import { existsSync, mkdirSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { z } from 'zod';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const REPO_ROOT = path.resolve(HERE, '../..');
const SERVER_MODULE = path.join(REPO_ROOT, 'dist/index.js');
const EVIDENCE_DIR = path.join(REPO_ROOT, '.omo/evidence/task-37');

// --- Anchored, source-verified constants (see qa-plan.md §1) ---
const CATALOG_URI = 'ue://capability/catalog';        // CATALOG_SUBSCRIPTION_URI
const OFF_ALLOWLIST_URI = 'ue://editor';              // real, readable, NOT subscribable
const COALESCE_WINDOW_MS = 50;                        // DEFAULT_COALESCE_WINDOW_MS
const WINDOW_GRACE_MS = 300;                          // slack over the coalesce window
const CALL_TIMEOUT = 15000;
const VISIBILITY_TOOL = 'manage_ai';                  // non-protected gameplay tool
const BURST_TOOLS = ['manage_ai', 'manage_combat', 'manage_inventory', 'manage_interaction'];
const PROTOCOL_VERSION = '2025-11-25';
const PROMPT_ID = 'asset-import';
const PROMPT_ARGS = { destinationPath: '/Game/Imported/Rock' };

const REQUIRED_MATRIX = [
  ['initialize-full-caps', 'connect and read server capabilities', 'server capabilities equal {tools:{}, resources:{subscribe:true}, prompts:{}, completions:{}} with no tasks/logging/listChanged/extra resource keys'],
  ['resources-list', 'resources/list', 'includes ue://capability/catalog + Task 31 resources'],
  ['resources-read-catalog', 'resources/read ue://capability/catalog', 'one application/json content that parses and carries a revision'],
  ['resources-templates-list', 'resources/templates/list', 'includes the four ue:// templates'],
  ['resources-subscribe-catalog', 'resources/subscribe ue://capability/catalog', 'success, no throw'],
  ['configure-visibility-revision', 'unreal configure disable_tools [manage_ai]', 'success; catalog revision advances'],
  ['resources-updated-single-uri-only', 'await coalesce window', 'exactly one resources/updated, wire params keys === ["uri"], uri === catalog'],
  ['resources-unsubscribe', 'resources/unsubscribe ue://capability/catalog', 'success'],
  ['resources-updated-silence-after-unsubscribe', '2nd configure change + await window', 'zero further resources/updated'],
  ['prompts-list', 'prompts/list', 'six workflow prompts incl. asset-import, sequence-render'],
  ['prompts-get', 'prompts/get asset-import', 'bounded messages[], no secret interpolation'],
  ['completion-match', 'completion/complete asset-import sourceFormat="f"', 'non-empty completion.values'],
  ['completion-secret-refusal', 'completion/complete apiKey=""', 'empty completion.values (refused)'],
  ['tasks-list-method-not-found', 'raw request tasks/list', 'JSON-RPC error -32601'],
  ['tools-list-single-unreal-four-ops', 'tools/list', 'one tool unreal; operation enum === {search,describe,execute,configure}'],
  ['disconnect-cleanup', 'close transport', 'clean teardown; no post-close notification; no unhandled rejection'],
];

const ADVERSARIAL_MATRIX = [
  ['adversarial-malformed-caps', 'raw initialize capabilities:"bogus"', 'JSON-RPC error (internal error -32603); server does not crash'],
  ['adversarial-off-allowlist-subscribe', 'resources/subscribe ue://editor', 'JSON-RPC error (invalid params -32602); not-subscribable rejected'],
  ['adversarial-burst-coalescing', '4 distinct visibility mutations inside the window', 'exactly one coalesced resources/updated'],
  ['adversarial-disconnect-mid-window', 'mutate then close before the window elapses', 'no delivered notification; no crash'],
];

const LIMITATION_NOTES = [
  'Mock Unreal connection over in-memory transport: proves protocol WIRING only, not live-editor side effects.',
  'Some cleanup / timer-suppression guarantees are not observable over a closed transport; those are additionally covered by Task 34 unit tests (notification-coalescer.test.ts, subscription-store.test.ts).',
  'resources/updated is asserted URI-only on the WIRE (keys === ["uri"]); a leaked revision/changeKind fails the check.',
];

const rejections = [];

// --- small helpers ---
const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
const errMsg = (e) => (e instanceof Error ? e.message : String(e));

function mkResult(id, status, expected, observed, detail) {
  return { id, status, expected, observed: observed ?? null, detail: detail ?? null };
}

function structured(res) {
  if (res && typeof res === 'object' && res.structuredContent && typeof res.structuredContent === 'object') {
    return res.structuredContent;
  }
  const content = res?.content;
  if (Array.isArray(content) && content[0]?.type === 'text' && typeof content[0].text === 'string') {
    try {
      return JSON.parse(content[0].text);
    } catch {
      return null;
    }
  }
  return null;
}

function summarize(results) {
  const summary = { total: results.length, pass: 0, fail: 0, error: 0, planned: 0, blocked: 0, skip: 0 };
  for (const r of results) {
    if (r.status in summary) summary[r.status] += 1;
  }
  return summary;
}

function overallOf(results) {
  if (results.length === 0) return 'NOT-RUN';
  return results.some((r) => r.status === 'fail' || r.status === 'error' || r.status === 'blocked') ? 'FAIL' : 'PASS';
}

function baseDoc(mode) {
  return {
    task: 37,
    harness: 'task-37-stdio-qa',
    mode,
    startedAt: new Date().toISOString(),
    finishedAt: null,
    node: process.version,
    serverModule: SERVER_MODULE,
    distPresent: existsSync(SERVER_MODULE),
    catalogUri: CATALOG_URI,
    coalesceWindowMs: COALESCE_WINDOW_MS,
    protocolVersion: PROTOCOL_VERSION,
    required: [],
    adversarial: [],
    summary: null,
    overall: null,
    notes: LIMITATION_NOTES,
  };
}

function outPathFor(opts, doc) {
  if (opts.out) return path.resolve(process.cwd(), opts.out);
  const name = doc.mode === 'self-check' ? 'stdio-qa-selfcheck.json' : 'stdio-qa-result.json';
  return path.join(EVIDENCE_DIR, name);
}

function finish(doc, opts) {
  doc.finishedAt = new Date().toISOString();
  const json = JSON.stringify(doc, null, 2);
  const dest = outPathFor(opts, doc);
  try {
    mkdirSync(path.dirname(dest), { recursive: true });
    writeFileSync(dest, `${json}\n`);
    doc.writtenTo = dest;
  } catch (e) {
    process.stderr.write(`[task-37-stdio-qa] could not write ${dest}: ${errMsg(e)}\n`);
  }
  process.stdout.write(`${json}\n`);
  if (!opts.jsonOnly) {
    const s = doc.summary ?? { total: 0 };
    process.stderr.write(
      `[task-37-stdio-qa] mode=${doc.mode} overall=${doc.overall} `
      + `total=${s.total} pass=${s.pass ?? 0} fail=${s.fail ?? 0} error=${s.error ?? 0} `
      + `planned=${s.planned ?? 0} blocked=${s.blocked ?? 0} -> ${dest}\n`,
    );
  }
}

// --- connection lifecycle ---
async function makeConnected() {
  process.env.MOCK_UNREAL_CONNECTION = 'true';
  process.env.NODE_ENV = 'test';
  const mod = await import(pathToFileURL(SERVER_MODULE).href);
  const parts = mod.createServer();
  const { server, bridge, automationBridge, metricsServer } = parts;
  const [clientTransport, serverTransport] = InMemoryTransport.createLinkedPair();
  const client = new Client({ name: 'task-37-stdio-qa', version: '0.0.0' }, { capabilities: {} });

  const rawNotifications = [];
  const typedNotifications = [];
  await server.connect(serverTransport);
  await client.connect(clientTransport, { timeout: CALL_TIMEOUT });

  // Tap the raw wire so URI-only + count assertions see the exact params the
  // server put on the wire (independent of any schema stripping in the SDK).
  const inner = clientTransport.onmessage;
  clientTransport.onmessage = (message, extra) => {
    if (message && typeof message === 'object' && message.method === 'notifications/resources/updated') {
      rawNotifications.push({ ts: Date.now(), params: message.params ?? {} });
    }
    if (typeof inner === 'function') inner(message, extra);
  };
  client.setNotificationHandler(ResourceUpdatedNotificationSchema, (n) => {
    typedNotifications.push(n.params);
  });

  return { server, bridge, automationBridge, metricsServer, client, clientTransport, rawNotifications, typedNotifications };
}

async function teardown(ctx) {
  const errs = [];
  const guard = async (label, fn) => {
    try {
      await fn();
    } catch (e) {
      errs.push(`${label}: ${errMsg(e)}`);
    }
  };
  if (ctx?.client) await guard('client.close', () => ctx.client.close());
  if (ctx?.automationBridge) await guard('automationBridge.stop', () => ctx.automationBridge.stop());
  if (ctx?.bridge) await guard('bridge.dispose', () => ctx.bridge.dispose());
  if (ctx?.metricsServer) await guard('metricsServer.close', () => ctx.metricsServer.close());
  return errs;
}

async function configureVisibility(client, action, tools) {
  const res = await client.callTool(
    { name: 'unreal', arguments: { operation: 'configure', action, params: { tools } } },
    undefined,
    { timeout: CALL_TIMEOUT },
  );
  return structured(res);
}

async function step(results, id, expected, fn) {
  try {
    const outcome = await fn();
    results.push(mkResult(id, outcome.status, expected, outcome.observed, outcome.detail));
    return outcome.status;
  } catch (e) {
    results.push(mkResult(id, 'error', expected, { message: errMsg(e), code: e?.code ?? null }, 'threw during execution'));
    return 'error';
  }
}

// --- required matrix (ordered, single stdio session) ---
async function runRequired() {
  const results = [];
  const ctx = await makeConnected();
  const expectedOf = (id) => REQUIRED_MATRIX.find((m) => m[0] === id)?.[2] ?? '';
  try {
    await step(results, 'initialize-full-caps', expectedOf('initialize-full-caps'), async () => {
      const caps = ctx.client.getServerCapabilities() ?? {};
      const onlyKeys = (o, keys) => o !== null && typeof o === 'object'
        && Object.keys(o).length === keys.length && keys.every((k) => k in o);
      const ok = onlyKeys(caps, ['tools', 'resources', 'prompts', 'completions'])
        && onlyKeys(caps.tools, []) && onlyKeys(caps.prompts, []) && onlyKeys(caps.completions, [])
        && onlyKeys(caps.resources, ['subscribe']) && caps.resources?.subscribe === true;
      return { status: ok ? 'pass' : 'fail', observed: caps, detail: 'server capability advertisement' };
    });

    await step(results, 'resources-list', expectedOf('resources-list'), async () => {
      const r = await ctx.client.listResources(undefined, { timeout: CALL_TIMEOUT });
      const uris = (r.resources ?? []).map((x) => x.uri);
      return { status: uris.includes(CATALOG_URI) ? 'pass' : 'fail', observed: { count: uris.length, hasCatalog: uris.includes(CATALOG_URI) }, detail: null };
    });

    await step(results, 'resources-read-catalog', expectedOf('resources-read-catalog'), async () => {
      const r = await ctx.client.readResource({ uri: CATALOG_URI }, { timeout: CALL_TIMEOUT });
      const first = r.contents?.[0];
      let parsed = null;
      try {
        parsed = first?.text ? JSON.parse(first.text) : null;
      } catch { parsed = null; }
      const ok = first?.mimeType === 'application/json' && parsed !== null && parsed.revision !== undefined;
      return { status: ok ? 'pass' : 'fail', observed: { mimeType: first?.mimeType, revision: parsed?.revision }, detail: null };
    });

    await step(results, 'resources-templates-list', expectedOf('resources-templates-list'), async () => {
      const r = await ctx.client.listResourceTemplates(undefined, { timeout: CALL_TIMEOUT });
      const templates = (r.resourceTemplates ?? []).map((x) => x.uriTemplate);
      const need = ['ue://capability/{capabilityId}', 'ue://knowledge/{engineVersion}/{topic}', 'ue://object/{objectPath}', 'ue://asset/{assetPath}'];
      const missing = need.filter((t) => !templates.includes(t));
      return { status: missing.length === 0 ? 'pass' : 'fail', observed: { templates, missing }, detail: null };
    });

    await step(results, 'resources-subscribe-catalog', expectedOf('resources-subscribe-catalog'), async () => {
      await ctx.client.subscribeResource({ uri: CATALOG_URI }, { timeout: CALL_TIMEOUT });
      return { status: 'pass', observed: 'subscribed', detail: null };
    });

    await step(results, 'configure-visibility-revision', expectedOf('configure-visibility-revision'), async () => {
      const sc = await configureVisibility(ctx.client, 'disable_tools', [VISIBILITY_TOOL]);
      return { status: sc?.success === true ? 'pass' : 'fail', observed: sc, detail: `disable_tools [${VISIBILITY_TOOL}]` };
    });

    await step(results, 'resources-updated-single-uri-only', expectedOf('resources-updated-single-uri-only'), async () => {
      await sleep(COALESCE_WINDOW_MS + WINDOW_GRACE_MS);
      const catalog = ctx.rawNotifications.filter((n) => n.params?.uri === CATALOG_URI);
      const keys = catalog.length === 1 ? Object.keys(catalog[0].params) : [];
      const uriOnly = keys.length === 1 && keys[0] === 'uri';
      const ok = catalog.length === 1 && uriOnly;
      return { status: ok ? 'pass' : 'fail', observed: { count: catalog.length, keys }, detail: 'exactly one, URI-only' };
    });

    await step(results, 'resources-unsubscribe', expectedOf('resources-unsubscribe'), async () => {
      await ctx.client.unsubscribeResource({ uri: CATALOG_URI }, { timeout: CALL_TIMEOUT });
      return { status: 'pass', observed: 'unsubscribed', detail: null };
    });

    await step(results, 'resources-updated-silence-after-unsubscribe', expectedOf('resources-updated-silence-after-unsubscribe'), async () => {
      const mark = ctx.rawNotifications.length;
      await configureVisibility(ctx.client, 'enable_tools', [VISIBILITY_TOOL]);
      await sleep(COALESCE_WINDOW_MS + WINDOW_GRACE_MS);
      const fresh = ctx.rawNotifications.slice(mark).filter((n) => n.params?.uri === CATALOG_URI);
      return { status: fresh.length === 0 ? 'pass' : 'fail', observed: { newCatalogNotifications: fresh.length }, detail: 'silence after unsubscribe' };
    });

    await step(results, 'prompts-list', expectedOf('prompts-list'), async () => {
      const r = await ctx.client.listPrompts(undefined, { timeout: CALL_TIMEOUT });
      const names = (r.prompts ?? []).map((p) => p.name);
      const ok = names.includes('asset-import') && names.includes('sequence-render');
      return { status: ok ? 'pass' : 'fail', observed: { count: names.length, names }, detail: null };
    });

    await step(results, 'prompts-get', expectedOf('prompts-get'), async () => {
      const r = await ctx.client.getPrompt({ name: PROMPT_ID, arguments: PROMPT_ARGS }, { timeout: CALL_TIMEOUT });
      const text = r.messages?.[0]?.content?.text ?? '';
      const ok = Array.isArray(r.messages) && r.messages.length >= 1 && typeof text === 'string' && text.length > 0;
      return { status: ok ? 'pass' : 'fail', observed: { messages: r.messages?.length, bytes: text.length }, detail: null };
    });

    await step(results, 'completion-match', expectedOf('completion-match'), async () => {
      const r = await ctx.client.complete(
        { ref: { type: 'ref/prompt', name: 'asset-import' }, argument: { name: 'sourceFormat', value: 'f' } },
        { timeout: CALL_TIMEOUT },
      );
      const values = r.completion?.values ?? [];
      return { status: values.length > 0 ? 'pass' : 'fail', observed: { values }, detail: 'enum candidates for a known slot' };
    });

    await step(results, 'completion-secret-refusal', expectedOf('completion-secret-refusal'), async () => {
      const r = await ctx.client.complete(
        { ref: { type: 'ref/prompt', name: 'asset-import' }, argument: { name: 'apiKey', value: '' } },
        { timeout: CALL_TIMEOUT },
      );
      const values = r.completion?.values ?? [];
      return { status: values.length === 0 ? 'pass' : 'fail', observed: { values, meta: r.completion?._meta ?? r._meta ?? null }, detail: 'secret-named arg must be refused (empty values)' };
    });

    await step(results, 'tasks-list-method-not-found', expectedOf('tasks-list-method-not-found'), async () => {
      try {
        await ctx.client.request({ method: 'tasks/list', params: {} }, z.object({}).passthrough(), { timeout: CALL_TIMEOUT });
        return { status: 'fail', observed: 'resolved', detail: 'tasks/list unexpectedly succeeded' };
      } catch (e) {
        const code = e?.code ?? null;
        return { status: code === -32601 ? 'pass' : 'fail', observed: { code, message: errMsg(e) }, detail: 'expect -32601 method not found' };
      }
    });

    await step(results, 'tools-list-single-unreal-four-ops', expectedOf('tools-list-single-unreal-four-ops'), async () => {
      const r = await ctx.client.listTools(undefined, { timeout: CALL_TIMEOUT });
      const one = r.tools?.length === 1 && r.tools[0]?.name === 'unreal';
      const enumVals = r.tools?.[0]?.inputSchema?.properties?.operation?.enum ?? [];
      const ops = new Set(enumVals);
      const fourOps = ops.size === 4 && ['search', 'describe', 'execute', 'configure'].every((o) => ops.has(o));
      return { status: one && fourOps ? 'pass' : 'fail', observed: { toolCount: r.tools?.length, name: r.tools?.[0]?.name, operations: enumVals }, detail: null };
    });

    await step(results, 'disconnect-cleanup', expectedOf('disconnect-cleanup'), async () => {
      const before = rejections.length;
      await ctx.client.close();
      await sleep(COALESCE_WINDOW_MS + WINDOW_GRACE_MS);
      const post = ctx.rawNotifications.filter((n) => n.ts > Date.now() - (COALESCE_WINDOW_MS + WINDOW_GRACE_MS));
      const newRejections = rejections.length - before;
      const ok = newRejections === 0 && post.length === 0;
      return { status: ok ? 'pass' : 'fail', observed: { unhandledRejections: newRejections, postCloseNotifications: post.length }, detail: 'clean transport close' };
    });
  } finally {
    await teardown(ctx);
  }
  return results;
}

// --- adversarial matrix (each scenario owns its server) ---
async function advMalformedCaps() {
  const expected = ADVERSARIAL_MATRIX[0][2];
  process.env.MOCK_UNREAL_CONNECTION = 'true';
  process.env.NODE_ENV = 'test';
  const mod = await import(pathToFileURL(SERVER_MODULE).href);
  const { server, bridge, automationBridge, metricsServer } = mod.createServer();
  const [ct, st] = InMemoryTransport.createLinkedPair();
  const responses = [];
  ct.onmessage = (m) => responses.push(m);
  await server.connect(st);
  await ct.start();
  const before = rejections.length;
  try {
    await ct.send({
      jsonrpc: '2.0',
      id: 1,
      method: 'initialize',
      params: { protocolVersion: PROTOCOL_VERSION, capabilities: 'bogus', clientInfo: { name: 'adv', version: '0' } },
    });
    await sleep(150);
    const resp = responses.find((r) => r && r.id === 1);
    const code = resp?.error?.code ?? null;
    const crashed = rejections.length - before > 0;
    const status = code === -32603 && !crashed ? 'pass' : 'fail';
    return mkResult('adversarial-malformed-caps', status, expected, { code, hadResult: Boolean(resp?.result), crashed }, 'raw initialize with non-object capabilities');
  } catch (e) {
    return mkResult('adversarial-malformed-caps', 'error', expected, { message: errMsg(e) }, 'threw');
  } finally {
    try { automationBridge?.stop?.(); } catch { /* ignore */ }
    try { bridge?.dispose?.(); } catch { /* ignore */ }
    try { metricsServer?.close?.(); } catch { /* ignore */ }
  }
}

async function advOffAllowlistSubscribe() {
  const expected = ADVERSARIAL_MATRIX[1][2];
  const ctx = await makeConnected();
  try {
    let info = null;
    try {
      await ctx.client.subscribeResource({ uri: OFF_ALLOWLIST_URI }, { timeout: CALL_TIMEOUT });
    } catch (e) {
      info = { code: e?.code ?? null, message: errMsg(e) };
    }
    return mkResult('adversarial-off-allowlist-subscribe', info?.code === -32602 ? 'pass' : 'fail', expected, info ?? 'resolved (unexpected)', `subscribe ${OFF_ALLOWLIST_URI} must be rejected`);
  } finally {
    await teardown(ctx);
  }
}

async function advBurstCoalescing() {
  const expected = ADVERSARIAL_MATRIX[2][2];
  const ctx = await makeConnected();
  try {
    await ctx.client.subscribeResource({ uri: CATALOG_URI }, { timeout: CALL_TIMEOUT });
    const mark = ctx.rawNotifications.length;
    // Issue distinct visibility mutations concurrently so they land inside one window.
    await Promise.all(BURST_TOOLS.map((t) => configureVisibility(ctx.client, 'disable_tools', [t])));
    await sleep(COALESCE_WINDOW_MS + WINDOW_GRACE_MS);
    const catalog = ctx.rawNotifications.slice(mark).filter((n) => n.params?.uri === CATALOG_URI);
    return mkResult('adversarial-burst-coalescing', catalog.length === 1 ? 'pass' : 'fail', expected, { count: catalog.length, mutations: BURST_TOOLS.length }, 'burst folds to one notification');
  } finally {
    await teardown(ctx);
  }
}

async function advDisconnectMidWindow() {
  const expected = ADVERSARIAL_MATRIX[3][2];
  const ctx = await makeConnected();
  const before = rejections.length;
  try {
    await ctx.client.subscribeResource({ uri: CATALOG_URI }, { timeout: CALL_TIMEOUT });
    const mark = ctx.rawNotifications.length;
    await configureVisibility(ctx.client, 'disable_tools', [VISIBILITY_TOOL]);
    // Close BEFORE the coalesce window elapses.
    await ctx.client.close();
    await sleep(COALESCE_WINDOW_MS + WINDOW_GRACE_MS);
    const delivered = ctx.rawNotifications.slice(mark).filter((n) => n.params?.uri === CATALOG_URI);
    const newRejections = rejections.length - before;
    const ok = delivered.length === 0 && newRejections === 0;
    return mkResult('adversarial-disconnect-mid-window', ok ? 'pass' : 'fail', expected, { delivered: delivered.length, unhandledRejections: newRejections }, 'session clear suppresses the pending flush');
  } finally {
    await teardown(ctx);
  }
}

async function runAdversarial() {
  const results = [];
  results.push(await advMalformedCaps());
  results.push(await advOffAllowlistSubscribe());
  results.push(await advBurstCoalescing());
  results.push(await advDisconnectMidWindow());
  return results;
}

// --- modes ---
async function selfCheck(opts) {
  const doc = baseDoc('self-check');
  let harnessLoadable = true;
  let loadError = null;
  try {
    await import('@modelcontextprotocol/sdk/client/index.js');
    if (doc.distPresent) {
      const mod = await import(pathToFileURL(SERVER_MODULE).href);
      if (typeof mod.createServer !== 'function') {
        harnessLoadable = false;
        loadError = 'dist/index.js exists but has no createServer export';
      }
    }
  } catch (e) {
    harnessLoadable = false;
    loadError = errMsg(e);
  }
  doc.required = REQUIRED_MATRIX.map(([id, title, expected]) => mkResult(id, 'planned', expected, null, title));
  doc.adversarial = ADVERSARIAL_MATRIX.map(([id, title, expected]) => mkResult(id, 'planned', expected, null, title));
  doc.selfCheck = {
    harnessLoadable,
    loadError,
    distPresent: doc.distPresent,
    hint: doc.distPresent ? 'dist/ present — `--run`/`--all` are runnable once Task 37 lands' : 'dist/ missing — run `npm run build` before `--run`',
  };
  doc.summary = summarize([...doc.required, ...doc.adversarial]);
  doc.overall = 'NOT-RUN';
  finish(doc, opts);
  return 0; // no-op readiness ALWAYS succeeds
}

async function runMode(opts, doRequired, doAdversarial) {
  const doc = baseDoc(opts.mode);
  if (!doc.distPresent) {
    if (doRequired) doc.required = REQUIRED_MATRIX.map(([id, , expected]) => mkResult(id, 'blocked', expected, null, 'dist/ missing — run `npm run build`'));
    if (doAdversarial) doc.adversarial = ADVERSARIAL_MATRIX.map(([id, , expected]) => mkResult(id, 'blocked', expected, null, 'dist/ missing — run `npm run build`'));
    doc.summary = summarize([...doc.required, ...doc.adversarial]);
    doc.overall = 'BLOCKED';
    finish(doc, opts);
    return 1;
  }
  if (doRequired) doc.required = await runRequired();
  if (doAdversarial) doc.adversarial = await runAdversarial();
  doc.summary = summarize([...doc.required, ...doc.adversarial]);
  doc.overall = overallOf([...doc.required, ...doc.adversarial]);
  finish(doc, opts);
  return doc.overall === 'PASS' ? 0 : 1;
}

function printHelp() {
  process.stdout.write(`task-37-stdio-qa — Task 37 stdio/in-memory MCP primitive matrix

Usage: node scripts/qa/task-37-stdio-qa.mjs [mode] [options]

Modes:
  --self-check   no-op readiness: print the matrix + plan JSON, run NO feature
                 assertion, exit 0 (this is the default with no arguments)
  --run          execute the REQUIRED primitive matrix (post-Task 37)
  --adversarial  execute ONLY the adversarial matrix
  --all          required + adversarial
  --help, -h     this message

Options:
  --out=<path>   override the JSON output path
  --json-only    print only the JSON document to stdout

Notes:
  * Requires a built dist/ (run \`npm run build\`) for --run/--adversarial/--all.
  * Uses a MOCK Unreal connection over an in-memory transport — no live editor.
  * Before Task 37 is implemented, run ONLY --help / --self-check.
`);
}

function parseArgs(argv) {
  const opts = { mode: 'self-check', out: null, jsonOnly: false, help: false, unknown: [] };
  for (const a of argv) {
    if (a === '--help' || a === '-h') opts.help = true;
    else if (a === '--self-check') opts.mode = 'self-check';
    else if (a === '--run') opts.mode = 'run';
    else if (a === '--adversarial') opts.mode = 'adversarial';
    else if (a === '--all') opts.mode = 'all';
    else if (a === '--json-only') opts.jsonOnly = true;
    else if (a.startsWith('--out=')) opts.out = a.slice('--out='.length);
    else opts.unknown.push(a);
  }
  return opts;
}

async function main() {
  const opts = parseArgs(process.argv.slice(2));
  if (opts.help) {
    printHelp();
    return 0;
  }
  process.on('unhandledRejection', (reason) => rejections.push(errMsg(reason)));
  if (opts.mode === 'run') return runMode(opts, true, false);
  if (opts.mode === 'adversarial') return runMode(opts, false, true);
  if (opts.mode === 'all') return runMode(opts, true, true);
  return selfCheck(opts);
}

main()
  .then((code) => process.exit(code))
  .catch((e) => {
    process.stderr.write(`[task-37-stdio-qa] fatal: ${errMsg(e)}\n`);
    process.exit(1);
  });
