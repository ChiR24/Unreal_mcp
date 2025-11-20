import { coerceString } from '../../utils/result-helpers.js';

// Helper utilities extracted from the large BlueprintTools implementation
// to keep the main file more focused. These are pure helpers and safe to
// reuse across the class methods.

export type TransformInput = {
  location?: unknown;
  rotation?: unknown;
  scale?: unknown;
};

export function toFiniteNumber(raw: unknown): number | undefined {
  if (typeof raw === 'number' && Number.isFinite(raw)) return raw;
  if (typeof raw === 'string') {
    const trimmed = raw.trim();
    if (trimmed.length === 0) return undefined;
    const parsed = Number(trimmed);
    if (Number.isFinite(parsed)) return parsed;
  }
  return undefined;
}

export function normalizePartialVector(value: unknown, alternateKeys: string[] = ['x', 'y', 'z']): Record<string, number> | undefined {
  if (value === undefined || value === null) {
    return undefined;
  }

  const result: Record<string, number> = {};

  const assignIfPresent = (component: 'x' | 'y' | 'z', raw: unknown) => {
    const numberValue = toFiniteNumber(raw);
    if (numberValue !== undefined) {
      result[component] = numberValue;
    }
  };

  if (Array.isArray(value)) {
    if (value.length > 0) assignIfPresent('x', value[0]);
    if (value.length > 1) assignIfPresent('y', value[1]);
    if (value.length > 2) assignIfPresent('z', value[2]);
  } else if (typeof value === 'object') {
    const obj = value as Record<string, unknown>;
    assignIfPresent('x', obj.x ?? obj[alternateKeys[0]]);
    assignIfPresent('y', obj.y ?? obj[alternateKeys[1]]);
    assignIfPresent('z', obj.z ?? obj[alternateKeys[2]]);
  } else {
    assignIfPresent('x', value);
  }

  return Object.keys(result).length > 0 ? result : undefined;
}

export function normalizeTransformInput(transform: TransformInput | undefined): Record<string, unknown> | undefined {
  if (!transform || typeof transform !== 'object') return undefined;
  const result: Record<string, unknown> = {};
  const location = normalizePartialVector(transform.location);
  if (location) result.location = location;
  const rotation = normalizePartialVector(transform.rotation, ['pitch', 'yaw', 'roll']);
  if (rotation) result.rotation = rotation;
  const scale = normalizePartialVector(transform.scale);
  if (scale) result.scale = scale;
  return Object.keys(result).length > 0 ? result : undefined;
}

export type BlueprintScsOperationInput = {
  type: string;
  componentName?: string;
  componentClass?: string;
  attachTo?: string;
  transform?: TransformInput;
  properties?: Record<string, unknown>;
};

export function sanitizeScsOperation(rawOperation: BlueprintScsOperationInput, index: number): { ok: true; operation: Record<string, unknown> } | { ok: false; error: string } {
  if (!rawOperation || typeof rawOperation !== 'object') {
    return { ok: false, error: `Operation at index ${index} must be an object.` };
  }

  const type = (rawOperation.type || '').toString().trim().toLowerCase();
  if (!type) return { ok: false, error: `Operation at index ${index} missing type.` };
  const operation: Record<string, unknown> = { type };

  // Type-safe access to properties
  const op = rawOperation as Record<string, any>;
  const componentName = op.componentName ?? op.name;
  const componentClass = op.componentClass ?? op.componentType ?? op.class;
  const attachTo = op.attachTo ?? op.parent ?? op.attach;
  const transform = normalizeTransformInput(op.transform);
  const properties = rawOperation.properties && typeof rawOperation.properties === 'object' ? rawOperation.properties : undefined;

  switch (type) {
    case 'add_component': {
      if (!componentName) return { ok: false, error: `add_component operation at index ${index} requires componentName.` };
      if (!componentClass) return { ok: false, error: `add_component operation for ${componentName} missing componentClass.` };
      operation.componentName = componentName;
      operation.componentClass = componentClass;
      if (attachTo) operation.attachTo = attachTo;
      if (transform) operation.transform = transform;
      if (properties) operation.properties = properties;
      break;
    }
    case 'remove_component': {
      if (!componentName) return { ok: false, error: `remove_component operation at index ${index} requires componentName.` };
      operation.componentName = componentName;
      break;
    }
    case 'set_component_properties': {
      if (!componentName) return { ok: false, error: `set_component_properties operation at index ${index} requires componentName.` };
      if (!properties) return { ok: false, error: `set_component_properties operation at index ${index} missing properties object.` };
      operation.componentName = componentName;
      operation.properties = properties;
      if (transform) operation.transform = transform;
      break;
    }
    case 'modify_component': {
      if (!componentName) return { ok: false, error: `modify_component operation at index ${index} requires componentName.` };
      if (!transform && !properties) return { ok: false, error: `modify_component operation at index ${index} requires transform or properties.` };
      operation.componentName = componentName;
      if (transform) operation.transform = transform;
      if (properties) operation.properties = properties;
      break;
    }
    case 'attach_component': {
      const parent = op.parentComponent ?? op.parent;
      if (!componentName) return { ok: false, error: `attach_component operation at index ${index} requires componentName.` };
      if (!parent) return { ok: false, error: `attach_component operation at index ${index} requires parentComponent.` };
      operation.componentName = componentName;
      operation.attachTo = parent;
      break;
    }
    default:
      return { ok: false, error: `Unknown SCS operation type: ${type}` };
  }

  return { ok: true, operation };
}

export function resolveBlueprintCandidates(rawName: string | undefined): { primary: string | undefined; candidates: string[] } {
  const trimmed = coerceString(rawName)?.trim();
  if (!trimmed) return { primary: undefined, candidates: [] };

  // Normalize slashes and remove duplicates (global flag fixed)
  const normalized = trimmed.replace(/\\/g, '/').replace(/\/+/g, '/');
  const withoutLeading = normalized.replace(/^\/+/, '');
  
  // Build a prioritized list using a Set to handle uniqueness automatically
  const candidates = new Set<string>();
  
  const add = (path: string) => {
    if (path && path.trim()) candidates.add(path.replace(/\/+/g, '/'));
  };

  if (normalized.includes('/')) {
    // Path-like input: try to guess standard content paths first
    const basename = withoutLeading.split('/').pop();
    if (basename) {
      add(`/Game/Blueprints/${basename}`);
      add(`/Game/${basename}`);
    }
    add(normalized);
    add(normalized.startsWith('/') ? normalized : `/${withoutLeading}`);
  } else {
    // Bare name: try standard locations
    add(`/Game/Blueprints/${withoutLeading}`);
    add(`/Game/${withoutLeading}`);
    add(normalized);
    add(`/${withoutLeading}`);
  }

  const ordered = Array.from(candidates);
  return { primary: ordered[0], candidates: ordered };
}

 

export function inferVariableTypeFromValue(value: unknown): string | undefined {
  if (value === null || value === undefined) return undefined;
  if (typeof value === 'boolean') return 'Bool';
  if (typeof value === 'number') return Number.isInteger(value) ? 'Int' : 'Float';
  if (typeof value === 'string') return 'String';
  if (Array.isArray(value)) return 'Array';
  if (typeof value === 'object') {
    const keys = Object.keys(value as Record<string, unknown>);
    if (keys.includes('x') && keys.includes('y') && keys.includes('z')) return 'Vector';
    return 'Struct';
  }
  return undefined;
}
