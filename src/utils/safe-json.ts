import { Logger } from './logger.js';

// Module-level logger to avoid creating new instances on every call
const log = new Logger('safe-json');

/**
 * Remove circular references and non-serializable properties from an object.
 * @param obj - The object to clean
 * @param maxDepth - Maximum recursion depth (default: 10)
 * @returns Cleaned object safe for JSON serialization
 */
export function cleanObject<T = unknown>(obj: T, maxDepth: number = 10): T {
  const seen = new WeakSet<object>();

  function clean(value: unknown, depth: number, path: string = 'root'): unknown {
    // Prevent infinite recursion
    if (depth > maxDepth) {
      return '[Max depth reached]';
    }

    // Handle primitives
    if (value === null || value === undefined) {
      return value;
    }

    if (typeof value !== 'object') {
      if (typeof value === 'function' || typeof value === 'symbol') {
        return undefined;
      }
      if (typeof value === 'bigint') {
        return value.toString();
      }
      return value;
    }

    // Check for circular reference - keep in set permanently for this call
    // This prevents the same object from appearing in multiple branches
    if (seen.has(value)) {
      return '[Circular Reference]';
    }

    seen.add(value);

    // Handle arrays
    if (Array.isArray(value)) {
      return value.map((item, index) => clean(item, depth + 1, `${path}[${index}]`));
    }

    // Handle objects
    const cleaned: Record<string, unknown> = {};

    // Use Object.keys to avoid prototype properties
    const keys = Object.keys(value as object);
    for (const key of keys) {
      try {
        const cleanedValue = clean((value as Record<string, unknown>)[key], depth + 1, `${path}.${key}`);
        if (cleanedValue !== undefined) {
          cleaned[key] = cleanedValue;
        }
      } catch (e: unknown) {
        // Skip properties that throw errors when accessed
        log.error(`Error cleaning property ${path}.${key}`, e);
      }
    }

    return cleaned;
  }

  return clean(obj, 0) as T;
}
