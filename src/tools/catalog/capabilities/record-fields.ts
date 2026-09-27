import { z } from 'zod';

import {
  BEHAVIOR_EFFECTS,
  CONSENT_MODES,
  DATA_ACCESS_CLASSES,
  EDITOR_STATES,
  IDEMPOTENCY_CLASSES,
  LATENCY_CLASSES,
  POLICY_SCOPES,
  RESOURCE_CLASSES,

} from './constants.js';
import {
  CapabilityAliasSchema,
  CapabilityIdSchema,
  LegacyActionNameSchema,
  LegacyToolNameSchema,
} from './identifiers.js';
import { Draft202012ObjectSchemaSchema, jsonObjectSchema } from './json-schema.js';
import { getParentToolMetadata } from './records/parent-metadata.js';

const HEX64 = /^[0-9a-f]{64}$/;

export const hashesSchema = z.strictObject({
  algorithm: z.literal('sha256'),
  schema: z.string().regex(HEX64, 'expected 64-char lowercase hex sha256'),
  content: z.string().regex(HEX64, 'expected 64-char lowercase hex sha256')
});

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

const availabilitySchema = z.strictObject({
  requiredPlugins: z.array(z.string()),
  editorStates: z.array(z.enum(EDITOR_STATES))
});

const behaviorSchema = z.strictObject({
  effect: z.enum(BEHAVIOR_EFFECTS),
  idempotency: z.enum(IDEMPOTENCY_CLASSES),
  longRunning: z.boolean(),
  safeToRetry: z.boolean(),
  compensation: z.union([
    z.strictObject({ inverse: z.array(CapabilityIdSchema).min(1) }),
    z.strictObject({ guidance: z.string().min(1) })
  ]).optional()
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

const dispatchBySchema = z.strictObject({
  param: z.string().min(1),
  actions: z.record(z.string().min(1), LegacyActionNameSchema),
  declaredBy: z.record(z.string().min(1), z.array(z.string().min(1)).min(1)).optional()
});

const isCanonicalParent = (tool: string): boolean => {
  try {
    getParentToolMetadata(tool);
    return true;
  } catch {
    return false;
  }
};

const routingSchema = z.strictObject({
  parentTool: LegacyToolNameSchema.refine(isCanonicalParent, 'parentTool is not one of the 23 canonical tools'),
  dispatchAction: LegacyActionNameSchema,
  dispatchBy: dispatchBySchema.optional()
});

const legacyIdSchema = z.strictObject({
  tool: LegacyToolNameSchema,
  action: LegacyActionNameSchema,
  folded: jsonObjectSchema.optional()
});

// The parent action enum and the gateway's legacy-pair index are derived from
// `legacyIds` and from nothing else, so a record declaring none would ship
// searchable and callable by canonical id while `describe` never names its
// action. The empty case is refused here rather than discovered at the gateway.
const legacyIdsSchema = z
  .array(legacyIdSchema)
  .min(1, 'a capability must declare the legacyIds pair it is reachable through');

export const sourceShape = {
  id: CapabilityIdSchema,
  aliases: z.array(CapabilityAliasSchema),
  legacyIds: legacyIdsSchema,
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
  routing: routingSchema
};
