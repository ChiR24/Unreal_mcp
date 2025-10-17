import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation-bridge.js';
import { Logger } from '../utils/logger.js';
import { bestEffortInterpretedText, interpretStandardResult } from '../utils/result-helpers.js';

export interface ObjectInfo {
  class: string;
  name: string;
  path: string;
  properties: PropertyInfo[];
  functions?: FunctionInfo[];
  parent?: string;
  interfaces?: string[];
  flags?: string[];
}

export interface PropertyInfo {
  name: string;
  type: string;
  value?: any;
  flags?: string[];
  metadata?: Record<string, any>;
  category?: string;
  tooltip?: string;
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
  
  constructor(private bridge: UnrealBridge, private automationBridge?: AutomationBridge) {}

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
   * Parse Python execution result with better error handling
   */
  private parsePythonResult(resp: any, operationName: string): any {
        const interpreted = interpretStandardResult(resp, {
            successMessage: `${operationName} succeeded`,
            failureMessage: `${operationName} failed`
        });

        if (interpreted.success) {
            return {
                ...interpreted.payload,
                success: true
            };
        }

    const output = bestEffortInterpretedText(interpreted) ?? '';
    if (output) {
      this.log.error(`Failed to parse ${operationName} result: ${output}`);
    }

    if (output.includes('ModuleNotFoundError')) {
      return { success: false, error: 'Reflection module not available.' };
    }
    if (output.includes('AttributeError')) {
      return { success: false, error: 'Reflection API method not found. Check Unreal Engine version compatibility.' };
    }

    return {
      success: false,
            error: `${interpreted.error ?? `${operationName} did not return a valid result`}: ${output.substring(0, 200)}`
    };
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

        const result = {
          success: true,
          info: response.info
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
                params.value.scale || params.value.Scale || {x: 1, y: 1, z: 1},
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
   * Clear object cache
   */
  clearCache(): void {
    this.objectCache.clear();
  }
}
