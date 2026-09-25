import { z } from 'zod';

import {
  BEHAVIOR_EFFECTS,
  CAPABILITY_PROVENANCE,
  COMPENSATION_MODES,
  CONSENT_MODES,
  DATA_ACCESS_CLASSES,
  DISPATCH_MODES,
  EDITOR_STATES,
  IDEMPOTENCY_CLASSES,
  LATENCY_CLASSES,
  NORMALIZATION_CLASSES,
  NORMALIZATION_DISPOSITIONS,
  POLICY_SCOPES,
  PREVIEW_MODES,
  PREVIEW_REPORTS,
  RESOURCE_CLASSES,
  SEMANTICS_EVIDENCE_GRADES,
  UNDO_MODES
} from './constants.js';
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

export const hashesSchema = z.strictObject({
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

const evidenceSchema = z.strictObject({
  grade: z.enum(SEMANTICS_EVIDENCE_GRADES),
  citation: z.string().min(1, 'every semantics declaration must cite its evidence')
});

const semanticsShape = {
  preview: z.strictObject({
    mode: z.enum(PREVIEW_MODES),
    reports: z.array(z.enum(PREVIEW_REPORTS)),
    evidence: evidenceSchema
  }),
  undo: z.strictObject({
    mode: z.enum(UNDO_MODES),
    transactionScope: z.string().min(1).nullable(),
    evidence: evidenceSchema
  }),
  compensation: z.strictObject({
    mode: z.enum(COMPENSATION_MODES),
    inverse: z.array(CapabilityIdSchema),
    guidance: z.string().min(1).nullable(),
    evidence: evidenceSchema
  })
};

const refineSemantics = (semantics: z.infer<z.ZodObject<typeof semanticsShape>>, ctx: z.RefinementCtx): void => {
  const { preview, undo, compensation } = semantics;
  const fail = (path: readonly (string | number)[], message: string): void => {
    ctx.addIssue({ code: 'custom', path: [...path], message });
  };

  if ((preview.mode === 'none') !== (preview.reports.length === 0)) {
    fail(['preview', 'reports'], 'a preview reports what it can see, and only when it exists');
  }
  if ((undo.mode === 'transaction') !== (undo.transactionScope !== null)) {
    fail(['undo', 'transactionScope'], 'an undoable mutation must name its transaction scope');
  }
  if ((compensation.mode === 'inverse-capability') !== (compensation.inverse.length > 0)) {
    fail(['compensation', 'inverse'], 'inverse-capability compensation must name its capabilities');
  }
  if ((compensation.mode === 'manual-cleanup') !== (compensation.guidance !== null)) {
    fail(['compensation', 'guidance'], 'manual-cleanup compensation must describe the cleanup');
  }

  // A claim beyond the pessimistic value must have been earned, and vice versa.
  const pessimistic: readonly [readonly string[], boolean, string][] = [
    [['preview'], preview.mode === 'none', preview.evidence.grade],
    [['undo'], undo.mode === 'none', undo.evidence.grade],
    [['compensation'], compensation.mode === 'none', compensation.evidence.grade]
  ];
  for (const [path, isPessimisticValue, grade] of pessimistic) {
    if (isPessimisticValue !== (grade === 'pessimistic-default')) {
      fail(
        [...path, 'evidence', 'grade'],
        'evidence grade must agree with the declared mode: only a pessimistic value may be a pessimistic-default'
      );
    }
  }
};

const semanticsSchema = z.strictObject(semanticsShape).superRefine(refineSemantics);

const behaviorBaseShape = {
  effect: z.enum(BEHAVIOR_EFFECTS),
  idempotency: z.enum(IDEMPOTENCY_CLASSES),
  longRunning: z.boolean(),
  safeToRetry: z.boolean(),
  supportsPreview: z.boolean(),
  supportsUndo: z.boolean()
};

const behaviorSchema = z.strictObject({
  ...behaviorBaseShape,
  semantics: semanticsSchema.optional()
});

export const behaviorRecordSchema = z
  .strictObject({
    ...behaviorBaseShape,
    semantics: z.strictObject(semanticsShape).superRefine(refineSemantics)
  })
  .superRefine((behavior, ctx) => {
    if (behavior.supportsPreview !== (behavior.semantics.preview.mode !== 'none')) {
      ctx.addIssue({
        code: 'custom',
        path: ['supportsPreview'],
        message: 'supportsPreview must agree with semantics.preview.mode'
      });
    }
    if (behavior.supportsUndo !== (behavior.semantics.undo.mode === 'transaction')) {
      ctx.addIssue({
        code: 'custom',
        path: ['supportsUndo'],
        message: 'supportsUndo must agree with semantics.undo.mode'
      });
    }
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

const routingSchema = z.strictObject({
  parentTool: LegacyToolNameSchema,
  dispatchAction: LegacyActionNameSchema,
  dispatchMode: z.enum(DISPATCH_MODES),
  dispatchBy: dispatchBySchema.optional()
});

const normalizationSchema = z
  .strictObject({
    class: z.enum(NORMALIZATION_CLASSES),
    disposition: z.enum(NORMALIZATION_DISPOSITIONS),
    rationale: z.string(),
    aliasOf: CapabilityIdSchema.optional(),
    provenance: z.enum(CAPABILITY_PROVENANCE).optional()
  })
  .superRefine((candidate, ctx) => {
    if (candidate.aliasOf === undefined) return;
    if (candidate.disposition !== 'alias') {
      ctx.addIssue({
        code: 'custom',
        path: ['aliasOf'],
        message: 'aliasOf requires disposition "alias"'
      });
    }
  });

const legacyIdSchema = z.strictObject({
  tool: LegacyToolNameSchema,
  action: LegacyActionNameSchema,
  provenance: z.enum(CAPABILITY_PROVENANCE).optional(),
  folded: jsonObjectSchema.optional()
});

// The parent action enum and the gateway's legacy-pair index are derived from
// `legacyIds` and from nothing else, so a record declaring none would ship
// searchable and callable by canonical id while `describe` never names its
// action. The empty case is refused here rather than discovered at the gateway.
const legacyIdsSchema = z
  .array(legacyIdSchema)
  .min(1, 'a capability must declare the legacyIds pair it is reachable through');

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
  routing: routingSchema,
  normalization: normalizationSchema,
  deprecation: deprecationSchema,
  parent: parentSchema
};
