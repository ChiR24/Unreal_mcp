import { z } from 'zod';

import { UNREAL_RELEASE_CHANNELS } from './constants.js';

const canonicalIdPattern = /^[a-z][a-z0-9_]*(?:\.[a-z][a-z0-9_]*)+$/;
const legacyNamePattern = /^[a-z][a-z0-9_]*$/;

export const CapabilityIdSchema = z.string()
  .min(3)
  .max(160)
  .regex(canonicalIdPattern, 'Expected a lower-snake-case dotted capability ID')
  .brand<'CapabilityId'>();

export const CapabilityAliasSchema = z.string()
  .min(3)
  .max(160)
  .regex(canonicalIdPattern, 'Expected a lower-snake-case dotted capability alias')
  .brand<'CapabilityAlias'>();

export const LegacyToolNameSchema = z.string()
  .min(1)
  .max(80)
  .regex(legacyNamePattern, 'Expected a lower-snake-case legacy tool name')
  .brand<'LegacyToolName'>();

export const LegacyActionNameSchema = z.string()
  .min(1)
  .max(120)
  .regex(legacyNamePattern, 'Expected a lower-snake-case legacy action name')
  .brand<'LegacyActionName'>();

export const UnrealVersionSchema = z.strictObject({
  major: z.literal(5),
  minor: z.number().int().min(0).max(99),
  patch: z.number().int().min(0).max(999),
  channel: z.enum(UNREAL_RELEASE_CHANNELS),
  preview: z.number().int().positive().max(99).optional()
}).readonly().superRefine((version, context) => {
  if (version.channel === 'preview' && version.preview === undefined) {
    context.addIssue({
      code: 'custom',
      path: ['preview'],
      message: 'Preview releases require a positive preview number'
    });
  }
  if (version.channel === 'stable' && version.preview !== undefined) {
    context.addIssue({
      code: 'custom',
      path: ['preview'],
      message: 'Stable releases cannot declare a preview number'
    });
  }
});

export type CapabilityId = z.infer<typeof CapabilityIdSchema>;
export type CapabilityAlias = z.infer<typeof CapabilityAliasSchema>;
export type LegacyToolName = z.infer<typeof LegacyToolNameSchema>;
export type LegacyActionName = z.infer<typeof LegacyActionNameSchema>;
export type UnrealVersion = z.infer<typeof UnrealVersionSchema>;
