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
  const activePath = new WeakSet<object>();
  const depthLimit = Number.isInteger(maxDepth) && maxDepth >= 0 ? maxDepth : 10;

  function clean(value: unknown, depth: number, path: string = 'root'): unknown {
    // Prevent infinite recursion
    if (depth > depthLimit) {
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

    // Check only the active recursion path. The same object can validly appear
    // in multiple response branches (for example result.pins and top-level
    // pins); that is duplication, not a circular reference.
    if (activePath.has(value)) {
      return '[Circular Reference]';
    }

    activePath.add(value);

    try {
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
        } catch (e) {
          // Skip properties that throw errors when accessed
          log.error(`Error cleaning property ${path}.${key}`, e);
        }
      }

      return cleaned;
    } finally {
      activePath.delete(value);
    }
  }

  return clean(obj, 0) as T;
}
