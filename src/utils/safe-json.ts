// Utility to safely serialize objects with circular references
export function safeStringify(obj: any, space?: number): string {
  const seen = new WeakSet();
  
  return JSON.stringify(obj, (key, value) => {
    // Handle undefined, functions, symbols
    if (value === undefined || typeof value === 'function' || typeof value === 'symbol') {
      return undefined;
    }
    
    // Handle circular references
    if (typeof value === 'object' && value !== null) {
      if (seen.has(value)) {
        return '[Circular Reference]';
      }
      seen.add(value);
    }
    
    // Handle BigInt
    if (typeof value === 'bigint') {
      return value.toString();
    }
    
    return value;
  }, space);
}

export function sanitizeResponse(response: any): any {
  if (!response || typeof response !== 'object') {
    return response;
  }
  
  // Create a clean copy without circular references
  try {
    const jsonStr = safeStringify(response);
    return JSON.parse(jsonStr);
  } catch (error) {
    console.error('Failed to sanitize response:', error);
    
    // Fallback: return a simple error response
    return {
      content: [{
        type: 'text',
        text: 'Response contained unserializable data'
      }],
      error: 'Serialization error'
    };
  }
}

// Remove circular references and non-serializable properties from an object
export function cleanObject(obj: any, maxDepth: number = 10): any {
  const seen = new WeakSet();
  
  function clean(value: any, depth: number, path: string = 'root'): any {
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
    
    // Check for circular reference
    if (seen.has(value)) {
      return '[Circular Reference]';
    }
    
    seen.add(value);
    
    // Handle arrays
    if (Array.isArray(value)) {
      const result = value.map((item, index) => clean(item, depth + 1, `${path}[${index}]`));
      seen.delete(value); // Remove from seen after processing
      return result;
    }
    
    // Handle objects
    const cleaned: any = {};
    
    // Use Object.keys to avoid prototype properties
    const keys = Object.keys(value);
    for (const key of keys) {
      try {
        const cleanedValue = clean(value[key], depth + 1, `${path}.${key}`);
        if (cleanedValue !== undefined) {
          cleaned[key] = cleanedValue;
        }
      } catch (e) {
        // Skip properties that throw errors when accessed
        console.error(`Error cleaning property ${path}.${key}:`, e);
      }
    }
    
    seen.delete(value); // Remove from seen after processing
    return cleaned;
  }
  
  return clean(obj, 0);
}
