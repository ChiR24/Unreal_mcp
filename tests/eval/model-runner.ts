// tests/eval/model-runner.ts
// The OPTIONAL real-model arm of Task 48.
//
// Two structural guarantees, not conventions:
//   1. This module performs no network I/O at all. It cannot: there is no HTTP
//      client here. A real run must inject a `ModelInvoker`. Importing or
//      running the offline gate therefore cannot reach a network, which is why
//      "no network by default" needs no flag to be true.
//   2. The API key is never read into the report. The runner records the NAME
//      of the environment variable and whether it was populated, so the report
//      proves the run was authenticated without carrying the secret.
//
// Without configuration the result is `BLOCKED_EXTERNAL`. It is never a pass,
// never a fail, and never a fabricated accuracy number.

import { corpus } from './corpus.js';
import { canonicalIdFor, GATEWAY_DEFAULT_SEARCH_LIMIT, retrievalCases } from './fixtures.js';
import { sha256Canonical } from './hash.js';

export const MODEL_RUNNER_CLIENT = 'model-runner' as const;
export const MODEL_RUNNER_CLIENT_VERSION = '1.0.0' as const;

export type ModelBlockReason =
  | 'NOT_ENABLED'
  | 'MISSING_PROVIDER'
  | 'MISSING_MODEL_ID'
  | 'MISSING_API_KEY_ENV'
  | 'API_KEY_NOT_SET'
  | 'NO_INVOKER_SUPPLIED';

export type ModelConfig = {
  readonly provider: string;
  readonly modelId: string;
  readonly temperature: number;
  readonly apiKeyEnvVar: string;
  readonly baseUrl: string | null;
};

export type ModelConfigResolution =
  | { readonly kind: 'configured'; readonly config: ModelConfig }
  | { readonly kind: 'blocked'; readonly reason: ModelBlockReason; readonly detail: string };

export type ModelEnv = Readonly<Record<string, string | undefined>>;

/** Candidate capability IDs the model proposed for one corpus intent, best first. */
export type ModelInvoker = (input: {
  readonly caseId: string;
  readonly intent: string;
  readonly limit: number;
}) => readonly string[];

export type ModelCaseResult = {
  readonly caseId: string;
  readonly rankedCapabilityIds: readonly string[];
  readonly top1Correct: boolean;
  readonly recallCorrect: boolean;
};

export type ModelReport =
  | {
      readonly status: 'BLOCKED_EXTERNAL';
      readonly reason: ModelBlockReason;
      readonly detail: string;
      readonly declaredEnvVars: readonly string[];
    }
  | {
      readonly status: 'EVALUATED';
      readonly client: typeof MODEL_RUNNER_CLIENT;
      readonly clientVersion: typeof MODEL_RUNNER_CLIENT_VERSION;
      readonly provider: string;
      readonly modelId: string;
      readonly temperature: number;
      readonly apiKeyEnvVar: string;
      readonly apiKeyPresent: boolean;
      readonly baseUrl: string | null;
      readonly corpusHash: string;
      readonly corpusVersion: string;
      readonly evaluatedCases: number;
      readonly top1Accuracy: number;
      readonly topKRecall: number;
      readonly perCase: readonly ModelCaseResult[];
    };

export const MODEL_ENV_VARS: readonly string[] = Object.freeze([
  'TASK48_MODEL_ENABLE',
  'TASK48_MODEL_PROVIDER',
  'TASK48_MODEL_ID',
  'TASK48_MODEL_API_KEY_ENV',
  'TASK48_MODEL_TEMPERATURE',
  'TASK48_MODEL_BASE_URL',
]);

export function resolveModelConfig(env: ModelEnv): ModelConfigResolution {
  if (env.TASK48_MODEL_ENABLE !== '1') {
    return {
      kind: 'blocked',
      reason: 'NOT_ENABLED',
      detail: 'TASK48_MODEL_ENABLE is not "1"; the external model arm is opt-in.',
    };
  }
  const provider = env.TASK48_MODEL_PROVIDER;
  if (provider === undefined || provider.trim().length === 0) {
    return { kind: 'blocked', reason: 'MISSING_PROVIDER', detail: 'TASK48_MODEL_PROVIDER is unset.' };
  }
  const modelId = env.TASK48_MODEL_ID;
  if (modelId === undefined || modelId.trim().length === 0) {
    return { kind: 'blocked', reason: 'MISSING_MODEL_ID', detail: 'TASK48_MODEL_ID is unset.' };
  }
  const apiKeyEnvVar = env.TASK48_MODEL_API_KEY_ENV;
  if (apiKeyEnvVar === undefined || apiKeyEnvVar.trim().length === 0) {
    return {
      kind: 'blocked',
      reason: 'MISSING_API_KEY_ENV',
      detail: 'TASK48_MODEL_API_KEY_ENV must name the variable holding the key.',
    };
  }
  const key = env[apiKeyEnvVar.trim()];
  if (key === undefined || key.length === 0) {
    return {
      kind: 'blocked',
      reason: 'API_KEY_NOT_SET',
      detail: `${apiKeyEnvVar.trim()} names no populated credential.`,
    };
  }
  const rawTemperature = Number(env.TASK48_MODEL_TEMPERATURE ?? '0');
  return {
    kind: 'configured',
    config: {
      provider: provider.trim(),
      modelId: modelId.trim(),
      temperature: Number.isFinite(rawTemperature) ? rawTemperature : 0,
      apiKeyEnvVar: apiKeyEnvVar.trim(),
      baseUrl: env.TASK48_MODEL_BASE_URL?.trim() ?? null,
    },
  };
}

function corpusFingerprint(): string {
  const normalized = [...corpus.cases]
    .map((entry) => ({ id: entry.id, intent: entry.intent, expected: entry.expected }))
    .sort((left, right) => (left.id < right.id ? -1 : 1));
  return sha256Canonical({ version: corpus.version, cases: normalized });
}

export function runModelEvaluation(
  env: ModelEnv,
  invoke?: ModelInvoker,
  limit: number = GATEWAY_DEFAULT_SEARCH_LIMIT,
): ModelReport {
  const resolution = resolveModelConfig(env);
  if (resolution.kind === 'blocked') {
    return {
      status: 'BLOCKED_EXTERNAL',
      reason: resolution.reason,
      detail: resolution.detail,
      declaredEnvVars: MODEL_ENV_VARS,
    };
  }
  if (invoke === undefined) {
    return {
      status: 'BLOCKED_EXTERNAL',
      reason: 'NO_INVOKER_SUPPLIED',
      detail: 'Model configured but no ModelInvoker transport was supplied; this module performs no network I/O itself.',
      declaredEnvVars: MODEL_ENV_VARS,
    };
  }
  const cases = retrievalCases();
  const perCase: ModelCaseResult[] = cases.map((entry) => {
    const ranked = invoke({ caseId: entry.id, intent: entry.intent, limit }).slice(0, limit);
    const top = ranked[0];
    return {
      caseId: entry.id,
      rankedCapabilityIds: Object.freeze([...ranked]),
      top1Correct: top !== undefined && entry.acceptedCapabilityIds.includes(top),
      recallCorrect: ranked.includes(entry.expectedCapabilityId),
    };
  });
  const total = perCase.length === 0 ? 1 : perCase.length;
  const config = resolution.config;
  return {
    status: 'EVALUATED',
    client: MODEL_RUNNER_CLIENT,
    clientVersion: MODEL_RUNNER_CLIENT_VERSION,
    provider: config.provider,
    modelId: config.modelId,
    temperature: config.temperature,
    apiKeyEnvVar: config.apiKeyEnvVar,
    apiKeyPresent: true,
    baseUrl: config.baseUrl,
    corpusHash: corpusFingerprint(),
    corpusVersion: corpus.version,
    evaluatedCases: perCase.length,
    top1Accuracy: perCase.filter((entry) => entry.top1Correct).length / total,
    topKRecall: perCase.filter((entry) => entry.recallCorrect).length / total,
    perCase: Object.freeze(perCase),
  };
}

/** Guard used by the gate: a model report must never carry a live secret. */
export function reportLeaksSecret(report: ModelReport, secrets: readonly string[]): boolean {
  const serialized = JSON.stringify(report);
  return secrets.some((secret) => secret.length > 0 && serialized.includes(secret));
}

export { canonicalIdFor };
