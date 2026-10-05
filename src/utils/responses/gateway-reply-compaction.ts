import { isRecord } from '../validation/type-guards.js';

type Json = Record<string, unknown>;

// The reply a client reads. Execute, describe and search replies carried fields only logs and replay
// bookkeeping use (revision hashes, correlation ids, timings, live-state counters) and fields that
// repeat another word for word: one "Actor not found" arrived six times, every success twice over
// in `receipt`. Both are left out here; what can inform a next call stays. The input is never
// changed (the idempotency ledger keeps the full receipt). Mirrored by McpJsonRpcReplyCompaction.cpp.
export const LOG_ONLY_EXECUTE_FIELDS = [
  'capabilityId', 'capability', 'tool', 'action', 'status', 'options', 'catalogRevision',
  'capabilityRevision', 'schemaRevision', 'correlationId', 'replayedFrom', 'liveRevisions'
] as const;

/** What ran, kept when the call named it differently (an alias or another action name) and dropped as an echo otherwise. */
export const RESOLVED_IDENTITY_FIELDS = ['capabilityId', 'capability', 'tool', 'action'] as const;

/** The receipt fields that say what the call did; the rest of a receipt is bookkeeping. */
export const RECEIPT_OUTCOME_LISTS = ['nextCalls', 'handles', 'changes', 'warnings'] as const;

/** Output properties every capability declares in the same words; the reply itself shows them. */
export const STOCK_OUTPUT_PROPERTIES: Readonly<Record<string, string>> = {
  success: 'Whether the action succeeded.',
  message: 'Human-readable result message.',
  details: 'Additional handler result fields not named by the contract.'
};

const SEARCH_ROW_RANKING_FIELDS = ['matchReasons', 'score'] as const;

const same = (left: unknown, right: unknown): boolean =>
  left !== undefined && right !== undefined && JSON.stringify(left) === JSON.stringify(right);

function omit(source: Json, keys: readonly string[]): Json {
  const out = { ...source };
  for (const key of keys) delete out[key];
  return out;
}

function strings(value: unknown): string[] {
  return Array.isArray(value) ? value.filter((item): item is string => typeof item === 'string') : [];
}

// A payload's `warnings` that the receipt already lists, at its root and down `details` (where the
// receipt collects them from).
function withoutListedWarnings(payload: Json, listed: readonly string[]): Json {
  const out = { ...payload };
  if (Array.isArray(out.warnings) && out.warnings.every((item) => typeof item === 'string' && listed.includes(item))) {
    delete out.warnings;
  }
  if (isRecord(out.details)) out.details = withoutListedWarnings(out.details, listed);
  return out;
}

function receiptOutcome(receipt: Json, reply: Json): Json | undefined {
  const out: Json = {};
  for (const key of RECEIPT_OUTCOME_LISTS) {
    const list = receipt[key];
    if (Array.isArray(list) && list.length > 0) out[key] = list;
  }
  if (isRecord(receipt.task)) out.task = receipt.task;
  if (isRecord(receipt.error)) {
    const typed = isRecord(reply.typedError) ? reply.typedError : {};
    const error = { ...receipt.error };
    for (const key of Object.keys(error)) {
      if (same(error[key], typed[key]) || same(error[key], reply[key])) delete error[key];
    }
    if (Object.keys(error).length > 0) out.error = error;
  }
  return Object.keys(out).length > 0 ? out : undefined;
}

function compactTypedError(typedError: Json, reply: Json): Json {
  const out = { ...typedError };
  for (const key of Object.keys(out)) {
    if (same(out[key], reply[key])) delete out[key];
  }
  if (same(out.handlerCode, reply.errorCode)) delete out.handlerCode;
  if (isRecord(out.unrealDetail)) {
    const detail = omit(out.unrealDetail, ['success']);
    if (isRecord(detail.error) && same(detail.error.message, reply.message) && same(detail.error.code, reply.errorCode)) {
      delete detail.error;
    }
    if (isRecord(detail.data) && Object.keys(detail.data).length === 0) delete detail.data;
    if (Object.keys(detail).length > 0) out.unrealDetail = detail;
    else delete out.unrealDetail;
  }
  return out;
}

function compactExecute(reply: Json): Json {
  const out = omit(reply, LOG_ONLY_EXECUTE_FIELDS);
  const migrated = isRecord(reply.migratedFrom) ? reply.migratedFrom : undefined;
  const translated = reply.resolvedFromAlias !== undefined ||
    (migrated !== undefined && !(same(migrated.tool, reply.tool) && same(migrated.action, reply.action)));
  if (translated) {
    for (const key of RESOLVED_IDENTITY_FIELDS) if (key in reply) out[key] = reply[key];
  } else {
    delete out.migratedFrom;
  }
  if (same(out.error, out.message)) delete out.error;
  const receipt = isRecord(reply.receipt) ? receiptOutcome(reply.receipt, reply) : undefined;
  if (receipt === undefined) delete out.receipt;
  else out.receipt = receipt;
  if (isRecord(out.typedError)) out.typedError = compactTypedError(out.typedError, out);
  const listed = strings(receipt?.warnings);
  if (Array.isArray(out.warnings) && out.warnings.every((item) => typeof item === 'string' && listed.includes(item))) {
    delete out.warnings;
  }
  if (isRecord(out.data)) {
    const data = withoutListedWarnings(out.data, listed);
    if (same(data.success, out.success)) delete data.success;
    if (same(data.message, out.message)) delete data.message;
    out.data = data;
  }
  return out;
}

function compactSchema(schema: Json, parameters: unknown): Json {
  const out = omit(schema, ['$schema']);
  if (!isRecord(out.properties) || !Array.isArray(parameters)) return out;
  // parameters[] already carries each top-level description; nested ones exist only here.
  const listed = new Map(parameters.filter(isRecord).map((parameter) => [parameter.name, parameter.description]));
  const properties: Json = {};
  for (const [name, property] of Object.entries(out.properties)) {
    properties[name] = isRecord(property) && same(property.description, listed.get(name))
      ? omit(property, ['description'])
      : property;
  }
  out.properties = properties;
  return out;
}

function compactOutputSchema(schema: Json): Json | undefined {
  const out = omit(schema, ['$schema']);
  const removed: string[] = [];
  if (isRecord(out.properties)) {
    const properties = { ...out.properties };
    for (const [name, text] of Object.entries(STOCK_OUTPUT_PROPERTIES)) {
      const property = properties[name];
      if (isRecord(property) && property.description === text) {
        delete properties[name];
        removed.push(name);
      }
    }
    if (Object.keys(properties).length === 0) return undefined;
    out.properties = properties;
  }
  if (Array.isArray(out.required)) {
    const required = out.required.filter((name) => typeof name !== 'string' || !removed.includes(name));
    if (required.length > 0) out.required = required;
    else delete out.required;
  }
  return out;
}

function compactDescribe(reply: Json): Json {
  const out = omit(reply, ['hashes', 'exampleCount']);
  if (out.scope !== 'capability') return out;
  if (out.success === true) delete out.message;
  for (const key of ['parent', 'parentTool']) {
    if (same(out[key], out.tool)) delete out[key];
  }
  if (isRecord(out.behavior) && same(out.behavior.effect, out.effect)) out.behavior = omit(out.behavior, ['effect']);
  if (Array.isArray(out.examples)) {
    out.examples = out.examples.map((example) =>
      isRecord(example) && same(example.title, out.summary) ? omit(example, ['title']) : example);
  }
  if (isRecord(out.inputSchema)) out.inputSchema = compactSchema(out.inputSchema, out.parameters);
  if (isRecord(out.outputSchema)) {
    const outputSchema = compactOutputSchema(out.outputSchema);
    if (outputSchema === undefined) delete out.outputSchema;
    else out.outputSchema = outputSchema;
  }
  return out;
}

function compactSearch(reply: Json): Json {
  const out = omit(reply, ['query']);
  if (out.success === true) delete out.message;
  if (Array.isArray(out.results)) {
    out.results = out.results.map((row) => {
      if (!isRecord(row)) return row;
      const kept = omit(row, SEARCH_ROW_RANKING_FIELDS);
      const nextTool = isRecord(row.nextCall) ? row.nextCall.tool : undefined;
      for (const key of ['parent', 'parentTool']) {
        if (same(kept[key], nextTool)) delete kept[key];
      }
      return kept;
    });
  }
  return out;
}

export function compactGatewayReply(reply: unknown): unknown {
  if (!isRecord(reply)) return reply;
  switch (reply.operation) {
    case 'execute': return compactExecute(reply);
    case 'describe': return compactDescribe(reply);
    case 'search': return compactSearch(reply);
    default: return reply;
  }
}
