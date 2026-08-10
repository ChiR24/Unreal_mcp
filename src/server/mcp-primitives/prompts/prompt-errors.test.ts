// src/server/mcp-primitives/prompts/prompt-errors.test.ts
// Task 32 (RED first): typed prompt errors, the byte/length budgets, the secret
// guard, and strict per-kind argument validation. Every failure path is a typed
// PromptError so a rejected prompt is never mistaken for a rendered one.

import { describe, expect, it } from 'vitest';

import {
  MAX_ARGUMENT_LENGTH,
  MAX_PROMPT_BYTES,
  PROMPT_ERROR_CODES,
  PromptError,
  assertNotSecret,
  enforcePromptByteBudget,
  validateArgumentValue,
} from './prompt-errors.js';
import type { PromptArgumentSpec } from './prompt-types.js';

const spec = (over: Partial<PromptArgumentSpec>): PromptArgumentSpec => ({
  name: 'value',
  description: 'an argument',
  required: false,
  kind: 'text',
  example: 'x',
  ...over,
});

describe('prompt-errors', () => {
  it('exposes stable typed error codes and a named error', () => {
    expect(PROMPT_ERROR_CODES.NOT_FOUND).toBe('PROMPT_NOT_FOUND');
    expect(PROMPT_ERROR_CODES.SECRET_ARGUMENT).toBe('PROMPT_SECRET_ARGUMENT');
    expect(PROMPT_ERROR_CODES.TOO_LARGE).toBe('PROMPT_TOO_LARGE');
    const err = new PromptError(PROMPT_ERROR_CODES.NOT_FOUND, 'sequence-render', 'nope');
    expect(err).toBeInstanceOf(Error);
    expect(err.name).toBe('PromptError');
    expect(err.code).toBe('PROMPT_NOT_FOUND');
    expect(err.promptName).toBe('sequence-render');
  });

  it('pins the bounded budgets', () => {
    expect(MAX_PROMPT_BYTES).toBe(65536);
    expect(MAX_ARGUMENT_LENGTH).toBe(512);
  });

  it('rejects secret-named arguments before anything is interpolated', () => {
    for (const name of ['token', 'apiKey', 'api_key', 'password', 'secret', 'privateKey', 'bearer', 'authToken']) {
      expect(() => assertNotSecret('asset-import', name, 'whatever')).toThrow(PromptError);
      try {
        assertNotSecret('asset-import', name, 'whatever');
      } catch (e) {
        expect((e as PromptError).code).toBe('PROMPT_SECRET_ARGUMENT');
      }
    }
  });

  it('rejects secret-looking values even under an innocuous name', () => {
    expect(() => assertNotSecret('inspect-fix', 'newValue', 'Bearer abcdefghijklmnop')).toThrow(PromptError);
    expect(() => assertNotSecret('inspect-fix', 'newValue', '-----BEGIN PRIVATE KEY-----')).toThrow(PromptError);
    expect(() => assertNotSecret('inspect-fix', 'newValue', 'eyJhbGciOi.eyJzdWIiOiJ')).toThrow(PromptError);
    expect(() => assertNotSecret('inspect-fix', 'newValue', 'deadbeefdeadbeefdeadbeefdeadbeefdeadbeef')).toThrow(PromptError);
  });

  it('passes ordinary content paths through the secret guard', () => {
    expect(() => assertNotSecret('asset-import', 'destinationPath', '/Game/Imported/Rock')).not.toThrow();
    expect(() => assertNotSecret('inspect-fix', 'propertyName', 'RelativeLocation')).not.toThrow();
  });

  it('validates content-path arguments and rejects traversal and host paths', () => {
    expect(() => validateArgumentValue('asset-import', spec({ name: 'destinationPath', kind: 'content-path' }), '/Game/Imported/Rock')).not.toThrow();
    for (const bad of ['../etc/passwd', '/Game/../secret', 'C:/Windows', '/home/user/x', '/Unknown/Root']) {
      expect(() => validateArgumentValue('asset-import', spec({ name: 'destinationPath', kind: 'content-path' }), bad)).toThrow(PromptError);
    }
  });

  it('validates identifier, enum, and engine-version kinds', () => {
    expect(() => validateArgumentValue('x', spec({ kind: 'identifier' }), 'Health_1')).not.toThrow();
    expect(() => validateArgumentValue('x', spec({ kind: 'identifier' }), '1bad')).toThrow(PromptError);
    expect(() => validateArgumentValue('x', spec({ kind: 'enum', allowed: ['fbx', 'obj'] }), 'fbx')).not.toThrow();
    expect(() => validateArgumentValue('x', spec({ kind: 'enum', allowed: ['fbx', 'obj'] }), 'zip')).toThrow(PromptError);
    expect(() => validateArgumentValue('x', spec({ kind: 'engine-version' }), '5.7')).not.toThrow();
    expect(() => validateArgumentValue('x', spec({ kind: 'engine-version' }), 'five')).toThrow(PromptError);
  });

  it('rejects an over-long argument value with a typed error', () => {
    const tooLong = 'a'.repeat(MAX_ARGUMENT_LENGTH + 1);
    try {
      validateArgumentValue('x', spec({ kind: 'text' }), tooLong);
      throw new Error('expected throw');
    } catch (e) {
      expect(e).toBeInstanceOf(PromptError);
      expect((e as PromptError).code).toBe('PROMPT_ARGUMENT_TOO_LONG');
    }
  });

  it('enforces the whole-body byte budget', () => {
    expect(() => enforcePromptByteBudget('sequence-render', 'small body')).not.toThrow();
    const huge = 'x'.repeat(MAX_PROMPT_BYTES + 1);
    try {
      enforcePromptByteBudget('sequence-render', huge);
      throw new Error('expected throw');
    } catch (e) {
      expect(e).toBeInstanceOf(PromptError);
      expect((e as PromptError).code).toBe('PROMPT_TOO_LARGE');
    }
  });
});
