// src/tools/catalog/capabilities/records/shared/schema-props.ts
// The primitive JSON-Schema property constructors every record property map is
// written from.
//
// These four were byte-identical in control-actor/properties.ts,
// gameplay/properties.ts and gameplay/animation-physics/animation-properties.ts.
// They shape the input schemas that reach clients and that the generated
// registry is hashed from, so a divergence between copies would surface as a
// registry drift failure rather than a compile error.

import type { JsonObject } from '../../index.js';

/** A described string property. */
export const str = (desc: string): JsonObject => ({ type: 'string', description: desc });

/** A described number property. */
export const num = (desc: string): JsonObject => ({ type: 'number', description: desc });

/** A described boolean property. */
export const bool = (desc: string): JsonObject => ({ type: 'boolean', description: desc });

/** A described 3-element numeric vector (location, rotation, scale, colour). */
export const vec3 = (desc: string): JsonObject => ({
  type: 'array',
  items: { type: 'number' },
  minItems: 3,
  maxItems: 3,
  description: desc,
});
