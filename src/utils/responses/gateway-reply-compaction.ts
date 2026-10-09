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

// A payload list (warnings, changedAssets, changedEntities) whose every entry the receipt already lists, at its root and down
// `details` (where the receipt collects them from).
function withoutListed(payload: Json, field: string, listed: readonly string[]): Json {
  const out = { ...payload };
  const list = out[field];
  if (Array.isArray(list) && list.every((item) => typeof item === 'string' && listed.includes(item))) delete out[field];
  if (isRecord(out.details)) out.details = withoutListed(out.details, field, listed);
  return out;
}

// The projection puts the declared fields at the top of `data` and the rest in `details`: a value there (a path or
// a name of 8 or more characters) that repeats a declared field's (the read-back assetPath of a widgetPath, actorName
// beside name) says nothing new; the receipt read its fields from the full result before this.
function withoutRepeatedValues(data: Json): Json {
  if (!isRecord(data.details)) return data;
  const declared = Object.entries(data).filter(([key, value]) => key !== 'details' && typeof value === 'string').map(([, value]) => value);
  const details = { ...data.details };
  for (const [key, value] of Object.entries(details)) {
    if (typeof value === 'string' && value.length >= 8 && declared.includes(value)) delete details[key];
  }
  const out: Json = { ...data, details };
  if (Object.keys(details).length === 0) delete out.details;
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

/** The bridge frame's own fields. */
const DETAIL_BOOKKEEPING = ['type', 'requestId', 'liveRevisions'] as const;

// What Unreal reported beside a refusal (typedError.unrealDetail natively, the reply's `result` over stdio), less
// what the reply already says: its message, its code, an empty data, the frame's own fields. Partial results stay.
function compactDetail(detail: Json, reply: Json): Json | undefined {
  const out = omit(detail, DETAIL_BOOKKEEPING);
  // A handler that said success while the gateway refused its output keeps saying so.
  if (same(out.success, reply.success)) delete out.success;
  if (same(out.message, reply.message)) delete out.message;
  if (same(out.error, reply.message) || same(out.error, reply.errorCode)) delete out.error;
  if (isRecord(out.error) && same(out.error.message, reply.message) && same(out.error.code, reply.errorCode)) delete out.error;
  if (isRecord(out.data) && Object.keys(out.data).length === 0) delete out.data;
  if (isRecord(out.result)) {
    const inner = compactDetail(out.result, reply);
    if (inner === undefined) delete out.result;
    else out.result = inner;
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
    const detail = compactDetail(out.unrealDetail, reply);
    if (detail === undefined) delete out.unrealDetail;
    else out.unrealDetail = detail;
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
  // Over stdio a success also carried the raw handler result beside `data`, its projection; the projection folds
  // every undeclared field into data.details, so the raw copy only repeated it. A failure's `result` is its detail.
  if (out.success === true && isRecord(out.data)) delete out.result;
  if (out.success === false && isRecord(out.result)) {
    const detail = compactDetail(out.result, out);
    if (detail === undefined) delete out.result;
    else out.result = detail;
  }
  const receipt = isRecord(reply.receipt) ? receiptOutcome(reply.receipt, reply) : undefined;
  if (receipt === undefined) delete out.receipt;
  else out.receipt = receipt;
  if (isRecord(out.typedError)) out.typedError = compactTypedError(out.typedError, out);
  const listed = strings(receipt?.warnings);
  if (Array.isArray(out.warnings) && out.warnings.every((item) => typeof item === 'string' && listed.includes(item))) {
    delete out.warnings;
  }
  if (isRecord(out.data)) {
    const changes = strings(receipt?.changes);
    const data = withoutRepeatedValues(withoutListed(withoutListed(withoutListed(out.data, 'warnings', listed), 'changedAssets', changes), 'changedEntities', changes));
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

// Search-index and catalog bookkeeping the stdio contract carries (the native one never had them).
const DESCRIBE_INTERNAL_FIELDS = ['topics', 'category', 'perActionSchemas'] as const;

function compactDescribe(reply: Json): Json {
  const out = omit(reply, ['hashes', 'exampleCount']);
  if (out.scope !== 'capability' && out.scope !== 'parameter') return out;
  if (out.success === true) delete out.message;
  for (const key of DESCRIBE_INTERNAL_FIELDS) delete out[key];
  const pair = { tool: out.tool ?? out.parentTool, action: out.action };
  if (Array.isArray(out.aliases) && out.aliases.length === 0) delete out.aliases;
  if (Array.isArray(out.legacyIds) && out.legacyIds.length === 1 && same(out.legacyIds[0], pair)) delete out.legacyIds;
  if (same(out.migratedFrom, pair)) delete out.migratedFrom;
  if (Array.isArray(out.parameters) && out.parameterCount === out.parameters.length) delete out.parameterCount;
  if (out.runnable === true) delete out.runnable;
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
