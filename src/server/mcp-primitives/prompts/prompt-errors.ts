// src/server/mcp-primitives/prompts/prompt-errors.ts
// Task 32: typed prompt errors, the byte/length budgets, the secret guard, and
// strict per-kind argument validation shared by the prompt catalog. Every
// failure path converges on `PromptError` so a rejected prompt is never mistaken
// for a rendered one, and no secret or host filesystem path can be interpolated.

import type { PromptArgumentSpec } from './prompt-types.js';

/** Stable, typed error codes for the prompt surface. */
export const PROMPT_ERROR_CODES = {
  NOT_FOUND: 'PROMPT_NOT_FOUND',
  UNKNOWN_ARGUMENT: 'PROMPT_UNKNOWN_ARGUMENT',
  MISSING_ARGUMENT: 'PROMPT_MISSING_ARGUMENT',
  INVALID_ARGUMENT: 'PROMPT_INVALID_ARGUMENT',
  SECRET_ARGUMENT: 'PROMPT_SECRET_ARGUMENT',
  ARGUMENT_TOO_LONG: 'PROMPT_ARGUMENT_TOO_LONG',
  TOO_LARGE: 'PROMPT_TOO_LARGE',
  UNKNOWN_CAPABILITY: 'PROMPT_UNKNOWN_CAPABILITY',
  UNKNOWN_RESOURCE: 'PROMPT_UNKNOWN_RESOURCE',
} as const;

export type PromptErrorCode = (typeof PROMPT_ERROR_CODES)[keyof typeof PROMPT_ERROR_CODES];

/** A typed, non-executing prompt failure carrying the code and the prompt name. */
export class PromptError extends Error {
  readonly code: PromptErrorCode;
  readonly promptName: string;

  constructor(code: PromptErrorCode, promptName: string, message: string) {
    super(message);
    this.name = 'PromptError';
    this.code = code;
    this.promptName = promptName;
  }
}

/** Maximum serialized byte size for a single rendered prompt body (64 KiB). */
export const MAX_PROMPT_BYTES = 65536;

/** Maximum length of a single interpolated argument value. */
export const MAX_ARGUMENT_LENGTH = 512;

/** UE content mount roots a content/object path argument may reference. */
export const PROMPT_CONTENT_ROOTS = ['/Game', '/Engine', '/Script', '/Temp', '/Niagara'] as const;

// Argument names that name a secret. Compared against the lower-cased name.
const SECRET_NAME_PATTERN =
  /(token|secret|password|passwd|api[_-]?key|apikey|credential|private[_-]?key|privatekey|bearer|auth)/;

// Values that look like a credential: PEM blocks, bearer tokens, JWTs, long hex.
const SECRET_VALUE_PATTERN =
  /-----BEGIN|\bBearer\s+\S{8,}|\beyJ[A-Za-z0-9_-]{6,}\.[A-Za-z0-9_-]{6,}|\b[A-Fa-f0-9]{40,}\b/;

const HOST_PATH_PATTERN = /^[a-zA-Z]:[\\/]|\\|^~|^\/(?:home|users|etc|var|root|tmp)\b/i;
const IDENTIFIER_PATTERN = /^[A-Za-z_][A-Za-z0-9_]*$/;
const ENGINE_VERSION_PATTERN = /^\d+\.\d+$/;

function hasControlChar(value: string): boolean {
  for (let i = 0; i < value.length; i += 1) {
    const code = value.charCodeAt(i);
    if (code <= 0x1f || code === 0x7f) {
      return true;
    }
  }
  return false;
}

/**
 * Refuse any argument whose name or value looks like a secret. Runs before
 * strict/declared checks so an undeclared secret-named argument is reported as a
 * secret, never silently interpolated.
 */
export function assertNotSecret(promptName: string, argName: string, value: string): void {
  if (SECRET_NAME_PATTERN.test(argName.toLowerCase())) {
    throw new PromptError(
      PROMPT_ERROR_CODES.SECRET_ARGUMENT,
      promptName,
      `Argument "${argName}" names a secret; prompts never accept or interpolate secrets`,
    );
  }
  if (SECRET_VALUE_PATTERN.test(value)) {
    throw new PromptError(
      PROMPT_ERROR_CODES.SECRET_ARGUMENT,
      promptName,
      `Argument "${argName}" holds a secret-looking value; prompts never interpolate secrets`,
    );
  }
}

function assertContentPath(promptName: string, argName: string, value: string): void {
  if (value.length === 0) {
    throw new PromptError(PROMPT_ERROR_CODES.INVALID_ARGUMENT, promptName, `Argument "${argName}" is empty`);
  }
  if (hasControlChar(value)) {
    throw new PromptError(PROMPT_ERROR_CODES.INVALID_ARGUMENT, promptName, `Argument "${argName}" has control characters`);
  }
  if (HOST_PATH_PATTERN.test(value)) {
    throw new PromptError(PROMPT_ERROR_CODES.INVALID_ARGUMENT, promptName, `Argument "${argName}" is a host filesystem path`);
  }
  if (value.split('/').includes('..')) {
    throw new PromptError(PROMPT_ERROR_CODES.INVALID_ARGUMENT, promptName, `Argument "${argName}" contains path traversal`);
  }
  const underRoot = PROMPT_CONTENT_ROOTS.some((root) => value === root || value.startsWith(`${root}/`));
  if (!underRoot) {
    throw new PromptError(
      PROMPT_ERROR_CODES.INVALID_ARGUMENT,
      promptName,
      `Argument "${argName}" must resolve under a UE content root (${PROMPT_CONTENT_ROOTS.join(', ')})`,
    );
  }
}

/** Validate one declared argument value against its strict kind. */
export function validateArgumentValue(promptName: string, spec: PromptArgumentSpec, value: string): void {
  if (value.length > MAX_ARGUMENT_LENGTH) {
    throw new PromptError(
      PROMPT_ERROR_CODES.ARGUMENT_TOO_LONG,
      promptName,
      `Argument "${spec.name}" is ${value.length} chars, exceeding the ${MAX_ARGUMENT_LENGTH} limit`,
    );
  }
  switch (spec.kind) {
    case 'content-path':
      assertContentPath(promptName, spec.name, value);
      return;
    case 'object-path': {
      const [pathPart, suffix, ...rest] = value.split('.');
      if (rest.length > 0 || pathPart === undefined) {
        throw new PromptError(PROMPT_ERROR_CODES.INVALID_ARGUMENT, promptName, `Argument "${spec.name}" is not a valid object path`);
      }
      assertContentPath(promptName, spec.name, pathPart);
      if (suffix !== undefined && !IDENTIFIER_PATTERN.test(suffix)) {
        throw new PromptError(PROMPT_ERROR_CODES.INVALID_ARGUMENT, promptName, `Argument "${spec.name}" has an invalid object suffix`);
      }
      return;
    }
    case 'identifier':
      if (!IDENTIFIER_PATTERN.test(value)) {
        throw new PromptError(PROMPT_ERROR_CODES.INVALID_ARGUMENT, promptName, `Argument "${spec.name}" is not a valid identifier`);
      }
      return;
    case 'enum':
      if (!(spec.allowed ?? []).includes(value)) {
        throw new PromptError(
          PROMPT_ERROR_CODES.INVALID_ARGUMENT,
          promptName,
          `Argument "${spec.name}" must be one of: ${(spec.allowed ?? []).join(', ')}`,
        );
      }
      return;
    case 'engine-version':
      if (!ENGINE_VERSION_PATTERN.test(value)) {
        throw new PromptError(PROMPT_ERROR_CODES.INVALID_ARGUMENT, promptName, `Argument "${spec.name}" is not a MAJOR.MINOR engine version`);
      }
      return;
    case 'text':
      if (hasControlChar(value)) {
        throw new PromptError(PROMPT_ERROR_CODES.INVALID_ARGUMENT, promptName, `Argument "${spec.name}" has control characters`);
      }
      return;
    default:
      throw new PromptError(PROMPT_ERROR_CODES.INVALID_ARGUMENT, promptName, `Argument "${spec.name}" has an unknown kind`);
  }
}

/** Reject a rendered prompt body that exceeds the bounded byte budget. */
export function enforcePromptByteBudget(promptName: string, text: string): void {
  const bytes = Buffer.byteLength(text, 'utf8');
  if (bytes > MAX_PROMPT_BYTES) {
    throw new PromptError(
      PROMPT_ERROR_CODES.TOO_LARGE,
      promptName,
      `Prompt body is ${bytes} bytes, exceeding the ${MAX_PROMPT_BYTES} byte budget`,
    );
  }
}
