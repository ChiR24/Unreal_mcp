// Task 39 durable TS<->native receipt parity normalizer + DEEP comparator.
//
// Consumes a stdio transcript and a genuine native /mcp transcript and DEEP-
// compares each shared step's nested canonical `receipt`. Everything stable is
// compared field-by-field: status, capabilityId, the typed error (kind/code/
// pointer/field/currentRevision/expectedRevision/retryable/supported), handles,
// changes, warnings, nextCalls, validation, the three revision VALUES, requestId,
// idempotencyId and the redacted `data` payload. Only genuinely volatile values
// are excluded, each with an explicit invariant asserted in its place:
//   * correlationId VALUE differs across transports (TS gw-N vs native GUID), so
//     the value is excluded BUT both must be a present non-empty string equal to
//     the step's OUTER correlationId (the minted id threads envelope -> receipt);
//   * requestId is the client-assigned JSON-RPC id; the stdio surface is driven
//     by the MCP SDK client (auto-numbered) and the native surface by the HTTP
//     driver (independently numbered), so the two VALUES differ by construction
//     (the SDK exposes no way to force a chosen id). The value is excluded from
//     cross-transport equality BUT each side must echo its own client id in
//     canonical num:/str: form (the Task-39 echo guarantee). idempotencyId, which
//     is caller-supplied and identical across drivers, IS compared and must match;
//   * timingMs differs, so it is excluded BUT must be a number >= 0 on both;
//   * error.message free-text wording differs, so it is excluded while the typed
//     error FIELDS are compared;
//   * data.cursor / data.nextCursor are opaque pagination continuation tokens
//     that may encode nondeterministic state, so they are narrowly excluded.
//
// BLOCKED, never green, without genuine native input: if the native transcript
// is absent, unreadable, empty, or lacks native transport provenance, the tool
// exits non-zero with status BLOCKED rather than reporting a false parity pass.
//
// Usage:
//   node task-39-parity-normalize.mjs <out.json> <stdio.json> <native.json>
//   node task-39-parity-normalize.mjs --selftest   (negative-injection self-test)
import { existsSync, readFileSync, writeFileSync } from 'node:fs';
import { pathToFileURL } from 'node:url';

const VOLATILE_RECEIPT_KEYS = new Set(['correlationId', 'timingMs', 'requestId']);
const VOLATILE_ERROR_KEYS = new Set(['message']);
const VOLATILE_DATA_KEYS = new Set(['cursor', 'nextCursor']);

function canonicalOuterRequestId(step) {
  const id = step?.request?.id;
  if (typeof id === 'number') return `num:${id}`;
  if (typeof id === 'string') return `str:${id}`;
  return null;
}

export function structured(step) {
  const res = step?.response?.result;
  if (!res) return step?.response ?? {};
  if (res.structuredContent) return res.structuredContent;
  const txt = res.content?.find?.((c) => c.type === 'text')?.text;
  if (txt) {
    try { return JSON.parse(txt); } catch { /* summary text, not json */ }
  }
  return {};
}

function stripData(data) {
  if (Array.isArray(data)) return data.map(stripData);
  if (data && typeof data === 'object') {
    const out = {};
    for (const [k, v] of Object.entries(data)) {
      if (VOLATILE_DATA_KEYS.has(k)) continue;
      out[k] = stripData(v);
    }
    return out;
  }
  return data;
}

// Remove ONLY the volatile values (correlation id, timing, error wording, cursor)
// so the deep compare asserts every stable field. Their invariants are checked
// separately in receiptInvariantDiffs so nothing volatile is dropped unguarded.
export function stripVolatile(receipt) {
  if (!receipt || typeof receipt !== 'object') return receipt;
  const out = {};
  for (const [k, v] of Object.entries(receipt)) {
    if (VOLATILE_RECEIPT_KEYS.has(k)) continue;
    if (k === 'error' && v && typeof v === 'object' && !Array.isArray(v)) {
      const err = {};
      for (const [ek, ev] of Object.entries(v)) {
        if (!VOLATILE_ERROR_KEYS.has(ek)) err[ek] = ev;
      }
      out.error = err;
    } else if (k === 'data') {
      out.data = stripData(v);
    } else {
      out[k] = v;
    }
  }
  return out;
}

export function deepDiff(a, b, path, diffs) {
  if (JSON.stringify(a) === JSON.stringify(b)) return diffs;
  const aObj = a && typeof a === 'object';
  const bObj = b && typeof b === 'object';
  if (aObj && bObj && Array.isArray(a) === Array.isArray(b)) {
    for (const k of new Set([...Object.keys(a), ...Object.keys(b)])) {
      deepDiff(a[k], b[k], path ? `${path}.${k}` : k, diffs);
    }
  } else {
    diffs.push({ field: path, stdio: a, native: b });
  }
  return diffs;
}

export function normalizeStep(step) {
  const s = structured(step);
  return {
    outerErrorCode: s.errorCode ?? null,
    outerCorrelationId: typeof s.correlationId === 'string' ? s.correlationId : null,
    outerRequestId: canonicalOuterRequestId(step),
    nextCallOp: s.nextCall?.operation ?? null,
    receipt: s.receipt && typeof s.receipt === 'object' ? s.receipt : null
  };
}

// The invariants that stand in for the excluded volatile values: correlation is a
// present non-empty string equal to the outer minted id; requestId, when present,
// is a canonical num:/str: id echoing the client's own id; timing is >= 0.
function receiptInvariantDiffs(label, norm) {
  const out = [];
  const r = norm.receipt;
  if (typeof r.correlationId !== 'string' || r.correlationId.length === 0) {
    out.push({ field: `receipt.correlationId.valid(${label})`, stdio: label, native: r.correlationId });
  } else if (r.correlationId !== norm.outerCorrelationId) {
    out.push({ field: `receipt.correlationId==outer(${label})`, stdio: norm.outerCorrelationId, native: r.correlationId });
  }
  if (r.requestId !== undefined) {
    if (typeof r.requestId !== 'string' || !/^(num|str):/.test(r.requestId)) {
      out.push({ field: `receipt.requestId.wellFormed(${label})`, stdio: label, native: r.requestId });
    } else if (norm.outerRequestId !== null && r.requestId !== norm.outerRequestId) {
      out.push({ field: `receipt.requestId==outer(${label})`, stdio: norm.outerRequestId, native: r.requestId });
    }
  }
  if (r.timingMs !== undefined && !(typeof r.timingMs === 'number' && r.timingMs >= 0)) {
    out.push({ field: `receipt.timingMs.nonneg(${label})`, stdio: label, native: r.timingMs });
  }
  return out;
}

export function compareStep(stepId, stdioStep, nativeStep) {
  const a = normalizeStep(stdioStep);
  const b = normalizeStep(nativeStep);
  const diffs = [];
  const aHas = a.receipt !== null;
  const bHas = b.receipt !== null;
  if (aHas !== bHas) {
    diffs.push({ field: 'receipt.present', stdio: aHas, native: bHas });
  } else if (aHas && bHas) {
    diffs.push(...receiptInvariantDiffs('stdio', a), ...receiptInvariantDiffs('native', b));
    deepDiff(stripVolatile(a.receipt), stripVolatile(b.receipt), 'receipt', diffs);
  }
  return {
    stepId,
    equal: diffs.length === 0,
    diffs,
    outerFraming: {
      outerErrorCode: { stdio: a.outerErrorCode, native: b.outerErrorCode },
      nextCallOp: { stdio: a.nextCallOp, native: b.nextCallOp }
    }
  };
}

export function compareTranscripts(stdio, native) {
  const byStdio = new Map((stdio.steps ?? []).map((s) => [s.stepId, s]));
  const byNative = new Map((native.steps ?? []).map((s) => [s.stepId, s]));
  const shared = [...byStdio.keys()].filter((k) => byNative.has(k));
  const comparisons = shared.map((id) => compareStep(id, byStdio.get(id), byNative.get(id)));
  return {
    summary: {
      sharedScenarios: shared.length,
      equal: comparisons.filter((c) => c.equal).length,
      mismatched: comparisons.filter((c) => !c.equal).length
    },
    comparisons
  };
}

function loadOrBlock(path, label) {
  if (!existsSync(path)) {
    console.error(`BLOCKED: ${label} transcript missing at ${path}`);
    process.exit(3);
  }
  try {
    return JSON.parse(readFileSync(path, 'utf8'));
  } catch (err) {
    console.error(`BLOCKED: ${label} transcript unreadable: ${err.message}`);
    process.exit(3);
  }
}

function provenanceOf(native) {
  const initRes = native.handshake?.initialize?.body?.result ?? native.handshake?.initialize?.result ?? {};
  const toolsRes = native.handshake?.toolsList?.body?.result ?? native.handshake?.toolsList?.result ?? {};
  return {
    baseUrl: native.baseUrl ?? null,
    serverInfo: initRes.serverInfo ?? null,
    protocolVersion: initRes.protocolVersion ?? null,
    nativeTools: (toolsRes.tools ?? []).map((t) => t.name)
  };
}

function runCli() {
  const [outPath, stdioPath, nativePath] = process.argv.slice(2);
  if (!outPath || !stdioPath || !nativePath) {
    console.error('BLOCKED: usage: task-39-parity-normalize.mjs <out.json> <stdio.json> <native.json>');
    process.exit(2);
  }
  const stdio = loadOrBlock(stdioPath, 'stdio');
  const native = loadOrBlock(nativePath, 'native');
  const provenance = provenanceOf(native);
  if (!provenance.serverInfo || (native.steps ?? []).length === 0) {
    console.error('BLOCKED: native transcript lacks transport provenance or captured no steps');
    console.error('PROVENANCE', JSON.stringify(provenance));
    process.exit(3);
  }
  const { summary, comparisons } = compareTranscripts(stdio, native);
  writeFileSync(outPath, JSON.stringify({ summary, provenance, comparisons }, null, 2));
  console.log('PROVENANCE', JSON.stringify(provenance));
  for (const c of comparisons) {
    console.log(`${c.equal ? 'MATCH' : 'DIFF '} ${c.stepId}${c.equal ? '' : ' :: ' + JSON.stringify(c.diffs)}`);
  }
  console.log(`\nPARITY ${summary.equal}/${summary.sharedScenarios} normalized-equal | mismatched=${summary.mismatched}`);
  process.exit(summary.mismatched === 0 && summary.sharedScenarios > 0 ? 0 : 1);
}

// A matched stdio/native pair whose ONLY differences are the volatile values
// (correlation id, timing, error wording, data cursor). It must compare EQUAL,
// and every injected stable-field divergence below must be DETECTED.
function baseTranscripts() {
  const CAT = '740752bc2cdcb7b9';
  const CAP = 'c'.repeat(64);
  const SCH = 'd'.repeat(64);
  const clone = (o) => JSON.parse(JSON.stringify(o));
  const successStable = {
    status: 'success', capabilityId: 'asset.list', requestId: 'num:100', idempotencyId: 'k-1',
    catalogRevision: CAT, capabilityRevision: CAP, schemaRevision: SCH,
    nextCalls: [], handles: [], changes: [], warnings: [], validation: { outputSchema: 'passed' }
  };
  const staleStable = {
    status: 'error', capabilityId: 'asset.list', requestId: 'num:200',
    catalogRevision: CAT, capabilityRevision: CAP, schemaRevision: SCH, nextCalls: []
  };
  const makeStep = (stepId, receipt) => ({
    stepId,
    request: { id: receipt.requestId ? Number(receipt.requestId.split(':')[1]) : undefined },
    response: { result: { structuredContent: { correlationId: receipt.correlationId, receipt } } }
  });
  return {
    stdio: { steps: [
      makeStep('s01', { ...clone(successStable), correlationId: 'gw-1', timingMs: 5, data: { folders: ['/Game/Collections'], cursor: 'STDIO-CURSOR' } }),
      makeStep('s09', { ...clone(staleStable), correlationId: 'gw-9', timingMs: 2, error: { kind: 'staleState', code: 'STALE_STATE', message: 'stale (ts wording)', currentRevision: CAT, expectedRevision: 'deadbeef' } })
    ] },
    native: { steps: [
      makeStep('s01', { ...clone(successStable), correlationId: 'GUID-abc', timingMs: 9, data: { folders: ['/Game/Collections'], cursor: 'NATIVE-CURSOR' } }),
      makeStep('s09', { ...clone(staleStable), correlationId: 'GUID-def', timingMs: 8, error: { kind: 'staleState', code: 'STALE_STATE', message: 'stale (native wording)', currentRevision: CAT, expectedRevision: 'deadbeef' } })
    ] }
  };
}

function runSelfTest() {
  const checks = [];
  const record = (name, ok) => { checks.push(ok); console.log(`${ok ? 'PASS' : 'FAIL'} selftest: ${name}`); };
  const stepOf = (t, id) => t.steps.find((s) => s.stepId === id);
  const receiptOf = (t, id) => stepOf(t, id).response.result.structuredContent.receipt;

  const baseline = compareTranscripts(baseTranscripts().stdio, baseTranscripts().native);
  record('matched pair whose only differences are correlation/timing/message/cursor is EQUAL',
    baseline.summary.mismatched === 0 && baseline.summary.sharedScenarios === 2);

  const detects = (mutate) => {
    const t = baseTranscripts();
    mutate(t);
    return compareTranscripts(t.stdio, t.native).summary.mismatched > 0;
  };

  record('detects a deep error CODE change',
    detects((t) => { receiptOf(t.native, 's09').error.code = 'VALIDATION_ERROR'; }));
  record('detects an array CONTENT change',
    detects((t) => { receiptOf(t.native, 's01').changes = ['/Game/Different']; receiptOf(t.stdio, 's01').changes = ['/Game/Same']; }));
  record('detects an array LENGTH change',
    detects((t) => { receiptOf(t.native, 's01').handles = [{ kind: 'asset', path: '/Game/Extra' }]; }));
  record('detects a redacted-data difference (secret unmasked on one side)',
    detects((t) => { receiptOf(t.native, 's01').data = { folders: ['token=leaked-secret'], cursor: 'x' }; }));
  record('detects a malformed-pin misclassification (validation vs staleState)',
    detects((t) => { receiptOf(t.native, 's09').error = { kind: 'validation', code: 'VALIDATION_ERROR', pointer: '/options/expectedCatalogRevision' }; }));
  record('detects a revision VALUE change',
    detects((t) => { receiptOf(t.native, 's01').catalogRevision = 'ffffffffffffffff'; }));
  record('detects a broken requestId echo (receipt id != client id)',
    detects((t) => { receiptOf(t.native, 's01').requestId = 'num:999'; }));
  record('detects a malformed requestId format (no num:/str: prefix)',
    detects((t) => { receiptOf(t.native, 's01').requestId = 'raw-123'; }));
  record('detects an idempotencyId change (caller-supplied, must match)',
    detects((t) => { receiptOf(t.native, 's01').idempotencyId = 'k-other'; }));
  record('detects outer!=receipt correlation drift',
    detects((t) => { stepOf(t.native, 's01').response.result.structuredContent.correlationId = 'DIFFERENT-OUTER'; }));
  record('detects a negative timingMs',
    detects((t) => { receiptOf(t.native, 's01').timingMs = -1; }));
  record('detects a dropped receipt on one side',
    detects((t) => { stepOf(t.native, 's01').response.result.structuredContent.receipt = null; }));

  const passed = checks.filter(Boolean).length;
  console.log(`\nSELFTEST ${passed}/${checks.length} passed`);
  return checks.every(Boolean);
}

const invokedAsScript = process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href;
if (invokedAsScript) {
  if (process.argv.slice(2).includes('--selftest')) {
    process.exit(runSelfTest() ? 0 : 1);
  } else {
    runCli();
  }
}
