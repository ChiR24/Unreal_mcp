// scripts/canonical-registry/schema-merge.ts
//
// Deterministic JSON-Schema merge for the Task-23 record-only parent
// derivation.
//
// A canonical parent ToolDefinition has no hand-authored base. Its input and
// output schemas are built as a PERMISSIVE UNION of the exact per-action
// record `schemas.input.properties` / `schemas.output.properties` across every
// record of that parent:
//   - a property that appears with a single distinct JSON-Schema shape across
//     all records keeps that exact shape (no widening);
//   - a property that appears with multiple distinct shapes becomes a
//     deterministic JSON-Schema union (`oneOf`, branches sorted by stable JSON
//     text) so every observed record contract is preserved, except that a union
//     whose every branch is a pure type constraint collapses to the equivalent
//     `type` constraint rather than a redundant `oneOf` -- a bare scalar when the
//     branches agree on one type name, otherwise the sorted `type: [...]` list;
//   - properties never observed carry no schema (they are simply absent).
//
// The action property is handled separately (a direct string enum, never anyOf
// — see parent-derivation.ts). This module therefore never touches `action`.
//
// The emitted union is consumed by cpp-schema.ts, which reads a scalar `type`
// list as a first-class TypeUnion builder call and degrades a structural
// `oneOf` to AnyValue, so native parity keeps every branch it can represent.
//
// Pure functions only; no side effects; deterministic by construction.

import type { JsonSchemaNode } from './types.js';

const stableKey = (node: JsonSchemaNode): string => JSON.stringify(node);

const SCALAR_TYPES = new Set(['string', 'number', 'integer', 'boolean', 'null']);

// A branch constrains nothing but its JSON type when `type` is its only schema
// keyword (`description` carries no constraint), so it has no properties,
// items, enum, or nested union that a flattened type list would drop.
const pureScalarTypes = (node: JsonSchemaNode): string[] | undefined => {
  const constraints = Object.keys(node).filter((key) => key !== 'description');
  if (constraints.length !== 1 || constraints[0] !== 'type') return undefined;
  const type = node.type;
  const names = typeof type === 'string'
    ? [type]
    : Array.isArray(type) ? type.map(String) : [];
  if (names.length === 0) return undefined;
  return names.every((name) => SCALAR_TYPES.has(name)) ? names : undefined;
};

// `oneOf` over pure type constraints denotes exactly `type: [...]`, and only the
// flattened form survives C++ emission (FMcpSchemaBuilder::TypeUnion), so both
// surfaces must derive from the same normalized node or parity reports a gap.
const flattenScalarUnion = (branches: readonly JsonSchemaNode[]): string[] | undefined => {
  const types = new Set<string>();
  for (const branch of branches) {
    const scalar = pureScalarTypes(branch);
    if (scalar === undefined) return undefined;
    for (const name of scalar) types.add(name);
  }
  return [...types].sort();
};

// Branches that differ only by description collapse to a single type name. JSON
// Schema reads `['string']` and `'string'` as the same constraint, but consumers
// that treat `type` as a scalar see the one-member list as an unknown type, so
// emit the bare scalar and reserve the list form for a real multi-type union.
const typeKeyword = (names: readonly string[]): string | string[] =>
  names.length === 1 ? names[0] : [...names];

const representativeDescription = (
  branches: readonly JsonSchemaNode[],
): string | undefined => branches
  .map((branch) => (typeof branch.description === 'string' ? branch.description : ''))
  .filter((description) => description.length > 0)
  .sort((left, right) => left.localeCompare(right))[0];

/**
 * Collect the per-property distinct schema shapes across a list of property
 * maps (each map is an exact record `properties` object). Returns a map of
 * property name -> ordered list of distinct schema shapes (insertion order as
 * first seen, which is deterministic because callers pass records in a fixed
 * sorted order).
 */
const collectDistinctShapes = (
  propertyMaps: ReadonlyArray<Record<string, JsonSchemaNode>>,
): Map<string, JsonSchemaNode[]> => {
  const byProp = new Map<string, JsonSchemaNode[]>();
  for (const map of propertyMaps) {
    for (const [name, shape] of Object.entries(map)) {
      const existing = byProp.get(name);
      if (existing === undefined) {
        byProp.set(name, [shape]);
        continue;
      }
      const key = stableKey(shape);
      if (!existing.some((s) => stableKey(s) === key)) {
        existing.push(shape);
      }
    }
  }
  return byProp;
};

/**
 * Build the deterministic permissive-union property object for a set of
 * per-record property maps. Conflicting shapes become a sorted `oneOf`.
 */
export const mergePropertyUnion = (
  propertyMaps: ReadonlyArray<Record<string, JsonSchemaNode>>,
): Record<string, JsonSchemaNode> => {
  const byProp = collectDistinctShapes(propertyMaps);
  const out: Record<string, JsonSchemaNode> = {};
  for (const name of [...byProp.keys()].sort()) {
    const shapes = byProp.get(name);
    if (shapes === undefined) continue;
    if (shapes.length === 1) {
      out[name] = shapes[0];
      continue;
    }
    // Multiple distinct shapes: a deterministic union. Branches are sorted by
    // stable JSON text so identical inputs always produce identical output.
    const branches = [...shapes].sort((a, b) => stableKey(a).localeCompare(stableKey(b)));
    const scalarTypes = flattenScalarUnion(branches);
    const union: JsonSchemaNode = scalarTypes === undefined
      ? { oneOf: branches }
      : { type: typeKeyword(scalarTypes) };
    // Keep a representative description (deterministically the first by text).
    const description = representativeDescription(branches);
    if (description !== undefined) {
      union.description = description;
    }
    out[name] = union;
  }
  return out;
};

/**
 * Count how many properties would become unions for a set of property maps.
 * Exposed so tests can assert the merge is deterministic and only widens where
 * records genuinely disagree.
 */
export const countConflictingProperties = (
  propertyMaps: ReadonlyArray<Record<string, JsonSchemaNode>>,
): number => {
  const byProp = collectDistinctShapes(propertyMaps);
  let conflicts = 0;
  for (const shapes of byProp.values()) {
    if (shapes.length > 1) conflicts += 1;
  }
  return conflicts;
};
