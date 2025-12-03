import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';
import { Logger } from '../utils/logger.js';
import { lookupPropertyMetadata, normalizeDictionaryKey, PropertyDictionaryEntry } from './property-dictionary.js';

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
  original?: any;
}

export interface PropertyInfo {
  name: string;
  type: string;
  value?: any;
  flags?: string[];
  metadata?: Record<string, any>;
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
  defaultValue?: any;
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
    let lastError: any;

    for (let attempt = 1; attempt <= this.retryAttempts; attempt++) {
      try {
        return await operation();
      } catch (error: any) {
        lastError = error;
        this.log.warn(`${operationName} attempt ${attempt} failed: ${error.message || error}`);

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
  private convertPropertyValue(value: any, typeName: string): any {
    // Handle vectors, rotators, transforms
    if (typeName.includes('Vector')) {
      if (typeof value === 'object' && value !== null) {
        return { x: value.X || 0, y: value.Y || 0, z: value.Z || 0 };
      }
    }
    if (typeName.includes('Rotator')) {
      if (typeof value === 'object' && value !== null) {
        return { pitch: value.Pitch || 0, yaw: value.Yaw || 0, roll: value.Roll || 0 };
      }
    }
    if (typeName.includes('Transform')) {
      if (typeof value === 'object' && value !== null) {
        return {
          location: this.convertPropertyValue(value.Translation || value.Location, 'Vector'),
          rotation: this.convertPropertyValue(value.Rotation, 'Rotator'),
          scale: this.convertPropertyValue(value.Scale3D || value.Scale, 'Vector')
        };
      }
    }
    return value;
  }

  private isPlainObject(value: any): value is Record<string, any> {
    return !!value && typeof value === 'object' && !Array.isArray(value);
  }

  private isLikelyPropertyDescriptor(value: any): boolean {
    if (!this.isPlainObject(value)) return false;
    if (typeof value.name === 'string' && ('value' in value || 'type' in value)) return true;
    if ('propertyName' in value && ('currentValue' in value || 'defaultValue' in value)) return true;
    return false;
  }

  private shouldFilterProperty(name: string, value: any, flags?: string[], detailed = false): boolean {
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

  private formatDisplayValue(value: any, type: string): string {
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

  private normalizePropertyEntry(entry: any, detailed = false): PropertyInfo | null {
    if (!entry) return null;
    const name: string = entry.name ?? entry.propertyName ?? entry.key ?? '';
    if (!name) return null;

    const candidateType = entry.type ?? entry.propertyType ?? (entry.value !== undefined ? typeof entry.value : undefined);
    const type = typeof candidateType === 'string' && candidateType.length > 0 ? candidateType : 'unknown';
    const rawValue = entry.value ?? entry.currentValue ?? entry.defaultValue ?? entry.data ?? entry;
    const value = this.convertPropertyValue(rawValue, type);
    const flags: string[] | undefined = entry.flags ?? entry.attributes;
    const metadata: Record<string, any> | undefined = entry.metadata ?? entry.annotations;
    const filtered = this.shouldFilterProperty(name, value, flags, detailed);
    const dictionaryEntry = lookupPropertyMetadata(name);
    const propertyInfo: PropertyInfo = {
      name,
      type,
      value,
      flags,
      metadata,
      category: dictionaryEntry?.category ?? entry.category,
      tooltip: entry.tooltip ?? entry.helpText,
      description: dictionaryEntry?.description ?? entry.description,
      displayValue: this.formatDisplayValue(value, type),
      dictionaryEntry,
      isReadOnly: Boolean(entry.isReadOnly || entry.readOnly || flags?.some((f) => f.toLowerCase().includes('readonly')))
    };
    (propertyInfo as any).__filtered = filtered;
    return propertyInfo;
  }

  private flattenPropertyMap(source: Record<string, any>, prefix = '', detailed = false): PropertyInfo[] {
    const properties: PropertyInfo[] = [];
    for (const [rawKey, rawValue] of Object.entries(source)) {
      const name = prefix ? `${prefix}.${rawKey}` : rawKey;
      if (this.isLikelyPropertyDescriptor(rawValue)) {
        const normalized = this.normalizePropertyEntry({ ...rawValue, name }, detailed);
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

  private extractRawProperties(rawInfo: any, detailed = false): PropertyInfo[] {
    if (!rawInfo) return [];
    if (Array.isArray(rawInfo.properties)) {
      const entries = rawInfo.properties as Array<Record<string, unknown>>;
      return entries
        .map((entry) => this.normalizePropertyEntry(entry, detailed))
        .filter((entry): entry is PropertyInfo => Boolean(entry));
    }

    if (this.isPlainObject(rawInfo.properties)) {
      return this.flattenPropertyMap(rawInfo.properties, '', detailed);
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

  curateObjectInfo(rawInfo: any, objectPath: string, detailed = false): ObjectInfo {
    const properties = this.extractRawProperties(rawInfo, detailed);
    const filteredProperties: string[] = [];
    const curatedProperties = properties.filter((prop) => {
      const shouldFilter = (prop as any).__filtered;
      delete (prop as any).__filtered;
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

    const summary: ObjectSummary = {
      name: rawInfo?.name ?? rawInfo?.objectName ?? rawInfo?.displayName,
      class: rawInfo?.class ?? rawInfo?.className ?? rawInfo?.type ?? rawInfo?.objectClass,
      path: rawInfo?.path ?? rawInfo?.objectPath ?? objectPath,
      parent: rawInfo?.outer ?? rawInfo?.parent,
      tags: Array.isArray(rawInfo?.tags) ? rawInfo.tags : undefined,
      propertyCount: properties.length,
      curatedPropertyCount: curatedProperties.length,
      filteredPropertyCount: filteredProperties.length,
      categories
    };

    const info: ObjectInfo = {
      class: summary.class,
      name: summary.name,
      path: summary.path,
      parent: summary.parent,
      tags: summary.tags,
      properties: finalList,
      functions: Array.isArray(rawInfo?.functions) ? rawInfo.functions : undefined,
      interfaces: Array.isArray(rawInfo?.interfaces) ? rawInfo.interfaces : undefined,
      flags: Array.isArray(rawInfo?.flags) ? rawInfo.flags : undefined,
      summary,
      filteredProperties: filteredProperties.length ? filteredProperties : undefined,
      original: rawInfo
    };

    return info;
  }

  async inspectObject(params: { objectPath: string; detailed?: boolean }) {
    // Check cache first if not requesting detailed info
    if (!params.detailed && this.objectCache.has(params.objectPath)) {
      const cached = this.objectCache.get(params.objectPath);
      if (cached) {
        return { success: true, info: cached };
      }
    }

    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Introspection operations require plugin support.');
    }

    const automationBridge = this.automationBridge;
    return this.executeWithRetry(async () => {
      try {
        const response = await automationBridge.sendAutomationRequest('inspect_object', {
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
      } catch (error) {
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
  async setProperty(params: { objectPath: string; propertyName: string; value: any }) {
    return this.executeWithRetry(async () => {
      try {
        // Validate and convert value type if needed
        let processedValue = params.value;

        // Handle special Unreal types
        if (typeof params.value === 'object' && params.value !== null) {
          // Vector conversion
          if ('x' in params.value || 'X' in params.value) {
            processedValue = {
              X: params.value.x || params.value.X || 0,
              Y: params.value.y || params.value.Y || 0,
              Z: params.value.z || params.value.Z || 0
            };
          }
          // Rotator conversion
          else if ('pitch' in params.value || 'Pitch' in params.value) {
            processedValue = {
              Pitch: params.value.pitch || params.value.Pitch || 0,
              Yaw: params.value.yaw || params.value.Yaw || 0,
              Roll: params.value.roll || params.value.Roll || 0
            };
          }
          // Transform conversion
          else if ('location' in params.value || 'Location' in params.value) {
            processedValue = {
              Translation: this.convertPropertyValue(
                params.value.location || params.value.Location,
                'Vector'
              ),
              Rotation: this.convertPropertyValue(
                params.value.rotation || params.value.Rotation,
                'Rotator'
              ),
              Scale3D: this.convertPropertyValue(
                params.value.scale || params.value.Scale || { x: 1, y: 1, z: 1 },
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
      } catch (err: any) {
        const errorMsg = err?.message || String(err);
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
    parameters?: any[];
  }) {
    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Function call operations require plugin support.');
    }

    const automationBridge = this.automationBridge;
    return this.executeWithRetry(async () => {
      try {
        const response = await automationBridge.sendAutomationRequest('call_object_function', {
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
      } catch (error) {
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
    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. CDO operations require plugin support.');
    }

    const automationBridge = this.automationBridge;
    return this.executeWithRetry(async () => {
      try {
        const response = await automationBridge.sendAutomationRequest('get_class_default_object', {
          className
        }, {
          timeoutMs: 60000
        });

        if (response.success === false) {
          return {
            success: false,
            error: response.error || response.message || 'Failed to get CDO'
          };
        }

        return {
          success: true,
          cdo: response.cdo
        };
      } catch (error) {
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
    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Object search operations require plugin support.');
    }

    const automationBridge = this.automationBridge;
    return this.executeWithRetry(async () => {
      try {
        const response = await automationBridge.sendAutomationRequest('find_objects_by_class', {
          className,
          limit
        }, {
          timeoutMs: 60000
        });

        if (response.success === false) {
          return {
            success: false,
            error: response.error || response.message || 'Failed to find objects'
          };
        }

        return {
          success: true,
          objects: response.objects || [],
          count: response.count || 0
        };
      } catch (error) {
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
    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Component property operations require plugin support.');
    }

    const automationBridge = this.automationBridge;
    return this.executeWithRetry(async () => {
      try {
        const response = await automationBridge.sendAutomationRequest('get_component_property', {
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
      } catch (error) {
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
  async setComponentProperty(params: { objectPath: string; componentName: string; propertyName: string; value: any }) {
    if (!this.automationBridge) {
      throw new Error('Automation Bridge not available. Component property operations require plugin support.');
    }

    const automationBridge = this.automationBridge;
    return this.executeWithRetry(async () => {
      try {
        const response = await automationBridge.sendAutomationRequest('set_component_property', {
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
      } catch (error) {
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
