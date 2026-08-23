export function isRecord(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

// `key in object` walks the prototype chain, so `__proto__`, `constructor`,
// `toString` and every other Object.prototype member would read as "declared"
// and slip past an additionalProperties gate into dispatch. The native surface
// compares against a TMap and has no such chain, so own-key lookup is also what
// keeps both transports reporting the same code for the same payload.
export function hasOwn(target: object, key: string): boolean {
  return Object.prototype.hasOwnProperty.call(target, key);
}
