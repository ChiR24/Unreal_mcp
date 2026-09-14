import { stableJsonStringify } from '../../hashing.js';

export const sameJson = (a: unknown, b: unknown): boolean =>
  a === undefined || b === undefined ? a === b : stableJsonStringify(a) === stableJsonStringify(b);
export const unique = <T>(values: readonly T[]): T[] => [...new Set(values)];
