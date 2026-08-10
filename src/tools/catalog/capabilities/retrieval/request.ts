import { z } from 'zod';

import {
  BEHAVIOR_EFFECTS,
  EDITOR_STATES,
  POLICY_SCOPES,
} from '../constants.js';
import { LegacyToolNameSchema, UnrealVersionSchema } from '../identifiers.js';
import {
  DEFAULT_RESULT_LIMIT,
  MAX_RESULT_LIMIT,
  RETRIEVAL_TOKENIZATION,
} from './constants.js';
import type {
  CapabilityRetrievalRequest,
  CapabilityRuntimeProfile,
} from './types.js';

export const CAPABILITY_CATEGORIES = ['core', 'world', 'gameplay', 'utility'] as const;

function rejectDuplicates(values: readonly string[], context: z.RefinementCtx): void {
  const seen = new Set<string>();
  for (const [index, value] of values.entries()) {
    if (seen.has(value)) {
      context.addIssue({
        code: 'custom',
        path: [index],
        message: `duplicate value: ${value}`,
      });
      continue;
    }
    seen.add(value);
  }
}

const pluginListSchema = z.array(z.string().min(1).max(128))
  .max(128)
  .superRefine(rejectDuplicates)
  .readonly();
const parentListSchema = z.array(LegacyToolNameSchema)
  .max(64)
  .superRefine(rejectDuplicates)
  .readonly();
const categoryListSchema = z.array(z.enum(CAPABILITY_CATEGORIES))
  .max(CAPABILITY_CATEGORIES.length)
  .superRefine(rejectDuplicates)
  .readonly();
const scopeListSchema = z.array(z.enum(POLICY_SCOPES))
  .max(POLICY_SCOPES.length)
  .superRefine(rejectDuplicates)
  .readonly();
const effectListSchema = z.array(z.enum(BEHAVIOR_EFFECTS))
  .min(1)
  .max(BEHAVIOR_EFFECTS.length)
  .superRefine(rejectDuplicates)
  .readonly();
const outputFieldListSchema = z.array(z.string().min(1).max(128))
  .max(32)
  .superRefine(rejectDuplicates)
  .readonly();

export const CapabilityRuntimeProfileSchema = z.strictObject({
  unrealVersion: UnrealVersionSchema,
  installedPlugins: pluginListSchema,
  editorState: z.enum(EDITOR_STATES),
  enabledParents: parentListSchema,
  enabledCategories: categoryListSchema,
  authorizedScopes: scopeListSchema,
  requestedEffects: effectListSchema,
  requiredOutputFields: outputFieldListSchema,
}).readonly();

export const CapabilityRetrievalRequestSchema = z.strictObject({
  query: z.string().max(RETRIEVAL_TOKENIZATION.maxQueryLength),
  limit: z.number().int().min(1).max(MAX_RESULT_LIMIT).default(DEFAULT_RESULT_LIMIT),
  profile: CapabilityRuntimeProfileSchema,
}).readonly();

export function parseCapabilityRuntimeProfile(input: unknown): CapabilityRuntimeProfile {
  return CapabilityRuntimeProfileSchema.parse(input);
}

export function parseCapabilityRetrievalRequest(input: unknown): CapabilityRetrievalRequest {
  return CapabilityRetrievalRequestSchema.parse(input);
}
