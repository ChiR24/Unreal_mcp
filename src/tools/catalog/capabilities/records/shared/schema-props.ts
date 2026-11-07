// src/tools/catalog/capabilities/records/shared/schema-props.ts
// The primitive JSON-Schema property constructors every record property map is
// written from.
//
// These four were byte-identical in control-actor/properties.ts,
// gameplay/properties.ts and gameplay/animation-physics/animation-properties.ts.
// They shape the input schemas that reach clients and that the generated
// registry is hashed from, so a divergence between copies would surface as a
// registry drift failure rather than a compile error.

import { CODE_MAX_LENGTH, PROSE_MAX_LENGTH } from '../../constants.js';
import type { JsonObject } from '../../model.js';

/** A described string property: execute holds it to DEFAULT_STRING_MAX_LENGTH, the most an FName holds. */
export const str = (desc: string): JsonObject => ({ type: 'string', description: desc });

/** A string that carries code or a script, so it may run past the default length. */
export const codeStr = (desc: string): JsonObject => ({ type: 'string', maxLength: CODE_MAX_LENGTH, description: desc });

/** A string that carries prose (a description, comment, tooltip, the words a text shows), past the default length. */
export const proseStr = (desc: string): JsonObject => ({ type: 'string', maxLength: PROSE_MAX_LENGTH, description: desc });

/** A described number property. */
export const num = (desc: string): JsonObject => ({ type: 'number', description: desc });

/** A described boolean property. */
export const bool = (desc: string): JsonObject => ({ type: 'boolean', description: desc });

/** A described integer property. */
export const int = (desc: string): JsonObject => ({ type: 'integer', description: desc });

/** A described array of strings. */
export const strArr = (itemDesc: string, desc: string): JsonObject => ({ type: 'array', items: str(itemDesc), description: desc });

/** A described 3-element numeric vector (location, rotation, scale, colour). */
export const vec3 = (desc: string): JsonObject => ({
  type: 'array',
  items: { type: 'number' },
  minItems: 3,
  maxItems: 3,
  description: desc,
});

/** A described scale: one uniform number or exactly three numbers (the gateway turns {x, y, z} into the array). */
export const scale3 = (desc: string): JsonObject => ({ ...vec3(desc), type: ['number', 'array'] });

/** A described colour {r, g, b, a} (the gateway turns [r, g, b] or [r, g, b, a] into the object). */
export const rgba = (desc: string): JsonObject => ({
  type: 'object',
  properties: { r: num('Red.'), g: num('Green.'), b: num('Blue.'), a: num('Alpha.') },
  additionalProperties: false,
  description: desc,
});
