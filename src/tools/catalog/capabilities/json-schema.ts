import { z } from 'zod';

import { DRAFT_2020_12_SCHEMA_URI } from './constants.js';
import type { JsonValue } from './model.js';
import { isRecord } from '../../../utils/validation/type-guards.js';

const REFLECTION_MARKER = 'x-unreal-reflection-boundary';

const jsonValueSchema: z.ZodType<JsonValue> = z.lazy(() =>
  z.union([
    z.string(),
    z.number(),
    z.boolean(),
    z.null(),
    z.array(jsonValueSchema),
    z.record(z.string(), jsonValueSchema)
  ])
);
export const jsonObjectSchema = z.record(z.string(), jsonValueSchema);

function isUnboundedObject(node: Record<string, unknown>): boolean {
  if (node.type !== 'object') return false;
  const additional = node.additionalProperties;
  const unbounded = additional === true || additional === undefined;
  return unbounded && node[REFLECTION_MARKER] !== true;
}

function walk(node: unknown, path: readonly string[], ctx: z.RefinementCtx): void {
  if (typeof node === 'function' || typeof node === 'symbol') {
    ctx.addIssue({
      code: 'custom',
      path: [...path],
      message: 'JSON Schema must be JSON-compatible'
    });
    return;
  }
  if (Array.isArray(node)) {
    node.forEach((item, index) => {
      walk(item, [...path, String(index)], ctx);
    });
    return;
  }
  if (isRecord(node)) {
    if (isUnboundedObject(node)) {
      ctx.addIssue({
        code: 'custom',
        path: [...path],
        message: 'Unbounded object schema requires x-unreal-reflection-boundary: true'
      });
    }
    for (const key of Object.keys(node)) walk(node[key], [...path, key], ctx);
  }
}

export const Draft202012ObjectSchemaSchema = z
  .strictObject({
    $schema: z.literal(DRAFT_2020_12_SCHEMA_URI),
    type: z.literal('object'),
    properties: jsonObjectSchema,
    required: z.array(z.string()),
    additionalProperties: z.union([z.boolean(), jsonObjectSchema]),
    requiredOneOf: z.array(z.string()).min(1).optional()
  })
  .superRefine((schema, ctx) => {
    if (schema.requiredOneOf !== undefined) {
      schema.requiredOneOf.forEach((name, index) => {
        if (!Object.prototype.hasOwnProperty.call(schema.properties, name)) {
          ctx.addIssue({
            code: 'custom',
            path: ['requiredOneOf', index],
            message: `requiredOneOf member '${name}' must reference a declared property`
          });
        }
      });
    }
    walk(schema, [], ctx);
  });
