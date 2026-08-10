// src/server/gateway/gateway-schema-normalize.ts
// Canonicalizes the JSON-Schema `type` keyword for the gateway surface.
//
// The record-derived parent schemas express a property whose per-action shapes
// agreed on the JSON type but differed elsewhere (typically only the
// description) as a ONE-MEMBER type list, e.g. `{ type: ['string'] }`. JSON
// Schema treats that as identical to `{ type: 'string' }`, but gateway
// consumers read `type` as a scalar, so the list form reads as an unknown type
// and leaks a generator artifact into the published contract.
//
// Collapsing a one-member list to the scalar is therefore contract-preserving.
// A genuine multi-type union such as `['number', 'string']` is left untouched:
// picking one branch or widening it would misreport what the tool accepts.

import { isRecord } from '../../utils/validation/type-guards.js';

const NESTED_SCHEMA_LISTS = new Set(['oneOf', 'anyOf', 'allOf']);

function normalizeTypeKeyword(value: unknown): unknown {
  return Array.isArray(value) && value.length === 1 ? value[0] : value;
}

function normalizeBranchList(branches: readonly unknown[]): unknown[] {
  return branches.map((branch) => (isRecord(branch) ? normalizeSchemaTypes(branch) : branch));
}

function normalizeSchemaMap(schemas: Record<string, unknown>): Record<string, unknown> {
  const out: Record<string, unknown> = {};
  for (const [name, schema] of Object.entries(schemas)) {
    out[name] = isRecord(schema) ? normalizeSchemaTypes(schema) : schema;
  }
  return out;
}

/** Return `schema` with every one-member `type` list collapsed to its scalar. */
export function normalizeSchemaTypes(schema: Record<string, unknown>): Record<string, unknown> {
  const out: Record<string, unknown> = {};
  for (const [keyword, value] of Object.entries(schema)) {
    if (keyword === 'type') out[keyword] = normalizeTypeKeyword(value);
    else if (keyword === 'properties' && isRecord(value)) out[keyword] = normalizeSchemaMap(value);
    else if (keyword === 'items' && isRecord(value)) out[keyword] = normalizeSchemaTypes(value);
    else if (NESTED_SCHEMA_LISTS.has(keyword) && Array.isArray(value)) out[keyword] = normalizeBranchList(value);
    else out[keyword] = value;
  }
  return out;
}
