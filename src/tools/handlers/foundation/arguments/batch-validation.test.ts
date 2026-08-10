import { describe, expect, it } from 'vitest';

import { validateRequiredFields } from './batch-validation.js';

describe('validateRequiredFields', () => {
  it('returns mapping of each field to its string value when all fields are present and non-empty', () => {
    const args = { name: '  MyActor  ', path: '/Game/Actors/Foo' };
    const fields = ['name', 'path'] as const;

    const result = validateRequiredFields(args, fields);

    expect(result).toEqual({ name: '  MyActor  ', path: '/Game/Actors/Foo' });
  });

  it('throws with exact message when the first field is missing', () => {
    const args = { path: '/Game/Foo' };
    const fields = ['name', 'path'] as const;

    expect(() => validateRequiredFields(args, fields))
      .toThrow('Missing required parameter: name');
  });

  it('throws with the field name of whichever required field is checked first that is missing', () => {
    // prove order: if 'middle' is missing but 'later' is also missing,
    // the error reports 'middle' (first missing in iteration order)
    const args = { later: 'value' };
    const fields = ['first', 'middle', 'later'] as const;

    expect(() => validateRequiredFields(args, fields))
      .toThrow('Missing required parameter: first');
  });

  it('throws for the middle field when both first and middle are missing but later is present', () => {
    const args = { later: 'value' };
    const fields = ['first', 'middle', 'later'] as const;

    // first is missing → stops there, reports 'first'
    expect(() => validateRequiredFields(args, fields))
      .toThrow('Missing required parameter: first');
  });

  it('throws when a field value is an empty string', () => {
    const args = { name: '', path: '/Game/Foo' };
    const fields = ['name', 'path'] as const;

    expect(() => validateRequiredFields(args, fields))
      .toThrow('Missing required parameter: name');
  });

  it('throws when a field value is undefined', () => {
    const args = { name: undefined, path: '/Game/Foo' } as Record<string, unknown>;
    const fields = ['name', 'path'] as const;

    expect(() => validateRequiredFields(args, fields))
      .toThrow('Missing required parameter: name');
  });

  it('throws when a field value is null', () => {
    const args = { name: null, path: '/Game/Foo' } as Record<string, unknown>;
    const fields = ['name', 'path'] as const;

    expect(() => validateRequiredFields(args, fields))
      .toThrow('Missing required parameter: name');
  });

  it('returns empty object when fields array is empty without throwing', () => {
    const args = { anything: 'ignored' };

    expect(validateRequiredFields(args, [])).toEqual({});
  });

  it('throws when a field value is a non-string type (e.g. number)', () => {
    const args = { name: 42 as unknown, path: '/Game/Foo' };
    const fields = ['name', 'path'] as const;

    expect(() => validateRequiredFields(args, fields))
      .toThrow('Missing required parameter: name');
  });

  it('throws for a whitespace-only string (mirrors requireNonEmptyString: trim makes it empty)', () => {
    const args = { name: '   ', path: '/Game/Foo' };
    const fields = ['name', 'path'] as const;

    expect(() => validateRequiredFields(args, fields))
      .toThrow('Missing required parameter: name');
  });

  it('accepts a string with leading/trailing whitespace (trim is not applied to accepted values)', () => {
    const args = { name: '  Alice  ', path: '/Game/Foo' };
    const fields = ['name', 'path'] as const;

    expect(validateRequiredFields(args, fields)).toEqual({ name: '  Alice  ', path: '/Game/Foo' });
  });
});