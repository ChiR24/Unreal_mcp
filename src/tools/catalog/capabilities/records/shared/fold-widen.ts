// src/tools/catalog/capabilities/records/shared/fold-widen.ts
//
// The schema algebra a fold runs when two members declare the same parameter.
// Two members declaring the same parameter keep the wider of the two
// definitions: a keyword both sides declare and agree on survives; a keyword
// present on only ONE side is dropped, so the union accepts at least what each
// member accepted and never imposes one member's constraint on its siblings'
// calls. Enums union, differing defaults drop, numeric bounds widen, nested
// properties/items recurse. On the input side a type disagreement is a real
// conflict and is refused; on the output side it widens to an untyped field.
// `description` always carries from the first side because it documents rather
// than constrains.

import type {
  CapabilityRecordSource,
  JsonObject,
  JsonValue,
} from '../../model.js';
import { hasOwn } from '../../../../../utils/validation/type-guards.js';
import { sameJson, unique } from './fold-support.js';

const isObject = (value: unknown): value is JsonObject =>
  typeof value === 'object' && value !== null && !Array.isArray(value);
const LOWER_BOUNDS = ['minimum', 'minLength', 'minItems'] as const;
const UPPER_BOUNDS = ['maximum', 'maxLength', 'maxItems'] as const;

const actionOf = (record: CapabilityRecordSource): string => String(record.legacyIds[0]?.action ?? '');

export function widen(name: string, a: JsonValue, b: JsonValue, context: string, side: 'input' | 'output'): JsonValue {
  if (sameJson(a, b)) return a;
  if (!isObject(a) || !isObject(b)) return a;
  const hasTypeA = hasOwn(a, 'type');
  const hasTypeB = hasOwn(b, 'type');
  if (hasTypeA && hasTypeB && !sameJson(a.type, b.type)) {
    if (side === 'input') {
      throw new Error(`${context}: parameter '${name}' is typed ${JSON.stringify(a.type)} and ${JSON.stringify(b.type)} by different members`);
    }
    const untyped: Record<string, JsonValue> = {};
    if (typeof a.description === 'string') untyped.description = a.description;
    return untyped;
  }
  if (hasTypeA !== hasTypeB) {
    // One member types the value, the other leaves it free: the union must
    // accept both, so the type and any constraints riding on it drop.
    const free = hasTypeA ? b : a;
    const untyped: Record<string, JsonValue> = {};
    if (typeof free.description === 'string') untyped.description = free.description;
    return untyped;
  }
  const merged: Record<string, JsonValue> = {};
  for (const key of Object.keys(a)) {
    // Recursed below; a one-sided or disagreeing instance schema drops with
    // its parent property.
    if (key === 'items' || key === 'properties' || key === 'description') continue;
    if (!hasOwn(b, key)) continue;
    if (!sameJson(a[key], b[key])) continue;
    merged[key] = a[key];
  }
  if (typeof a.description === 'string') merged.description = a.description;
  if (hasOwn(a, 'enum') && hasOwn(b, 'enum')) {
    if (Array.isArray(a.enum) && Array.isArray(b.enum)) merged.enum = unique([...a.enum, ...b.enum]);
  }
  for (const key of LOWER_BOUNDS) {
    const left = a[key];
    const right = b[key];
    if (typeof left === 'number' && typeof right === 'number') merged[key] = Math.min(left, right);
  }
  for (const key of UPPER_BOUNDS) {
    const left = a[key];
    const right = b[key];
    if (typeof left === 'number' && typeof right === 'number') merged[key] = Math.max(left, right);
  }
  const leftItems = a.items;
  const rightItems = b.items;
  if (isObject(leftItems) && isObject(rightItems)) merged.items = widen(`${name}[]`, leftItems, rightItems, context, side);
  const leftProperties = a.properties;
  const rightProperties = b.properties;
  if (isObject(leftProperties) && isObject(rightProperties)) {
    const nested: Record<string, JsonValue> = {};
    for (const [key, definition] of Object.entries(leftProperties)) {
      if (hasOwn(rightProperties, key)) {
        nested[key] = widen(`${name}.${key}`, definition, rightProperties[key] as JsonValue, context, side);
      }
    }
    merged.properties = nested;
    const leftKeys = Object.keys(leftProperties);
    const rightKeys = Object.keys(rightProperties);
    // A shared `additionalProperties: false` only describes the interior both
    // members declared. When their key sets differ the intersection above has
    // already dropped a key one member accepted, so keeping the closed interior
    // would refuse that member's own call: drop it instead.
    const sameKeySet = leftKeys.length === rightKeys.length
      && leftKeys.every((key) => hasOwn(rightProperties, key));
    if (!sameKeySet) delete merged.additionalProperties;
    if (Array.isArray(a.required) && Array.isArray(b.required)) {
      const rightRequired: readonly unknown[] = b.required;
      merged.required = a.required.filter((entry) => rightRequired.some((other) => sameJson(entry, other)));
    }
    // One-sided nested required drops: the member that omitted it must keep
    // accepting calls that leave the field out.
  }
  return merged;
}

export function mergeProperties(members: readonly CapabilityRecordSource[], side: 'input' | 'output', context: string): JsonObject {
  const merged: Record<string, JsonValue> = {};
  for (const member of members) {
    for (const [name, definition] of Object.entries(member.schemas[side].properties)) {
      if (name === 'action') continue;
      const existing = merged[name];
      merged[name] = existing === undefined
        ? definition
        : widen(name, existing, definition, `${context} (${side}, member ${actionOf(member)})`, side);
    }
  }
  return merged;
}

export function intersectRequired(members: readonly CapabilityRecordSource[], side: 'input' | 'output'): string[] {
  const [first, ...rest] = members;
  if (first === undefined) return [];
  return first.schemas[side].required.filter((name) =>
    name !== 'action' && rest.every((member) => member.schemas[side].required.includes(name)));
}
