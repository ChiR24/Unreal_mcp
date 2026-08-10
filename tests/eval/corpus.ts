// tests/eval/corpus.ts
// Parse and validate the evaluation corpus. Validation is fail-closed: any
// malformed shape, duplicate id, unknown field, or reference to a capability
// not present in the gateway manifest throws a typed CorpusValidationError.
// This is what makes corpus/manifest drift and schema drift detectable.

import { rawCorpus } from './corpus.data.js';
import { CorpusValidationError } from './errors.js';
import { hasCapability } from './manifest-model.js';
import {
  type CapabilityRef,
  type Corpus,
  type CorpusCase,
  type CorpusCaseKind,
  type ElicitationExpectation,
  EVAL_SCHEMA,
  type ManifestModel,
  type ModelRanking,
  REQUIRED_COLLISION_IDS,
} from './types.js';

const CASE_KINDS: readonly CorpusCaseKind[] = [
  'exact', 'high_cardinality', 'ambiguous', 'destructive',
  'version_negative', 'plugin_negative', 'collision', 'near_tie_destructive',
];

const ELICITATIONS: readonly ElicitationExpectation[] = [
  'none', 'confirm', 'clarify', 'unavailable', 'ambiguous',
];

const ALLOWED_CASE_FIELDS: ReadonlySet<string> = new Set([
  'id', 'intent', 'kind', 'expected', 'requiredParams', 'allowedAlternatives',
  'elicitationExpectation', 'tokenBudget', 'collisionId', 'dispositionNote',
  'modelRankingOverride', 'firstAttemptParams', 'secondAttemptParams',
  'unavailableTool', 'negativeReason',
]);

function isObject(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

function isStringArray(value: unknown): value is readonly string[] {
  return Array.isArray(value) && value.every((v) => typeof v === 'string');
}

function isCapabilityRef(value: unknown): value is CapabilityRef {
  if (!isObject(value)) return false;
  return typeof value.tool === 'string' && typeof value.action === 'string';
}

function isModelRanking(value: unknown): value is ModelRanking {
  if (!Array.isArray(value)) return false;
  for (const entry of value) {
    if (!isObject(entry)) return false;
    if (typeof entry.tool !== 'string') return false;
    if (typeof entry.action !== 'string') return false;
    if (typeof entry.score !== 'number') return false;
  }
  return true;
}

export function parseCorpus(input: unknown): Corpus {
  if (!isObject(input)) {
    throw new CorpusValidationError('corpus root must be an object');
  }
  if (input.schema !== EVAL_SCHEMA) {
    throw new CorpusValidationError(`corpus schema must be ${EVAL_SCHEMA}`, input.schema);
  }
  if (typeof input.version !== 'string') {
    throw new CorpusValidationError('corpus version must be a string');
  }
  if (!Array.isArray(input.cases)) {
    throw new CorpusValidationError('corpus cases must be an array');
  }
  const seenIds = new Set<string>();
  const cases: CorpusCase[] = [];
  for (const raw of input.cases as unknown[]) {
    if (!isObject(raw)) {
      throw new CorpusValidationError('corpus case must be an object');
    }
    for (const key of Object.keys(raw)) {
      if (!ALLOWED_CASE_FIELDS.has(key)) {
        throw new CorpusValidationError('unknown corpus case field', key);
      }
    }
    const id = raw.id;
    if (typeof id !== 'string' || id.length === 0) {
      throw new CorpusValidationError('corpus case id must be a non-empty string');
    }
    if (seenIds.has(id)) {
      throw new CorpusValidationError('duplicate corpus case id', id);
    }
    seenIds.add(id);
    const kind = raw.kind;
    if (!CASE_KINDS.includes(kind as CorpusCaseKind)) {
      throw new CorpusValidationError('invalid case kind', kind);
    }
    if (typeof raw.intent !== 'string') {
      throw new CorpusValidationError('case intent must be a string', id);
    }
    if (!isCapabilityRef(raw.expected)) {
      throw new CorpusValidationError('case expected must be a capability ref', id);
    }
    if (!isStringArray(raw.requiredParams)) {
      throw new CorpusValidationError('case requiredParams must be string[]', id);
    }
    if (!Array.isArray(raw.allowedAlternatives) || !raw.allowedAlternatives.every(isCapabilityRef)) {
      throw new CorpusValidationError('case allowedAlternatives must be capability ref[]', id);
    }
    const elicitation = raw.elicitationExpectation;
    if (!ELICITATIONS.includes(elicitation as ElicitationExpectation)) {
      throw new CorpusValidationError('invalid elicitation expectation', elicitation);
    }
    if (typeof raw.tokenBudget !== 'number' || raw.tokenBudget < 0) {
      throw new CorpusValidationError('case tokenBudget must be a non-negative number', id);
    }
    if (raw.collisionId !== undefined && typeof raw.collisionId !== 'string') {
      throw new CorpusValidationError('case collisionId must be a string', id);
    }
    if (raw.modelRankingOverride !== undefined && !isModelRanking(raw.modelRankingOverride)) {
      throw new CorpusValidationError('case modelRankingOverride must be ranking', id);
    }
    if (raw.firstAttemptParams !== undefined && !isStringArray(raw.firstAttemptParams)) {
      throw new CorpusValidationError('case firstAttemptParams must be string[]', id);
    }
    if (raw.secondAttemptParams !== undefined && !isStringArray(raw.secondAttemptParams)) {
      throw new CorpusValidationError('case secondAttemptParams must be string[]', id);
    }
    if (raw.unavailableTool !== undefined && typeof raw.unavailableTool !== 'string') {
      throw new CorpusValidationError('case unavailableTool must be a string', id);
    }
    cases.push({
      id,
      intent: raw.intent as string,
      kind: kind as CorpusCaseKind,
      expected: raw.expected as CapabilityRef,
      requiredParams: raw.requiredParams as readonly string[],
      allowedAlternatives: raw.allowedAlternatives as readonly CapabilityRef[],
      elicitationExpectation: elicitation as ElicitationExpectation,
      tokenBudget: raw.tokenBudget as number,
      collisionId: raw.collisionId as string | undefined,
      dispositionNote: raw.dispositionNote as string | undefined,
      modelRankingOverride: raw.modelRankingOverride as ModelRanking | undefined,
      firstAttemptParams: raw.firstAttemptParams as readonly string[] | undefined,
      secondAttemptParams: raw.secondAttemptParams as readonly string[] | undefined,
      unavailableTool: raw.unavailableTool as string | undefined,
      negativeReason: raw.negativeReason as string | undefined,
    });
  }
  return { schema: EVAL_SCHEMA, version: input.version as string, cases };
}

export function validateCorpus(corpus: Corpus, manifest: ManifestModel): void {
  for (const entry of corpus.cases) {
    if (!hasCapability(manifest, entry.expected)) {
      throw new CorpusValidationError('expected capability not in manifest', {
        caseId: entry.id,
        expected: entry.expected,
      });
    }
    for (const alt of entry.allowedAlternatives) {
      if (!hasCapability(manifest, alt)) {
        throw new CorpusValidationError('allowed alternative not in manifest', {
          caseId: entry.id,
          alt,
        });
      }
    }
    if (entry.modelRankingOverride !== undefined) {
      for (const cand of entry.modelRankingOverride) {
        if (!hasCapability(manifest, { tool: cand.tool, action: cand.action })) {
          throw new CorpusValidationError('override candidate not in manifest', {
            caseId: entry.id,
            candidate: cand,
          });
        }
      }
    }
  }
}

export function assertFullParentCoverage(corpus: Corpus, manifest: ManifestModel): void {
  const covered = new Set<string>();
  for (const entry of corpus.cases) {
    covered.add(entry.expected.tool);
  }
  const missing = manifest.tools.filter((tool) => !covered.has(tool.name)).map((t) => t.name);
  if (missing.length > 0) {
    throw new CorpusValidationError('incomplete parent coverage', missing);
  }
}

export function assertCollisionCoverage(corpus: Corpus): readonly string[] {
  const present = new Set<string>();
  for (const entry of corpus.cases) {
    if (entry.collisionId !== undefined) present.add(entry.collisionId);
  }
  const missing = REQUIRED_COLLISION_IDS.filter((id) => !present.has(id));
  if (missing.length > 0) {
    throw new CorpusValidationError('incomplete collision coverage', missing);
  }
  return REQUIRED_COLLISION_IDS.filter((id) => present.has(id));
}

export const corpus: Corpus = parseCorpus(rawCorpus);
