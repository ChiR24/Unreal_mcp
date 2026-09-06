// scripts/canonical-registry/parent-derivation.ts
//
// Record-only parent derivation for the Task-23 generator (the EXCLUSIVE
// writer).
//
// The 1,401 strict CapabilityRecords plus records/parent-metadata.ts are the
// ONLY contract/registration metadata source. This module derives the 23
// canonical parent ToolDefinitions directly from the records:
//   - name / category / description come from record `parent` metadata
//     (stamped by getParentToolMetadata() during record build, mirrored in
//     records/parent-metadata.ts);
//   - the action property is a DIRECT string enum assembled from every record
//     legacyId.action (never anyOf), in CANONICAL RECORD SEQUENCE (first seen
//     wins) so the authored action order survives generation;
//   - input properties are a deterministic permissive union of the exact
//     per-action record schemas.input.properties (excluding action);
//   - output properties are a deterministic permissive union of the exact
//     per-action record schemas.output.properties;
//   - the generated parent carries NO `params` (the runtime facade injects it);
//   - parent `required` is only `action` (no per-action required set is imposed
//     at parent level);
//   - unknown or missing parents FAIL before any write (acyclic, deterministic).
//
// No hand-authored base is consulted, so the generator never imports the
// generated parent artifact, the consolidated facade, or allToolDefinitions:
// the bootstrap stays acyclic and records are the single source of truth.

import { LEGACY_TOOL_NAME_PATTERN } from './tool-name.js';
import { mergePropertyUnion } from './schema-merge.js';
import type { CapabilityRecord } from '../../src/tools/catalog/capabilities/model.js';
import type { ToolDefinition } from '../../src/tools/definitions/shared/tool-definition.js';
import type { JsonSchemaNode } from './types.js';

// A record input schema is stamped at the record level; the canonical parent
// adds the action property itself, so we strip it from the per-record merge to
// avoid a redundant (and possibly conflicting) action shape in the union.
const stripAction = (properties: Record<string, JsonSchemaNode>): Record<string, JsonSchemaNode> => {
  const out: Record<string, JsonSchemaNode> = {};
  for (const [name, shape] of Object.entries(properties)) {
    if (name === 'action') continue;
    out[name] = shape;
  }
  return out;
};

const isRecordShape = (props: unknown): props is Record<string, JsonSchemaNode> => {
  if (typeof props !== 'object' || props === null || Array.isArray(props)) return false;
  for (const value of Object.values(props)) {
    if (typeof value !== 'object' || value === null) return false;
  }
  return true;
};

// Fail generation unless every record of a parent agrees on its parent
// metadata and the parent name is a valid legacy tool name. This is validation
// only -- it never alters the records.
const validateParentMetadata = (parent: string, records: readonly CapabilityRecord[]): void => {
  if (!LEGACY_TOOL_NAME_PATTERN.test(parent)) {
    throw new Error(`FATAL: record parentTool "${parent}" is not a valid legacy tool name.`);
  }
  const descriptions = new Set<string>();
  const categories = new Set<string>();
  for (const r of records) {
    descriptions.add(r.parent.description);
    categories.add(r.parent.category);
  }
  if (descriptions.size !== 1) {
    throw new Error(`FATAL: parent "${parent}" has ${descriptions.size} distinct descriptions across its records.`);
  }
  if (categories.size !== 1) {
    throw new Error(`FATAL: parent "${parent}" has ${categories.size} distinct categories across its records.`);
  }
};

/**
 * Derive the 23 canonical parent ToolDefinitions deterministically from the
 * capability records alone.
 *
 * Ordering contract:
 *   - the parent LIST is sorted by parent name, so which parents are emitted
 *     and in what order never depends on the record sequence;
 *   - each action enum follows the CANONICAL RECORD SEQUENCE (first seen wins),
 *     never alphabetical order;
 *   - the input/output unions are order-independent by construction
 *     (mergePropertyUnion sorts property names and oneOf branches), so record
 *     sequence changes the action enum and nothing else.
 *
 * @param records the complete, validated set of capability records (e.g. the
 *                1,401 strict records) in canonical sequence. The parent
 *                surface is taken directly from the records' routing.parentTool
 *                values; there is no base input. Callers must NOT pre-sort by
 *                id: the id-sorted view feeds the record artifacts only.
 * @returns exactly one ToolDefinition per distinct parentTool, in a stable
 *          (parent-name sorted) order, with no `params` and an action enum in
 *          first-seen record order.
 */
export const deriveParents = (
  records: readonly CapabilityRecord[],
): readonly ToolDefinition[] => {
  const byParent = new Map<string, CapabilityRecord[]>();
  for (const r of records) {
    const parent = r.routing.parentTool;
    const bucket = byParent.get(parent);
    if (bucket) bucket.push(r);
    else byParent.set(parent, [r]);
  }

  if (byParent.size === 0) {
    throw new Error('FATAL: no parent tools found in records. Refusing to generate an empty registry.');
  }

  const parents: ToolDefinition[] = [];
  for (const parent of [...byParent.keys()].sort((a, b) => a.localeCompare(b))) {
    const recs = byParent.get(parent) as CapabilityRecord[];
    validateParentMetadata(parent, recs);

    // validateParentMetadata has proven both are uniform across the bucket, so
    // any record is an equally deterministic source.
    const description = recs[0].parent.description;
    const category = recs[0].parent.category;

    // Action enum: direct string union of every record legacyId.action, in
    // first-seen record order (Set preserves insertion order). Sorting here
    // would discard the authored action order.
    const actionSet = new Set<string>();
    for (const r of recs) {
      for (const legacy of r.legacyIds) actionSet.add(legacy.action);
    }
    const actionEnum = [...actionSet];
    if (actionEnum.length === 0) {
      throw new Error(`FATAL: parent "${parent}" has no legacyId actions across its records.`);
    }

    // Permissive unions of the exact per-action input/output properties. The
    // merge sorts property names and oneOf branches, so the union is identical
    // for any record order.
    const inputMaps = recs
      .map((r) => (isRecordShape(r.schemas.input.properties) ? stripAction(r.schemas.input.properties) : {}));
    const outputMaps = recs
      .map((r) => (isRecordShape(r.schemas.output.properties) ? r.schemas.output.properties : {}));

    const inputProperties = mergePropertyUnion(inputMaps);
    const outputProperties = mergePropertyUnion(outputMaps);

    // The action property is always a direct enum (never anyOf/oneOf).
    inputProperties.action = {
      type: 'string',
      enum: actionEnum,
      description: `Action to invoke on ${parent}.`,
    };

    const inputSchema: Record<string, unknown> = {
      type: 'object',
      properties: inputProperties,
      required: ['action'],
      additionalProperties: true,
    };
    const outputSchema: Record<string, unknown> = {
      type: 'object',
      properties: outputProperties,
      additionalProperties: true,
    };

    parents.push({
      name: parent,
      description,
      category,
      inputSchema,
      outputSchema,
    });
  }

  return parents;
};

export type { Category };
type Category = 'core' | 'world' | 'gameplay' | 'utility';
