// @ts-check
// tests/unit/capability-verdicts/capability-verdicts.mjs
// Task 59 — reading a live gateway's answers, and refusing to over-read them.
//
// These four functions decide what the UE 5.7.4 capability census MEANS. They
// are here rather than inside the probe because the probe needs a live editor
// and they do not: every one of them is a pure function of a response envelope,
// so the thing that turns an engine's answer into a verdict can be tested
// against captured payloads without an engine.
//
// That split is not tidiness. The first version of this probe read the wrong
// half of the response, harvested zero actions from 23 tools that had answered
// correctly, and produced a census that said the product had lost all 1,335 of
// its capabilities. Nothing in a live run would have caught that — the run was
// green — and the census file looked plausible. What catches it is being able to
// hand these functions a real captured payload and assert what they return.

/**
 * Pull the payload out of a gateway tool result.
 *
 * `structuredContent` FIRST, and that ordering is the whole point. The `content`
 * block is a HUMAN-READABLE rendering — `actions: [create_box, ...] (+46 more)` —
 * which is not JSON and is deliberately elided. A reader that took the text would
 * parse nothing and harvest nothing, and report every tool as missing every
 * action: 23 fabricated HIGH defects against a surface that answered correctly.
 * The structured block carries the real arrays.
 * @param {any} response
 */
export function payloadOf(response) {
  const result = response?.result;
  if (result === undefined || result === null) return null;
  if (result.structuredContent !== undefined && result.structuredContent !== null) return result.structuredContent;
  /** @type {any[]} */
  const content = Array.isArray(result.content) ? result.content : [];
  const text = content.filter((entry) => entry?.type === 'text').map((entry) => String(entry.text)).join('\n');
  if (text.length === 0) return null;
  try {
    return JSON.parse(text);
  } catch {
    return { raw: text.slice(0, 4000) };
  }
}

/**
 * Every action name anywhere in a describe payload, without assuming the exact
 * envelope. The gateway's describe shape is progressive and has changed across
 * waves; a reader hard-coded to one nesting would report an empty census as a
 * clean one, which is the failure this whole file is built to avoid.
 * @param {any} node
 * @param {Set<string>} [found]
 */
export function harvestActions(node, found = new Set()) {
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

// WHY THESE ARE FOUR VERDICTS AND NOT TWO. A capability can be absent for three
// unrelated reasons and only one of them is a given engine's business. Folding
// them into a single "gated" would let a plugin nobody enabled in a generated
// project masquerade as a 5.7 engine-gate failure, and would report the product
// broken on its own development target. The order matters: plugin and
// editor-state refusals are checked BEFORE the engine bucket, because the engine
// bucket is deliberately the widest and would otherwise absorb them.
export const VERDICT_RULES = Object.freeze([
  ['GATED_PLUGIN', [
    /[A-Z0-9]+_PLUGIN_NOT_ENABLED/u, /plugin\s+is\s+not\s+enabled/iu,
    /enable\s+the\s+'[^']+'\s+plugin/iu, /requires\s+the\s+\S+\s+plugin/iu,
  ]],
  ['GATED_STATE', [
    /EDITOR_STATE/u, /requires\s+PIE/iu, /only\s+(?:available|valid)\s+(?:in|during)\s+(?:PIE|play|simulate)/iu,
    /not\s+(?:available|valid)\s+in\s+edit(?:or)?\s*mode/iu, /must\s+be\s+(?:in\s+)?play(?:ing)?/iu,
  ]],
  ['REACHED', [
    /ACTOR_NOT_FOUND/u, /ASSET_NOT_FOUND/u, /INVALID_TARGET/u, /NOT_FOUND/u,
    /not\s*found/iu, /does\s*not\s*exist/iu, /no\s+such/iu, /failed to (?:find|resolve)/iu,
  ]],
  ['GATED_ENGINE', [
    /UNKNOWN_ACTION/u, /UNKNOWN_CAPABILITY/u, /CAPABILITY_NOT_FOUND/u, /UNSUPPORTED/u,
    /NOT_SUPPORTED/u, /not\s+compiled/iu, /requires\s+(?:UE|Unreal|engine)\s*\d/iu,
    /not\s+available\s+(?:on|in)\s+(?:this\s+)?(?:UE|Unreal|engine)/iu,
  ]],
]);

/**
 * Gateway-level refusals that never reached the editor at all. These say
 * something about the CALL, not about the engine, and calling one a gate verdict
 * would blame the product for a malformed request. The first live census scored
 * all four of its probes unreadable for exactly this reason: it repeated `action`
 * inside `params`, the gateway refused with INVALID_PARAMS before dispatch, and
 * nothing ever reached the plugin.
 */
export const CALLER_ERROR_CODES = new Set(['INVALID_PARAMS', 'VALIDATION_ERROR', 'SCHEMA_VIOLATION', 'UNKNOWN_OPERATION']);

/**
 * Did this response come back from the EDITOR, or was it turned away before it
 * got there?
 *
 * This is a structural reading, not a textual one, and that is deliberate: the
 * plugin stamps every receipt it produces with the capability revision it
 * dispatched and the live revisions it saw (`selection`, `level`,
 * `assetRegistry`, `package`). Nothing upstream of the game thread can
 * manufacture those, so their presence proves the handler was compiled in,
 * registered and reached on THIS engine — even when the handler then refused the
 * call. That distinction is the whole capability question: a refusal from the
 * editor means the capability is present, a refusal before it means nothing at
 * all about the engine.
 * @param {any} response
 */
export function reachedEditor(response) {
  const structured = response?.result?.structuredContent ?? null;
  if (structured === null || typeof structured !== 'object') return false;
  return structured.liveRevisions !== undefined
    || (typeof structured.capabilityRevision === 'string' && typeof structured.correlationId === 'string');
}

/**
 * Classify one execute response by WHICH gate answered, not merely whether one
 * did.
 * @param {any} response
 */
export function classify(response) {
  const result = response?.result;
  const structured = result?.structuredContent ?? null;
  const reached = reachedEditor(response);
  const text = result === undefined || result === null
    ? String(response?.error?.message ?? '')
    : JSON.stringify(result);
  if (result !== undefined && result !== null
    && result.isError !== true && structured?.success !== false) {
    return { verdict: 'SUCCEEDED', errorCode: null, reachedEditor: reached, evidence: text.slice(0, 400) };
  }
  const errorCode = typeof structured?.errorCode === 'string'
    ? structured.errorCode
    : (/Error \[([A-Z_]+)\]/u.exec(text)?.[1] ?? null);
  // Order matters. A caller-error code that nonetheless came back stamped with
  // the editor's live revisions was refused BY the handler, which is proof the
  // capability exists here — the opposite of what the same code means when the
  // gateway turned the call away before dispatch.
  if (errorCode !== null && CALLER_ERROR_CODES.has(errorCode) && !reached) {
    return { verdict: 'REJECTED_INPUT', errorCode, reachedEditor: false, evidence: String(structured?.error ?? '').slice(0, 400) };
  }
  if (reached && (errorCode === 'INVALID_ARGUMENT' || CALLER_ERROR_CODES.has(String(errorCode)))) {
    return {
      verdict: 'REACHED', errorCode, reachedEditor: true,
      evidence: `handler validated and refused the call (${String(structured?.message ?? errorCode)}); `
        + `the receipt carries the editor's live revisions, so the capability is registered on this engine`,
    };
  }
  // The plugin's own refusal is nested under `result.result` when the gateway
  // relayed it, so the whole envelope is searched rather than one blessed field.
  for (const [verdict, patterns] of VERDICT_RULES) {
    for (const rx of /** @type {RegExp[]} */ (patterns)) {
      const hit = rx.exec(text);
      if (hit !== null) {
        return { verdict, errorCode, reachedEditor: reached, evidence: text.slice(Math.max(0, hit.index - 140), hit.index + 300) };
      }
    }
  }
  return { verdict: 'UNCLEAR', errorCode, reachedEditor: reached, evidence: text.slice(0, 600) };
}

/**
 * Read one canonical tool's FULL action list out of the gateway, following the
 * pagination it advertises.
 *
 * The native surface answers `describe {tool}` with a page of 20 while the
 * TypeScript surface answers with all of them. A census that took the first page
 * as the whole list reported 22 of 23 tools as having lost most of their actions
 * — 1,000-odd fabricated defects against a surface that had paginated exactly as
 * documented. So the page size is never assumed and `actionCount`/`actionHasMore`
 * are followed to the end.
 *
 * The authoritative `actions` array is preferred over a deep scan of the
 * envelope. The deep scan is kept only as a fallback for a re-shaped payload,
 * and which one was used is RECORDED, because the scan also picks up `action`
 * keys belonging to `drillDown` hints and example `nextCall`s and would report
 * those as capabilities the engine invented.
 * @param {{ callTool: (args: any, options?: any) => Promise<any> }} driver
 * @param {string} tool
 */
export async function censusTool(driver, tool) {
  /** @type {Set<string>} */
  const found = new Set();
  /** @type {Array<Record<string, unknown>>} */
  const pages = [];
  let offset = 0;
  let declaredTotal = null;
  for (let page = 0; page < 40; page += 1) {
    const call = await driver.callTool({ operation: 'describe', tool, offset }, { timeoutMs: 120_000 });
    const payload = payloadOf(call.response);
    const actions = Array.isArray(payload?.actions)
      ? /** @type {any[]} */ (payload.actions).filter((entry) => typeof entry === 'string')
      : null;
    if (actions === null) {
      return {
        names: [...harvestActions(payload)].sort(),
        method: 'deep-harvest-fallback',
        pages: [...pages, { offset, returned: null, note: 'no top-level actions array; fell back to a deep scan' }],
        isError: call.response?.result?.isError === true,
      };
    }
    declaredTotal = typeof payload.actionCount === 'number' ? payload.actionCount : declaredTotal;
    for (const name of actions) found.add(name);
    pages.push({ offset, returned: actions.length, actionCount: payload.actionCount ?? null, hasMore: payload.actionHasMore ?? null });
    const more = payload.actionHasMore === true
      || (typeof declaredTotal === 'number' && found.size < declaredTotal);
    if (!more || actions.length === 0) break;
    offset += actions.length;
  }
  return { names: [...found].sort(), method: 'actions-array-paged', pages, reportedCount: declaredTotal, isError: false };
}

/**
 * `left` >= `right`, over the {major,minor,patch} shape the capability records
 * use for their declared engine range.
 * @param {{major:number,minor:number,patch:number}} left
 * @param {{major:number,minor:number,patch:number}} right
 */
export function atLeast(left, right) {
  if (left.major !== right.major) return left.major > right.major;
  if (left.minor !== right.minor) return left.minor > right.minor;
  return left.patch >= right.patch;
}
