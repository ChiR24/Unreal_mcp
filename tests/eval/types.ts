// tests/eval/types.ts
// Strict, readonly types and shared constants for the Task-4 golden discovery /
// model-call evaluation corpus and deterministic offline scorer.
//
// Everything that crosses into the scorer is plain internal data (no untrusted
// boundary), so we use `readonly` type aliases rather than Zod. External file
// input (manifest JSON, raw corpus JSON) is narrowed at the load boundary.

export type CapabilityRef = {
  readonly tool: string;
  readonly action: string;
};

export type CorpusCaseKind =
  | 'exact'
  | 'high_cardinality'
  | 'ambiguous'
  | 'destructive'
  | 'version_negative'
  | 'plugin_negative'
  | 'collision'
  | 'near_tie_destructive';

export type ElicitationExpectation =
  | 'none'
  | 'confirm'
  | 'clarify'
  | 'unavailable'
  | 'ambiguous';

export type RankedCandidate = {
  readonly tool: string;
  readonly action: string;
  readonly score: number;
};

export type ModelRanking = readonly RankedCandidate[];

export type CorpusCase = {
  readonly id: string;
  readonly intent: string;
  readonly kind: CorpusCaseKind;
  readonly expected: CapabilityRef;
  readonly requiredParams: readonly string[];
  readonly allowedAlternatives: readonly CapabilityRef[];
  readonly elicitationExpectation: ElicitationExpectation;
  readonly tokenBudget: number;
  readonly collisionId?: string;
  readonly dispositionNote?: string;
  readonly modelRankingOverride?: ModelRanking;
  readonly firstAttemptParams?: readonly string[];
  readonly secondAttemptParams?: readonly string[];
  readonly unavailableTool?: string;
  readonly negativeReason?: string;
};

export type Corpus = {
  readonly schema: 'omo.eval.corpus.v1';
  readonly version: string;
  readonly cases: readonly CorpusCase[];
};

export type ManifestTool = {
  readonly name: string;
  readonly category: string | null;
  readonly description: string;
  readonly actions: readonly string[];
  readonly parameterNames: readonly string[];
};

export type ManifestModel = {
  readonly version: number;
  readonly source: string;
  readonly tools: readonly ManifestTool[];
};

export type CaseScore = {
  readonly id: string;
  readonly kind: CorpusCaseKind;
  readonly top1Correct: boolean;
  readonly topKCorrect: boolean;
  readonly selected: CapabilityRef | null;
  readonly expectedInTiedSet: boolean;
  readonly paramCompleteness: boolean;
  readonly guidedRecovery: boolean | null;
  readonly unavailableFiltered: boolean | null;
  readonly ambiguityHandled: boolean | null;
  readonly elicitation: ElicitationExpectation;
  readonly autoSelected: boolean;
  readonly disclosureBytes: number;
  readonly estimatedTokens: number;
  readonly tokenBudgetCompliance: boolean;
};

export type EvalMetrics = {
  readonly totalCases: number;
  readonly top1Accuracy: number;
  readonly topKAccuracy: number;
  readonly paramCompletenessRate: number;
  readonly guidedRecoveryRate: number;
  readonly unavailableFilterRate: number;
  readonly ambiguityHandledRate: number;
  readonly tokenBudgetComplianceRate: number;
  readonly parentCoverage: readonly string[];
  readonly collisionCoverage: readonly string[];
};

export type DisclosureMetrics = {
  readonly totalBytes: number;
  readonly maxBytes: number;
  readonly totalTokens: number;
  readonly maxTokens: number;
};

export type ScoreReport = {
  readonly schema: 'omo.eval.report.v1';
  readonly corpusHash: string;
  readonly reportHash: string;
  readonly manifestSource: string;
  readonly manifestToolCount: number;
  readonly metrics: EvalMetrics;
  readonly disclosure: DisclosureMetrics;
  readonly perCase: readonly CaseScore[];
};

export const EVAL_SCHEMA = 'omo.eval.corpus.v1' as const;
export const REPORT_SCHEMA = 'omo.eval.report.v1' as const;
export const TOP_K = 5 as const;
export const NEAR_TIE_RATIO = 0.02 as const;
export const EPSILON = 1e-9 as const;
export const TOKEN_BYTES_PER_TOKEN = 4 as const;
export const REQUIRED_COLLISION_IDS: readonly string[] = [
  'C7', 'C8', 'C9', 'C10', 'C11', 'C13', 'C16', 'C17', 'C18', 'C19',
  'C20', 'C22', 'C23', 'C25', 'C26', 'C27', 'C30', 'C32', 'C33', 'C34',
  'C35', 'C39', 'C40', 'C41',
];
