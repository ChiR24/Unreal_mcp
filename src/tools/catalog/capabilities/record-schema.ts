import { z } from 'zod';

import {
  BEHAVIOR_EFFECTS,
  CONSENT_MODES,
  DATA_ACCESS_CLASSES,
  DISPATCH_MODES,
  EDITOR_STATES,
  IDEMPOTENCY_CLASSES,
  LATENCY_CLASSES,
  NORMALIZATION_CLASSES,
  NORMALIZATION_DISPOSITIONS,
  POLICY_SCOPES,
  RESOURCE_CLASSES
} from './constants.js';
import { computeCapabilityHashes, readField } from './hashing.js';
import {
  CapabilityAliasSchema,
  CapabilityIdSchema,
  LegacyActionNameSchema,
  LegacyToolNameSchema,
  UnrealVersionSchema
} from './identifiers.js';
import { Draft202012ObjectSchemaSchema, jsonObjectSchema } from './json-schema.js';
import { getParentToolMetadata, type ParentToolMetadata } from './records/parent-metadata.js';
import { compareUnrealVersion } from './version.js';

const HEX64 = /^[0-9a-f]{64}$/;

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

const hashesSchema = z.strictObject({
  algorithm: z.literal('sha256'),
  schema: z.string().regex(HEX64, 'expected 64-char lowercase hex sha256'),
  content: z.string().regex(HEX64, 'expected 64-char lowercase hex sha256')
});

const deprecationSchema = z.discriminatedUnion('status', [
  z.strictObject({ status: z.literal('active') }),
  z.strictObject({
    status: z.enum(['deprecated', 'removed'] as const),
    since: z.string(),
    guidance: z.string(),
    replacement: CapabilityIdSchema.optional()
  })
]);

const discoverySchema = z.strictObject({
  domain: z.string(),
  family: z.string(),
  topics: z.array(z.string()),
  summary: z.string(),
  whenToUse: z.array(z.string()),
  whenNotToUse: z.array(z.string())
});

const exampleSchema = z.strictObject({
  title: z.string(),
  input: jsonObjectSchema,
  output: jsonObjectSchema
});

const availabilitySchema = z
  .strictObject({
    unreal: z.strictObject({ min: UnrealVersionSchema, max: UnrealVersionSchema }),
    requiredPlugins: z.array(z.string()),
    editorStates: z.array(z.enum(EDITOR_STATES))
  })
  .superRefine((availability, ctx) => {
    if (compareUnrealVersion(availability.unreal.min, availability.unreal.max) > 0) {
      ctx.addIssue({
        code: 'custom',
        path: ['unreal', 'max'],
        message: 'Unreal availability max must not precede min'
      });
    }
  });

const behaviorSchema = z.strictObject({
  effect: z.enum(BEHAVIOR_EFFECTS),
  idempotency: z.enum(IDEMPOTENCY_CLASSES),
  longRunning: z.boolean(),
  safeToRetry: z.boolean(),
  supportsPreview: z.boolean(),
  supportsUndo: z.boolean()
});

const policySchema = z.strictObject({
  requiredScope: z.enum(POLICY_SCOPES),
  consent: z.enum(CONSENT_MODES),
  dataAccess: z.enum(DATA_ACCESS_CLASSES)
});

const costSchema = z.strictObject({
  latency: z.enum(LATENCY_CLASSES),
  resources: z.enum(RESOURCE_CLASSES)
});

const routingSchema = z.strictObject({
  parentTool: LegacyToolNameSchema,
  dispatchAction: LegacyActionNameSchema,
  dispatchMode: z.enum(DISPATCH_MODES)
});

const normalizationSchema = z.strictObject({
  class: z.enum(NORMALIZATION_CLASSES),
  disposition: z.enum(NORMALIZATION_DISPOSITIONS),
  rationale: z.string()
});

const legacyIdSchema = z.strictObject({
  tool: LegacyToolNameSchema,
  action: LegacyActionNameSchema
});

// Canonical parent-tool metadata: shape-checked here, content-checked against
// the single-source lookup in records/parent-metadata.ts so descriptions and
// categories are never duplicated locally.
const parentSchema = z
  .strictObject({
    parent: LegacyToolNameSchema,
    description: z.string().min(1),
    category: z.enum(['core', 'world', 'gameplay', 'utility'])
  })
  .superRefine((candidate, ctx) => {
    let canonical: ParentToolMetadata;
    try {
      canonical = getParentToolMetadata(candidate.parent);
    } catch {
      ctx.addIssue({
        code: 'custom',
        path: ['parent'],
        message: 'parent is not one of the 23 canonical tools'
      });
      return;
    }
    if (candidate.description !== canonical.description) {
      ctx.addIssue({
        code: 'custom',
        path: ['description'],
        message: 'parent description does not match the canonical lookup'
      });
    }
    if (candidate.category !== canonical.category) {
      ctx.addIssue({
        code: 'custom',
        path: ['category'],
        message: 'parent category does not match the canonical lookup'
      });
    }
  });

const sourceShape = {
  id: CapabilityIdSchema,
  aliases: z.array(CapabilityAliasSchema),
  legacyIds: z.array(legacyIdSchema),
  discovery: discoverySchema,
  schemas: z.strictObject({
    input: Draft202012ObjectSchemaSchema,
    output: Draft202012ObjectSchemaSchema
  }),
  examples: z.array(exampleSchema),
  availability: availabilitySchema,
  behavior: behaviorSchema,
  policy: policySchema,
  cost: costSchema,
  routing: routingSchema,
  normalization: normalizationSchema,
  deprecation: deprecationSchema,
  parent: parentSchema
};

export const CapabilityRecordSourceSchema = z.strictObject(sourceShape);

const recordShape = { ...sourceShape, hashes: hashesSchema };

function verifyHashes(record: Record<string, unknown>, ctx: z.RefinementCtx): void {
  const { hashes, ...source } = record;
  const computed = computeCapabilityHashes(source);
  if (!isRecord(hashes)) {
    ctx.addIssue({
      code: 'custom',
      path: ['hashes'],
      message: 'hashes must be a JSON object'
    });
    return;
  }
  const rawSchemaHash = readField(hashes, 'schema');
  const schemaHash = typeof rawSchemaHash === 'string' ? rawSchemaHash : undefined;
  const rawContentHash = readField(hashes, 'content');
  const contentHash = typeof rawContentHash === 'string' ? rawContentHash : undefined;
  if (computed.schema !== schemaHash) {
    ctx.addIssue({
      code: 'custom',
      path: ['hashes', 'schema'],
      message: 'schema hash mismatch'
    });
  }
  if (computed.content !== contentHash) {
    ctx.addIssue({
      code: 'custom',
      path: ['hashes', 'content'],
      message: 'content hash mismatch'
    });
  }
}

export const CapabilityRecordSchema = z
  .strictObject(recordShape)
  .superRefine((record, ctx) => {
    if (!isRecord(record)) return;
    verifyHashes(record, ctx);
  });
