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

  const componentName = (rawOperation as any).componentName ?? (rawOperation as any).name;
  const componentClass = (rawOperation as any).componentClass ?? (rawOperation as any).componentType ?? (rawOperation as any).class;
  const attachTo = (rawOperation as any).attachTo ?? (rawOperation as any).parent ?? (rawOperation as any).attach;
  const transform = normalizeTransformInput((rawOperation as any).transform);
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
      const parent = (rawOperation as any).parentComponent ?? (rawOperation as any).parent;
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
  const normalizedInput = trimmed.replace(/\\/g, '/').replace(/\/\/+/, '/');
  const withoutLeading = normalizedInput.replace(/^\/+/, '');
  // Build a prioritized list where absolute /Game/ paths are preferred
  // before attempting bare names. This avoids calling EditorAssetLibrary
  // with invalid paths such as 'BP_TestPawn' which cause repeated engine
  // errors when probed frequently.
  const seen = new Set<string>();
  const ordered: string[] = [];
  const pushUnique = (v?: string) => {
    if (!v) return;
    const fixed = v.replace(/\\/g, '/').replace(/\/\/+/ , '/').trim();
    if (!fixed) return;
    if (seen.has(fixed)) return;
    seen.add(fixed);
    ordered.push(fixed);
  };

  if (normalizedInput.includes('/')) {
    // If caller provided a path-like input, try to normalize and prefer
    // absolute /Game/ paths derived from the basename.
    const remainder = withoutLeading.split('/').pop();
    if (remainder) {
      pushUnique(`/Game/Blueprints/${remainder}`);
      pushUnique(`/Game/${remainder}`);
    }
    // Keep the original normalized input as a fallback.
    pushUnique(normalizedInput);
    // Also include a leading-slash variant if not present.
    pushUnique(normalizedInput.startsWith('/') ? normalizedInput : `/${withoutLeading}`);
  } else {
    // Bare name: prefer common content roots first, then fall back to
    // the raw name and a leading-slash variant.
    pushUnique(`/Game/Blueprints/${withoutLeading}`);
    pushUnique(`/Game/${withoutLeading}`);
    pushUnique(normalizedInput);
    pushUnique(`/${withoutLeading}`);
  }

  return { primary: ordered[0], candidates: ordered };
}

export function shouldAttemptPythonFallback(errorCode?: string, message?: string): boolean {
  const normalizedCode = (errorCode ?? '').toUpperCase();
  const normalizedMessage = (message ?? '').toLowerCase();
  if (!normalizedCode && !normalizedMessage) return false;
  if (normalizedCode.includes('AUTOMATION_BRIDGE')) return true;
  if (normalizedMessage.includes('automation bridge')) return true;
  if (normalizedCode.includes('SCS_UNAVAILABLE') || normalizedMessage.includes('simpleconstructionscript') || normalizedMessage.includes('scs_unavailable')) return true;
  if (normalizedCode.includes('COMPONENT_CLASS_NOT_FOUND') || normalizedMessage.includes('unable to load component class')) return true;
  if (normalizedCode.includes('PROPERTY_NOT_FOUND') || (normalizedMessage.includes('property') && normalizedMessage.includes('not found'))) return true;
  return false;
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
