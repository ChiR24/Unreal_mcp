import { describe, it, expect } from 'vitest';
import { extractOptionalVector } from '../../../src/tools/handlers/foundation/arguments/argument-helper.js';

// Regression cover for material.set_vector_parameter_value / material.add_vector_parameter.
//
// Before this fix both handlers read the colour with extractOptionalObject(), which returns undefined for an
// ARRAY, and then fell back to `?? { r: 1, g: 1, b: 1, a: 1 }`. The registry's own worked example for
// set_vector_parameter_value ("Tint an instance rust-orange") passes value: [0.55, 0.27, 0.1, 1], so following
// the documentation wrote opaque white and still answered success: true.
describe('extractOptionalVector', () => {
  it('accepts the documented array form and normalizes it to the bridge payload', () => {
    // The exact input from the registry example.
    expect(extractOptionalVector({ value: [0.55, 0.27, 0.1, 1] }, 'value')).toEqual({
      r: 0.55, g: 0.27, b: 0.1, a: 1,
    });
  });

  it('defaults alpha to 1 for a 3-element array', () => {
    expect(extractOptionalVector({ value: [0.2, 0.4, 0.6] }, 'value')).toEqual({
      r: 0.2, g: 0.4, b: 0.6, a: 1,
    });
  });

  it('passes an object through unchanged', () => {
    const obj = { r: 0.1, g: 0.2, b: 0.3, a: 0.4 };
    expect(extractOptionalVector({ value: obj }, 'value')).toEqual(obj);
  });

  it('returns undefined only when the key is absent, so optional callers keep their own default', () => {
    expect(extractOptionalVector({}, 'value')).toBeUndefined();
    expect(extractOptionalVector({ value: null }, 'value')).toBeUndefined();
  });

  it('THROWS on a present-but-unusable value instead of silently resolving to a default colour', () => {
    // This is the whole point: a caller-side `?? white` must never be reachable for a value the user did send.
    expect(() => extractOptionalVector({ value: 'red' }, 'value')).toThrow();
    expect(() => extractOptionalVector({ value: [1, 2] }, 'value')).toThrow();
    expect(() => extractOptionalVector({ value: [1, 2, 3, 4, 5] }, 'value')).toThrow();
    expect(() => extractOptionalVector({ value: [0.1, 'x', 0.3] }, 'value')).toThrow();
    expect(() => extractOptionalVector({ value: [0.1, NaN, 0.3] }, 'value')).toThrow();
  });
});
