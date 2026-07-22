// src/server/mcp-primitives/prompts/prompt-types.test.ts
// Task 32 (RED first): the prompt primitive types — branded version, the closed
// six-workflow id allowlist, and the strict typed argument kinds.

import { describe, expect, it } from 'vitest';

import {
  INITIAL_PROMPT_VERSION,
  PROMPT_ARGUMENT_KINDS,
  WORKFLOW_PROMPT_IDS,
  asPromptVersion,
  isPromptArgumentKind,
  isWorkflowPromptId,
} from './prompt-types.js';

describe('prompt-types', () => {
  it('starts versions at 1 and brands integer versions >= 1', () => {
    expect(INITIAL_PROMPT_VERSION).toBe(1);
    expect(asPromptVersion(1)).toBe(1);
    expect(asPromptVersion(7)).toBe(7);
  });

  it('rejects non-integer or sub-1 versions rather than coercing', () => {
    expect(() => asPromptVersion(0)).toThrow(RangeError);
    expect(() => asPromptVersion(-1)).toThrow(RangeError);
    expect(() => asPromptVersion(1.5)).toThrow(RangeError);
    expect(() => asPromptVersion(Number.NaN)).toThrow(RangeError);
  });

  it('exposes exactly the six user-selected workflow ids', () => {
    expect([...WORKFLOW_PROMPT_IDS]).toEqual([
      'inspect-fix',
      'asset-import',
      'level-build',
      'blueprint-edit',
      'validation',
      'sequence-render',
    ]);
    // A closed allowlist: no duplicates.
    expect(new Set(WORKFLOW_PROMPT_IDS).size).toBe(WORKFLOW_PROMPT_IDS.length);
  });

  it('narrows an arbitrary string to a workflow id via the guard', () => {
    expect(isWorkflowPromptId('sequence-render')).toBe(true);
    expect(isWorkflowPromptId('not-a-workflow')).toBe(false);
    expect(isWorkflowPromptId('')).toBe(false);
  });

  it('enumerates the strict argument kinds and narrows them', () => {
    expect(PROMPT_ARGUMENT_KINDS).toContain('content-path');
    expect(PROMPT_ARGUMENT_KINDS).toContain('object-path');
    expect(PROMPT_ARGUMENT_KINDS).toContain('identifier');
    expect(PROMPT_ARGUMENT_KINDS).toContain('enum');
    expect(PROMPT_ARGUMENT_KINDS).toContain('engine-version');
    expect(PROMPT_ARGUMENT_KINDS).toContain('text');
    expect(isPromptArgumentKind('content-path')).toBe(true);
    expect(isPromptArgumentKind('freeform')).toBe(false);
  });
});
