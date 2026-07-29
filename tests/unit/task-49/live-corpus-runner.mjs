// @ts-check
// tests/unit/task-49/live-corpus-runner.mjs
// Task 49 — the TRANSPORT-BLIND execution semantics of the corpus.
//
// This module turns a validated scenario into a gateway call, decides what a
// response MEANS, and runs the independent oracle. It never touches a socket:
// both drivers expose the same two methods (`callTool`, `cancel`), so everything
// here is exercised offline against a fake driver, and a live divergence can
// never be "two scripts read the response differently" — there is one reader.
//
// Response shape note (observed on the wire by the Task 42/43 probes): a gateway
// reply carries its payload BOTH as `result.structuredContent` and as
// `result.content[0].text` formatted "<message>\n\n<receipt json>". We prefer
// structuredContent and fall back to parsing the text, because a transport that
// only fills one of the two must still be judged, not skipped.

/** Setup, cleanup and oracle steps always use the LEGACY REQUEST form, so a
 * scaffolding failure can never be the canonical form being unsupported
 * somewhere — that is what the scenario under test is for.
 *
 * The step's `consent.capability` is a separate matter and must name the
 * CANONICAL capability id. The plugin's CheckConsent compares the grant against
 * the id it resolved, and over stdio an alias is canonicalised before that
 * check; a cleanup grant naming the alias is therefore refused, which is how
 * destructive fixtures were silently left on disk.
 *
 * Scenario and step values are intentionally typed loose: they are already frozen
 * and closed by live-corpus-schema.mjs, so restating their shape here would be a
 * second contract free to drift from the validated one.
 * @param {any} record @param {any} params @param {any} consent */
export function legacyArgs(record, params, consent) {
  const legacy = record.legacyIds?.[0];
  /** @type {Record<string, unknown>} */
  const args = {
    operation: 'execute',
    tool: legacy?.tool,
    action: legacy?.action,
    params: params ?? {},
  };
  if (consent) args.consent = consent;
  return args;
}

/**
 * Compile a validated scenario into the gateway arguments for its primitive and
 * execute form. This is the ONLY place a form becomes wire shape.
 * @param {any} scenario
 * @returns {Record<string, unknown>}
 */
export function compileRequest(scenario) {
  const discovery = scenario.discovery ?? {};
  switch (scenario.primitive) {
    case 'search':
      return { operation: 'search', ...pick(discovery, ['query', 'domain', 'family', 'limit']) };
    case 'describe':
      return { operation: 'describe', ...pick(discovery, ['tool', 'action', 'param', 'domain', 'family']) };
    case 'configure':
      return { operation: 'configure', action: discovery.action, params: scenario.params ?? {} };
    case 'execute': {
      /** @type {Record<string, unknown>} */
      const args = { operation: 'execute', params: scenario.params ?? {} };
      if (scenario.form === 'canonical') {
        args.capability = scenario.capability;
      } else {
        const legacy = scenario.record?.legacyIds?.[0];
        args.tool = legacy?.tool;
        args.action = legacy?.action;
      }
      if (scenario.options) args.options = scenario.options;
      if (scenario.consent) args.consent = scenario.consent;
      return args;
    }
    default:
      throw new Error(`unknown primitive "${scenario.primitive}"`);
  }
}

/** @param {Record<string, unknown>} source @param {readonly string[]} keys */
function pick(source, keys) {
  /** @type {Record<string, unknown>} */
  const out = {};
  for (const key of keys) if (source[key] !== undefined) out[key] = source[key];
  return out;
}

/**
 * Extract the gateway receipt from a JSON-RPC response frame, whichever of the
 * two documented locations it arrived in.
 * @param {any} frame @returns {any|null}
 */
export function receiptOf(frame) {
  if (frame === null || frame === undefined) return null;
  const structured = frame?.result?.structuredContent;
  if (structured !== undefined && structured !== null) return structured;
  const text = frame?.result?.content?.[0]?.text;
  if (typeof text !== 'string') return null;
  const brace = text.indexOf('{');
  if (brace < 0) return null;
  try { return JSON.parse(text.slice(brace)); } catch { return null; }
}

/** Typed error codes are UPPER_SNAKE; the numeric JSON-RPC code is framing, not contract. */
const TYPED_CODE = /"(?:errorCode|gatewayCode|code)"\s*:\s*"([A-Z][A-Z0-9_]*)"|Error \[([A-Z][A-Z0-9_]*)\]/u;

/**
 * Decide what a transport observation MEANS, in the vocabulary of the expectation
 * grammar. One classifier for both transports.
 * @param {{ response: any, body: string, status: number }} observed
 * @returns {{ intent: 'success'|'error'|'timeout', errorCode: string|null, text: string, receipt: any|null }}
 */
export function classifyOutcome(observed) {
  const text = observed.body ?? '';
  if (observed.response === null || observed.response === undefined || text === 'TIMEOUT' || observed.status === 0) {
    return { intent: 'timeout', errorCode: null, text, receipt: null };
  }
  const receipt = receiptOf(observed.response);
  const match = TYPED_CODE.exec(text);
  const errorCode = match ? (match[1] ?? match[2] ?? null) : null;

  const jsonRpcError = observed.response?.error !== undefined;
  const toolError = observed.response?.result?.isError === true;
  const receiptFailed = receipt !== null && (receipt.success === false || typeof receipt.errorCode === 'string');
  if (jsonRpcError || toolError || receiptFailed) {
    return { intent: 'error', errorCode, text, receipt };
  }
  return { intent: 'success', errorCode: null, text, receipt };
}

/**
 * Judge one observation against one scenario's expectation.
 *
 * Two rules do the real work:
 *   - the PRIMARY intent must match, or one of the narrow success-alternatives
 *     must appear in the response text (only when the primary intent is success)
 *   - an error-primary scenario must match the EXACT typed code it named; "some
 *     error happened" is the failure mode this whole schema exists to prevent,
 *     because a misconfigured probe produces errors too
 * @param {any} scenario @param {ReturnType<typeof classifyOutcome>} outcome
 */
export function judge(scenario, outcome) {
  const wanted = scenario.expected.intent;
  if (outcome.intent === wanted) {
    if (wanted === 'error' && scenario.expectedErrorCode !== null) {
      if (outcome.errorCode !== scenario.expectedErrorCode) {
        return {
          pass: false,
          reason: `expected typed code ${scenario.expectedErrorCode}, observed ${outcome.errorCode ?? 'none'}`,
        };
      }
    }
    return { pass: true, reason: `primary intent "${wanted}" satisfied` };
  }
  if (wanted === 'success' && scenario.expected.alternatives.length > 0) {
    const haystack = outcome.text.toLowerCase();
    const hit = scenario.expected.alternatives.find((/** @type {string} */ alternative) => haystack.includes(alternative.toLowerCase()));
    if (hit !== undefined) return { pass: true, reason: `narrow alternative "${hit}" matched` };
  }
  return { pass: false, reason: `expected "${scenario.expected.text}", observed "${outcome.intent}"` };
}

/**
 * ONE independent oracle reading. Mirrors the Task 42/43 mechanic exactly: read a
 * SEPARATE list capability and search its returned array. Returns `null` when the
 * read itself was unusable, so an unusable oracle is reported inconclusive rather
 * than counted as a convenient negative.
 * @param {{ callTool: Function }} driver @param {any} scenario @param {number} timeoutMs
 */
export async function readOracleOnce(driver, scenario, timeoutMs) {
  const oracle = scenario.oracle;
  const observed = await driver.callTool(legacyArgs(oracle.record, oracle.params, null), { timeoutMs });
  const receipt = receiptOf(observed.response);
  const listed = receipt?.data?.assets ?? receipt?.receipt?.data?.assets ?? receipt?.assets ?? null;
  if (Array.isArray(listed)) {
    return {
      verdict: listed.some((entry) => JSON.stringify(entry).includes(oracle.needle)),
      via: 'folder-listing',
      count: listed.length,
    };
  }
  // The folder read failed. A refused create never creates the folder either, so
  // the PARENT listing is conclusive in the negative direction only.
  const parentPath = String(oracle.params.path ?? '').replace(/\/[^/]+$/u, '') || '/Game';
  const parent = await driver.callTool(legacyArgs(oracle.record, { ...oracle.params, path: parentPath }, null), { timeoutMs });
  const parentReceipt = receiptOf(parent.response);
  const folders = parentReceipt?.data?.folders ?? parentReceipt?.receipt?.data?.folders ?? null;
  if (Array.isArray(folders)) {
    const owned = String(oracle.params.path ?? '');
    const present = folders.some((folder) => String(folder).replace(/\/$/u, '') === owned);
    if (!present) return { verdict: false, via: 'parent-listing (owned folder absent)', count: 0 };
  }
  return { verdict: null, via: 'unusable (neither the owned folder nor its parent listed)', count: null };
}

/**
 * Poll the oracle. Presence is sticky, so stop at the first `true`; absence is
 * trusted only after the asset registry has had every chance to surface a late
 * creation, so BOTH polarities get the same window. The verdict is the last
 * CONCLUSIVE reading, and every reading is kept in the evidence.
 * @param {{ callTool: Function }} driver @param {any} scenario @param {number} timeoutMs
 */
export async function consultOracle(driver, scenario, timeoutMs) {
  const oracle = scenario.oracle;
  /** @type {any[]} */
  const readings = [];
  /** @type {boolean|null} */
  let last = null;
  for (let attempt = 0; attempt < oracle.attempts; attempt += 1) {
    const reading = await readOracleOnce(driver, scenario, timeoutMs);
    readings.push(reading);
    if (reading.verdict !== null) last = reading.verdict;
    if (reading.verdict === true) break;
    if (attempt + 1 < oracle.attempts && oracle.intervalMs > 0) {
      await new Promise((settle) => { setTimeout(settle, oracle.intervalMs); });
    }
  }
  const wanted = oracle.expect === 'present';
  return {
    expect: oracle.expect,
    verdict: last,
    conclusive: last !== null,
    // An inconclusive oracle NEVER passes. A mutation nobody could observe is
    // not a mutation anybody proved.
    pass: last !== null && last === wanted,
    readings,
    needle: oracle.needle,
    capability: oracle.capability,
  };
}

/**
 * Run one scenario end to end on one driver: setup -> request -> judge -> oracle
 * -> cleanup. Cleanup runs in a finally, so a thrown assertion still removes the
 * fixture; a corpus that leaks on failure poisons every later run.
 * @param {any} driver @param {any} scenario
 * @param {{ onStep?: (phase: string, detail: Record<string, unknown>) => void }} [hooks]
 */
export async function runScenario(driver, scenario, hooks = {}) {
  const timeoutMs = scenario.timeoutMs;
  /** @type {Record<string, any>} */
  const record = {
    namespace: scenario.namespace,
    driver: driver.name,
    primitive: scenario.primitive,
    form: scenario.form,
    capability: scenario.capability,
    timeoutTier: scenario.timeoutTier,
    timeoutMs,
    setup: /** @type {any[]} */ ([]),
    cleanup: /** @type {any[]} */ ([]),
    protocol: scenario.protocol?.kind ?? null,
  };

  try {
    for (const step of scenario.setup) {
      const observed = await driver.callTool(legacyArgs(step.record, step.params, step.consent), { timeoutMs });
      const outcome = classifyOutcome(observed);
      record.setup.push({ capability: step.capability, intent: outcome.intent, errorCode: outcome.errorCode });
      hooks.onStep?.('setup', { capability: step.capability, intent: outcome.intent });
      // A setup step that silently failed makes every later assertion vacuous —
      // Task 46's drain test asserted a container emptied that was never filled.
      if (outcome.intent !== 'success' && !step.tolerateFailure) {
        record.status = 'BLOCKED';
        record.detail = `setup step ${step.capability} did not succeed (${outcome.intent}/${outcome.errorCode ?? 'no code'}); every later assertion would be vacuous`;
        return record;
      }
    }

    // PRE-STATE. A `present` reading only proves THIS call created the asset if
    // the asset was absent first. In run 1 the native side created and failed to
    // delete the fixture, so the stdio side's oracle read `present` while its own
    // create had errored — a false positive that scored a broken call as proven.
    if (scenario.oracle !== null && scenario.mutates) {
      const before = await readOracleOnce(driver, scenario, timeoutMs);
      record.preState = before;
      if (before.verdict === true) {
        record.status = 'BLOCKED';
        record.detail = `"${scenario.oracle.needle}" already existed before this scenario ran; a "present" reading `
          + 'could not distinguish this call from a leftover, and an "absent" reading would be judging a fixture nobody made';
        return record;
      }
    }

    const args = compileRequest(scenario);
    record.request = args;
    const call = await invoke(driver, scenario, args, timeoutMs);
    record.requestId = call.requestId ?? null;
    record.ms = call.ms ?? null;
    const outcome = classifyOutcome(call);
    record.observed = { intent: outcome.intent, errorCode: outcome.errorCode };
    const verdict = judge(scenario, outcome);
    record.expected = scenario.expected.text;
    record.expectedErrorCode = scenario.expectedErrorCode;
    record.judgement = verdict;
    // A failure nobody can diagnose from the report costs another live run to
    // reproduce. Bounded so a large listing cannot bloat the evidence file.
    if (!verdict.pass) record.rawBody = String(call.body ?? '').slice(0, 2000);

    if (scenario.protocol !== null) {
      record.protocolCheck = await checkProtocol(driver, scenario, call, timeoutMs);
    }

    if (scenario.oracle !== null) {
      record.oracle = await consultOracle(driver, scenario, timeoutMs);
    }

    const oracleOk = scenario.oracle === null ? true : record.oracle.pass === true;
    const protocolOk = record.protocolCheck === undefined ? true : record.protocolCheck.pass === true;
    record.status = verdict.pass && oracleOk && protocolOk ? 'PASS' : 'FAIL';
    return record;
  } finally {
    for (const step of scenario.cleanup) {
      try {
        const observed = await driver.callTool(legacyArgs(step.record, step.params, step.consent), { timeoutMs });
        const outcome = classifyOutcome(observed);
        record.cleanup.push({ capability: step.capability, intent: outcome.intent, errorCode: outcome.errorCode, tolerated: step.tolerateFailure });
      } catch (error) {
        record.cleanup.push({ capability: step.capability, intent: 'threw', error: error instanceof Error ? error.message : String(error) });
      }
    }
    // VERIFY the cleanup with the same independent read, never with the delete
    // call's own response. The first run of this corpus reported `0 leaked` while
    // two materials sat on disk: every delete had answered INVALID_ARGUMENT and
    // `tolerateFailure` swallowed it. A cleanup nobody re-read is not a cleanup.
    if (scenario.oracle !== null && scenario.cleanup.length > 0) {
      const after = await readOracleOnce(driver, scenario, timeoutMs);
      record.cleanupVerified = after.verdict === false;
      record.cleanupReading = after;
      if (record.cleanupVerified !== true) record.status = 'FAIL';
    }
  }
}

/**
 * Issue the scenario's call with whatever MCP-level augmentation it declared.
 * @param {any} driver @param {any} scenario @param {Record<string, unknown>} args @param {number} timeoutMs
 */
async function invoke(driver, scenario, args, timeoutMs) {
  const protocol = scenario.protocol;
  if (protocol === null) return driver.callTool(args, { timeoutMs });
  if (protocol.kind === 'progress') {
    return driver.callTool(args, { timeoutMs, meta: { progressToken: protocol.progressToken } });
  }
  if (protocol.kind === 'task') {
    return driver.callTool(args, { timeoutMs, task: { ttl: protocol.taskTtlMs ?? 60_000 } });
  }
  // cancel: send the call, then cancel it mid-flight. We do NOT await the call
  // before cancelling — cancelling a settled request proves nothing.
  const inFlight = driver.callTool(args, { timeoutMs });
  await new Promise((settle) => { setTimeout(settle, protocol.cancelAfterMs ?? 50); });
  const requestIdGuess = driver.rpcId;
  await driver.cancel(requestIdGuess, 'task49 corpus cancellation probe');
  return inFlight;
}

/**
 * The protocol-level assertion for progress/task/cancel scenarios. Each one is
 * deliberately narrow enough to be falsifiable:
 *   progress — the server must never invent a progressToken the client did not send
 *   task     — a task-augmented call must settle (or be refused with its typed code)
 *   cancel   — the connection must still serve an INDEPENDENT call afterwards
 * @param {any} driver @param {any} scenario @param {any} call @param {number} timeoutMs
 */
async function checkProtocol(driver, scenario, call, timeoutMs) {
  const kind = scenario.protocol.kind;
  if (kind === 'progress') {
    const notifications = [...(call.streamNotifications ?? []), ...(driver.notifications ?? [])];
    const foreign = notifications.filter((frame) => frame?.method === 'notifications/progress'
      && frame?.params?.progressToken !== undefined
      && frame.params.progressToken !== scenario.protocol.progressToken);
    return {
      kind, pass: foreign.length === 0,
      observedProgressNotifications: notifications.filter((frame) => frame?.method === 'notifications/progress').length,
      foreignTokens: foreign.length,
      detail: foreign.length === 0
        ? 'no progress notification carried a token the client did not supply'
        : `${foreign.length} progress notification(s) carried a foreign token`,
    };
  }
  if (kind === 'task') {
    // Settled either way is the checkable claim: a task-augmented call must not
    // hang. Whether it ran or was refused is the scenario's own expectation.
    const settled = call.response !== null && call.response !== undefined;
    return { kind, pass: settled, detail: settled ? 'task-augmented call settled' : 'task-augmented call never settled' };
  }
  // cancel
  const followUp = await driver.callTool({ operation: 'search', query: 'post-cancellation liveness' }, { timeoutMs });
  const outcome = classifyOutcome(followUp);
  return {
    kind,
    pass: outcome.intent === 'success',
    detail: outcome.intent === 'success'
      ? 'an independent call on the same session succeeded after notifications/cancelled'
      : `the transport did not serve a follow-up call after cancellation (${outcome.intent})`,
  };
}

/** Coverage actually present in a validated corpus, for a test to compare against its claim.
 * @param {readonly any[]} scenarios */
export function coverageOf(scenarios) {
  return {
    primitives: [...new Set(scenarios.map((entry) => entry.primitive))].sort(),
    protocolKinds: [...new Set(scenarios.filter((entry) => entry.protocol !== null).map((entry) => entry.protocol.kind))].sort(),
    executeForms: [...new Set(scenarios.filter((entry) => entry.form !== null).map((entry) => entry.form))].sort(),
    oraclePolarities: [...new Set(scenarios.filter((entry) => entry.oracle !== null).map((entry) => entry.oracle.expect))].sort(),
  };
}
