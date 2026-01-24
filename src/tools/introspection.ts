import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';
import { Logger } from '../utils/logger.js';
import { lookupPropertyMetadata, normalizeDictionaryKey, PropertyDictionaryEntry } from './property-dictionary.js';
import { requireBridge } from './base-tool.js';

export interface ObjectSummary {
  name?: string;
  class?: string;
  path?: string;
  parent?: string;
  tags?: string[];
  propertyCount: number;
  curatedPropertyCount: number;
  filteredPropertyCount: number;
  categories?: Record<string, number>;
}

export interface ObjectInfo {
  class?: string;
  name?: string;
  path?: string;
  properties: PropertyInfo[];
  functions?: FunctionInfo[];
  parent?: string;
  interfaces?: string[];
  flags?: string[];
  summary?: ObjectSummary;
  filteredProperties?: string[];
  tags?: string[];
  original?: unknown;
}

export interface PropertyInfo {
  name: string;
  type: string;
  value?: unknown;
  flags?: string[];
  metadata?: Record<string, unknown>;
  category?: string;
  tooltip?: string;
  description?: string;
  displayValue?: string;
  dictionaryEntry?: PropertyDictionaryEntry;
  isReadOnly?: boolean;
}

export interface FunctionInfo {
  name: string;
  parameters: ParameterInfo[];
  returnType?: string;
  flags?: string[];
  category?: string;
}

export interface ParameterInfo {
  name: string;
  type: string;
  defaultValue?: unknown;
  isOptional?: boolean;
}

export class IntrospectionTools {
  private log = new Logger('IntrospectionTools');
  private objectCache = new Map<string, ObjectInfo>();
  private retryAttempts = 3;
  private retryDelay = 1000;

  private readonly propertyFilterPatterns: RegExp[] = [
    /internal/i,
    /transient/i,
    /^temp/i,
    /^bhidden/i,
    /renderstate/i,
    /previewonly/i,
    /deprecated/i
  ];

  private readonly ignoredPropertyKeys = new Set<string>([
    'assetimportdata',
    'blueprintcreatedcomponents',
    'componentreplicator',
    'componentoverrides',
    'actorcomponenttags'
  ]);

  constructor(private bridge: UnrealBridge, private automationBridge?: AutomationBridge) { }

  setAutomationBridge(automationBridge?: AutomationBridge) { this.automationBridge = automationBridge; }

  /**
   * Execute with retry logic for transient failures
   */
  private async executeWithRetry<T>(
    operation: () => Promise<T>,
    operationName: string
  ): Promise<T> {
    let lastError: unknown;

    for (let attempt = 1; attempt <= this.retryAttempts; attempt++) {
      try {
        return await operation();
      } catch (error: unknown) {
        lastError = error;
        const errMsg = error instanceof Error ? error.message : String(error);
        this.log.warn(`${operationName} attempt ${attempt} failed: ${errMsg}`);

        if (attempt < this.retryAttempts) {
          await new Promise(resolve =>
            setTimeout(resolve, this.retryDelay * attempt)
          );
        }
      }
    }

    throw lastError;
  }

  /**
   * Convert Unreal property value to JavaScript-friendly format
   */
  private convertPropertyValue(value: unknown, typeName: string): unknown {
    // Handle vectors, rotators, transforms
    if (typeName.includes('Vector')) {
      if (typeof value === 'object' && value !== null) {
        const v = value as Record<string, unknown>;
        return { x: v.X || 0, y: v.Y || 0, z: v.Z || 0 };
      }
    }
    if (typeName.includes('Rotator')) {
      if (typeof value === 'object' && value !== null) {
        const v = value as Record<string, unknown>;
        return { pitch: v.Pitch || 0, yaw: v.Yaw || 0, roll: v.Roll || 0 };
      }
    }
    if (typeName.includes('Transform')) {
      if (typeof value === 'object' && value !== null) {
        const v = value as Record<string, unknown>;
        return {
          location: this.convertPropertyValue(v.Translation || v.Location, 'Vector'),
          rotation: this.convertPropertyValue(v.Rotation, 'Rotator'),
          scale: this.convertPropertyValue(v.Scale3D || v.Scale, 'Vector')
        };
      }
    }
    return value;
  }

  private isPlainObject(value: unknown): value is Record<string, unknown> {
    return !!value && typeof value === 'object' && !Array.isArray(value);
  }

  private isLikelyPropertyDescriptor(value: unknown): boolean {
    if (!this.isPlainObject(value)) return false;
    if (typeof value.name === 'string' && ('value' in value || 'type' in value)) return true;
    if ('propertyName' in value && ('currentValue' in value || 'defaultValue' in value)) return true;
    return false;
  }

  private shouldFilterProperty(name: string, value: unknown, flags?: string[], detailed = false): boolean {
    if (detailed) return false;
    if (!name) return true;
    const normalized = normalizeDictionaryKey(name);
    if (this.ignoredPropertyKeys.has(normalized)) return true;
    for (const pattern of this.propertyFilterPatterns) {
      if (pattern.test(name)) return true;
    }
    if (Array.isArray(value) && value.length === 0) return true;
    if (this.isPlainObject(value) && Object.keys(value).length === 0) return true;
    if (flags?.some((f) => /deprecated/i.test(f))) return true;
    return false;
  }

  private formatDisplayValue(value: unknown, type: string): string {
    if (value === null || value === undefined) return 'None';
    if (typeof value === 'string') return value.length > 120 ? `${value.slice(0, 117)}...` : value;
    if (typeof value === 'number' || typeof value === 'boolean') return `${value}`;
    if (Array.isArray(value)) {
      const preview = value.slice(0, 5).map((entry) => this.formatDisplayValue(entry, typeof entry));
      const suffix = value.length > 5 ? `, … (+${value.length - 5})` : '';
      return `[${preview.join(', ')}${suffix}]`;
    }
    if (this.isPlainObject(value)) {
      const keys = Object.keys(value);
      if (['vector', 'rotator', 'transform'].some((token) => type.toLowerCase().includes(token))) {
        const printable = Object.entries(value)
          .map(([k, v]) => `${k}: ${this.formatDisplayValue(v, typeof v)}`)
          .join(', ');
        return `{ ${printable} }`;
      }
      const preview = keys.slice(0, 5).map((k) => `${k}: ${this.formatDisplayValue(value[k], typeof value[k])}`);
      const suffix = keys.length > 5 ? `, … (+${keys.length - 5} keys)` : '';
      return `{ ${preview.join(', ')}${suffix} }`;
    }
    return String(value);
  }

  private normalizePropertyEntry(entry: unknown, detailed = false): PropertyInfo | null {
    if (!entry) return null;
    const e = entry as Record<string, unknown>;
    const name: string = (e.name ?? e.propertyName ?? e.key ?? '') as string;
    if (!name) return null;

    const candidateType = e.type ?? e.propertyType ?? (e.value !== undefined ? typeof e.value : undefined);
    const type = typeof candidateType === 'string' && candidateType.length > 0 ? candidateType : 'unknown';
    const rawValue = e.value ?? e.currentValue ?? e.defaultValue ?? e.data ?? entry;
    const value = this.convertPropertyValue(rawValue, type);
    const flags: string[] | undefined = (e.flags ?? e.attributes) as string[] | undefined;
    const metadata: Record<string, unknown> | undefined = (e.metadata ?? e.annotations) as Record<string, unknown> | undefined;
    const filtered = this.shouldFilterProperty(name, value, flags, detailed);
    const dictionaryEntry = lookupPropertyMetadata(name);
    const propertyInfo: PropertyInfo & { __filtered?: boolean } = {
      name,
      type,
      value,
      flags,
      metadata,
      category: dictionaryEntry?.category ?? (e.category as string | undefined),
      tooltip: (e.tooltip ?? e.helpText) as string | undefined,
      description: dictionaryEntry?.description ?? (e.description as string | undefined),
      displayValue: this.formatDisplayValue(value, type),
      dictionaryEntry,
      isReadOnly: Boolean(e.isReadOnly || e.readOnly || flags?.some((f) => f.toLowerCase().includes('readonly')))
    };
    propertyInfo.__filtered = filtered;
    return propertyInfo;
  }

  private flattenPropertyMap(source: Record<string, unknown>, prefix = '', detailed = false): PropertyInfo[] {
    const properties: PropertyInfo[] = [];
    for (const [rawKey, rawValue] of Object.entries(source)) {
      const name = prefix ? `${prefix}.${rawKey}` : rawKey;
      if (this.isLikelyPropertyDescriptor(rawValue)) {
        const normalized = this.normalizePropertyEntry({ ...(rawValue as Record<string, unknown>), name }, detailed);
        if (normalized) properties.push(normalized);
        continue;
      }

      if (this.isPlainObject(rawValue)) {
        const nestedKeys = Object.keys(rawValue);
        const hasPrimitiveChildren = nestedKeys.some((key) => {
          const child = rawValue[key];
          return child === null || typeof child !== 'object' || Array.isArray(child) || this.isLikelyPropertyDescriptor(child);
        });

        if (hasPrimitiveChildren) {
          const normalized = this.normalizePropertyEntry({ name, value: rawValue }, detailed);
          if (normalized) properties.push(normalized);
        } else {
          properties.push(...this.flattenPropertyMap(rawValue, name, detailed));
        }
        continue;
      }

      const normalized = this.normalizePropertyEntry({ name, value: rawValue }, detailed);
      if (normalized) properties.push(normalized);
    }
    return properties;
  }

  private extractRawProperties(rawInfo: unknown, detailed = false): PropertyInfo[] {
    if (!rawInfo) return [];
    const info = rawInfo as Record<string, unknown>;
    if (Array.isArray(info.properties)) {
      const entries = info.properties as Array<Record<string, unknown>>;
      return entries
        .map((entry) => this.normalizePropertyEntry(entry, detailed))
        .filter((entry): entry is PropertyInfo => Boolean(entry));
    }

    if (this.isPlainObject(info.properties)) {
      return this.flattenPropertyMap(info.properties, '', detailed);
    }

    if (Array.isArray(rawInfo)) {
      const entries = rawInfo as Array<Record<string, unknown>>;
      return entries
        .map((entry) => this.normalizePropertyEntry(entry, detailed))
        .filter((entry): entry is PropertyInfo => Boolean(entry));
    }

    if (this.isPlainObject(rawInfo)) {
      const shallow = { ...rawInfo };
      delete shallow.properties;
      delete shallow.functions;
      delete shallow.summary;
      return this.flattenPropertyMap(shallow, '', detailed);
    }

    return [];
  }

  curateObjectInfo(rawInfo: unknown, objectPath: string, detailed = false): ObjectInfo {
    const properties = this.extractRawProperties(rawInfo, detailed);
    const filteredProperties: string[] = [];
    const curatedProperties = properties.filter((prop) => {
      const propWithFiltered = prop as PropertyInfo & { __filtered?: boolean };
      const shouldFilter = propWithFiltered.__filtered;
      delete propWithFiltered.__filtered;
      if (shouldFilter) {
        filteredProperties.push(prop.name);
      }
      return !shouldFilter;
    });

    const categories: Record<string, number> = {};
    const finalList = (detailed ? properties : curatedProperties).map((prop) => {
      const category = (prop.category ?? prop.dictionaryEntry?.category ?? 'General');
      categories[category] = (categories[category] ?? 0) + 1;
      return prop;
    });

    const info = rawInfo as Record<string, unknown> | null | undefined;
    const summary: ObjectSummary = {
      name: (info?.name ?? info?.objectName ?? info?.displayName) as string | undefined,
      class: (info?.class ?? info?.className ?? info?.type ?? info?.objectClass) as string | undefined,
      path: (info?.path ?? info?.objectPath ?? objectPath) as string | undefined,
      parent: (info?.outer ?? info?.parent) as string | undefined,
      tags: Array.isArray(info?.tags) ? info.tags as string[] : undefined,
      propertyCount: properties.length,
      curatedPropertyCount: curatedProperties.length,
      filteredPropertyCount: filteredProperties.length,
      categories
    };

    const objectInfo: ObjectInfo = {
      class: summary.class,
      name: summary.name,
      path: summary.path,
      parent: summary.parent,
      tags: summary.tags,
      properties: finalList,
      functions: Array.isArray(info?.functions) ? info.functions as FunctionInfo[] : undefined,
      interfaces: Array.isArray(info?.interfaces) ? info.interfaces as string[] : undefined,
      flags: Array.isArray(info?.flags) ? info.flags as string[] : undefined,
      summary,
      filteredProperties: filteredProperties.length ? filteredProperties : undefined,
      original: rawInfo
    };

    return objectInfo;
  }

  async inspectObject(params: { objectPath: string; detailed?: boolean }) {
    // Check cache first if not requesting detailed info
    if (!params.detailed && this.objectCache.has(params.objectPath)) {
      const cached = this.objectCache.get(params.objectPath);
      if (cached) {
        return { success: true, info: cached };
      }
    }

    const bridge = requireBridge(this.automationBridge, 'Introspection operations');

    return this.executeWithRetry(async () => {
      try {
        const response = await bridge.sendAutomationRequest('inspect_object', {
          objectPath: params.objectPath,
          detailed: params.detailed ?? false
        }, {
          timeoutMs: 60000
        });

        if (response.success === false) {
          return {
            success: false,
            error: response.error || response.message || 'Failed to inspect object'
          };
        }

        const rawInfo = response.info ?? response.result ?? response.data ?? response;
        const curatedInfo = this.curateObjectInfo(rawInfo, params.objectPath, params.detailed ?? false);

        const result = {
          success: true,
          info: curatedInfo
        };

        // Cache the result if successful and not detailed
        if (result.success && result.info && !params.detailed) {
          this.objectCache.set(params.objectPath, result.info as ObjectInfo);
        }

        return result;
      } catch (error: unknown) {
        return {
          success: false,
          error: `Failed to inspect object: ${error instanceof Error ? error.message : String(error)}`
        };
      }
    }, 'inspectObject');
  }

  /**
   * Set property value on an object
   */
  async setProperty(params: { objectPath: string; propertyName: string; value: unknown }) {
    return this.executeWithRetry(async () => {
      try {
        // Validate and convert value type if needed
        let processedValue = params.value;

        // Handle special Unreal types
        if (typeof params.value === 'object' && params.value !== null) {
          const v = params.value as Record<string, unknown>;
          // Vector conversion
          if ('x' in v || 'X' in v) {
            processedValue = {
              X: v.x || v.X || 0,
              Y: v.y || v.Y || 0,
              Z: v.z || v.Z || 0
            };
          }
          // Rotator conversion
          else if ('pitch' in v || 'Pitch' in v) {
            processedValue = {
              Pitch: v.pitch || v.Pitch || 0,
              Yaw: v.yaw || v.Yaw || 0,
              Roll: v.roll || v.Roll || 0
            };
          }
          // Transform conversion
          else if ('location' in v || 'Location' in v) {
            processedValue = {
              Translation: this.convertPropertyValue(
                v.location || v.Location,
                'Vector'
              ),
              Rotation: this.convertPropertyValue(
                v.rotation || v.Rotation,
                'Rotator'
              ),
              Scale3D: this.convertPropertyValue(
                v.scale || v.Scale || { x: 1, y: 1, z: 1 },
                'Vector'
              )
            };
          }
        }
        const res = await this.bridge.setObjectProperty({
          objectPath: params.objectPath,
          propertyName: params.propertyName,
          value: processedValue
        });

        if (res.success) {
          this.objectCache.delete(params.objectPath);
        }

        return res;
      } catch (err: unknown) {
        const errorMsg = (err instanceof Error ? err.message : undefined) || String(err);
        if (errorMsg.includes('404')) {
          return { success: false, error: `Property '${params.propertyName}' not found on object '${params.objectPath}'` };
        }
        if (errorMsg.includes('400')) {
          return { success: false, error: `Invalid value type for property '${params.propertyName}'` };
        }
        return { success: false, error: errorMsg };
      }
    }, 'setProperty');
  }

  /**
   * Get property value of an object
   */
  async getProperty(params: { objectPath: string; propertyName: string }) {
    return this.executeWithRetry(
      async () => {
        const result = await this.bridge.getObjectProperty({
          objectPath: params.objectPath,
          propertyName: params.propertyName
        });

        return result;
      },
      'getProperty'
    );
  }

  /**
   * Call a function on an object
   */
  async callFunction(params: {
    objectPath: string;
    functionName: string;
    parameters?: unknown[];
  }) {
    const bridge = requireBridge(this.automationBridge, 'Function call operations');

    return this.executeWithRetry(async () => {
      try {
        const response = await bridge.sendAutomationRequest('call_object_function', {
          objectPath: params.objectPath,
          functionName: params.functionName,
          parameters: params.parameters || []
        }, {
          timeoutMs: 60000
        });

        if (response.success === false) {
          return {
            success: false,
            error: response.error || response.message || 'Failed to call function'
          };
        }

        return {
          success: true,
          result: response.result
        };
      } catch (error: unknown) {
        return {
          success: false,
          error: `Failed to call function: ${error instanceof Error ? error.message : String(error)}`
        };
      }
    }, 'callFunction');
  }

  /**
   * Get Class Default Object (CDO) for a class
   */
  async getCDO(className: string) {
    const bridge = requireBridge(this.automationBridge, 'CDO operations');

    return this.executeWithRetry(async () => {
      try {
        const response = await bridge.sendAutomationRequest('inspect_class', {
          // C++ plugin expects `classPath` for inspect_class, but accepts both
          // short names and full paths (e.g. "Actor" or "/Script/Engine.Actor").
          classPath: className
        }, {
          timeoutMs: 60000
        }) as Record<string, unknown>;

        if (response?.success === false) {
          return {
            success: false,
            error: response.error || response.message || 'Failed to get CDO'
          };
        }

        return {
          success: true,
          // Plugin returns class inspection data under data/result depending on bridge version.
          cdo: response?.data ?? response?.result ?? response
        };
      } catch (error: unknown) {
        return {
          success: false,
          error: `Failed to get CDO: ${error instanceof Error ? error.message : String(error)}`
        };
      }
    }, 'getCDO');
  }

  /**
   * Search for objects by class
   */
  async findObjectsByClass(className: string, limit: number = 100) {
    const bridge = requireBridge(this.automationBridge, 'Object search operations');

    return this.executeWithRetry(async () => {
      try {
        const response = await bridge.sendAutomationRequest('find_by_class', {
          className,
          limit
        }, {
          timeoutMs: 60000
        }) as Record<string, unknown>;

        if (response?.success === false) {
          return {
            success: false,
            error: (response.error || response.message || 'Failed to find objects') as string
          };
        }

        const data = (response?.data ?? response?.result ?? response) as Record<string, unknown>;
        const validObjects = Array.isArray(data?.actors)
          ? data.actors as Record<string, unknown>[]
          : (Array.isArray(data?.objects) ? data.objects as Record<string, unknown>[] : (Array.isArray(data) ? data : []));
        return {
          success: true,
          message: `Found ${validObjects.length} objects`,
          objects: validObjects,
          count: validObjects.length
        };
      } catch (error: unknown) {
        return {
          success: false,
          error: `Failed to find objects: ${error instanceof Error ? error.message : String(error)}`
        };
      }
    }, 'findObjectsByClass');
  }

  /**
   * Get property value of a component
   */
  async getComponentProperty(params: { objectPath: string; componentName: string; propertyName: string }) {
    const bridge = requireBridge(this.automationBridge, 'Component property operations');

    return this.executeWithRetry(async () => {
      try {
        const response = await bridge.sendAutomationRequest('get_component_property', {
          objectPath: params.objectPath,
          componentName: params.componentName,
          propertyName: params.propertyName
        }, {
          timeoutMs: 15000
        });

        if (response.success === false) {
          return {
            success: false,
            error: response.error || response.message || 'Failed to get component property'
          };
        }

        return {
          success: true,
          value: response.value,
          type: response.type
        };
      } catch (error: unknown) {
        return {
          success: false,
          error: `Failed to get component property: ${error instanceof Error ? error.message : String(error)}`
        };
      }
    }, 'getComponentProperty');
  }

  /**
   * Set property value of a component
   */
  async setComponentProperty(params: { objectPath: string; componentName: string; propertyName: string; value: unknown }) {
    const bridge = requireBridge(this.automationBridge, 'Component property operations');

    return this.executeWithRetry(async () => {
      try {
        const response = await bridge.sendAutomationRequest('set_component_property', {
          objectPath: params.objectPath,
          componentName: params.componentName,
          propertyName: params.propertyName,
          value: params.value
        }, {
          timeoutMs: 15000
        });

        if (response.success === false) {
          return {
            success: false,
            error: response.error || response.message || 'Failed to set component property'
          };
        }

        return {
          success: true,
          message: response.message || 'Property set successfully'
        };
      } catch (error: unknown) {
        return {
          success: false,
          error: `Failed to set component property: ${error instanceof Error ? error.message : String(error)}`
        };
      }
    }, 'setComponentProperty');
  }

  /**
   * Clear object cache
   */
  clearCache(): void {
    this.objectCache.clear();
  }
}
