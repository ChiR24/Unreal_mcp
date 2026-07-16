// tests/eval/errors.ts
// Typed domain errors for the evaluation corpus/scorer boundary.

export class CorpusValidationError extends Error {
  readonly name = 'CorpusValidationError' as const;
  constructor(readonly reason: string, readonly detail?: unknown) {
    super(`corpus validation failed: ${reason}`);
  }
}

export class ManifestError extends Error {
  readonly name = 'ManifestError' as const;
  constructor(readonly reason: string, readonly detail?: unknown) {
    super(`manifest error: ${reason}`);
  }
}
