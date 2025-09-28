import { UnrealBridge } from '../unreal-bridge.js';
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
  
  constructor(private bridge: UnrealBridge) {}

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
    
    const py = `
import unreal, json, inspect
path = r"${params.objectPath}"
detailed = ${params.detailed ? 'True' : 'False'}

def get_property_info(prop, obj=None):
    """Extract detailed property information"""
    try:
        info = {
            'name': prop.get_name(),
            'type': prop.get_property_class_name() if hasattr(prop, 'get_property_class_name') else 'Unknown'
        }
        
        # Try to get property flags
        flags = []
        if hasattr(prop, 'has_any_property_flags'):
            if prop.has_any_property_flags(unreal.PropertyFlags.CPF_EDIT_CONST):
                flags.append('ReadOnly')
            if prop.has_any_property_flags(unreal.PropertyFlags.CPF_BLUEPRINT_READ_ONLY):
                flags.append('BlueprintReadOnly')
            if prop.has_any_property_flags(unreal.PropertyFlags.CPF_TRANSIENT):
                flags.append('Transient')
        info['flags'] = flags
        
        # Try to get metadata
        if hasattr(prop, 'get_metadata'):
            try:
                info['category'] = prop.get_metadata('Category')
                info['tooltip'] = prop.get_metadata('ToolTip')
            except Exception:
                pass
        
        # Try to get current value if object provided
        if obj and detailed:
            try:
                value = getattr(obj, prop.get_name())
                # Convert complex types to serializable format
                if hasattr(value, '__dict__'):
                    value = str(value)
                info['value'] = value
            except Exception:
                pass
        
        return info
    except Exception as e:
        return {'name': str(prop) if prop else 'Unknown', 'type': 'Unknown', 'error': str(e)}

try:
    obj = unreal.load_object(None, path)
    if not obj:
        # Try as class if object load fails
        try:
            obj = unreal.load_class(None, path)
            if not obj:
                print('RESULT:' + json.dumps({'success': False, 'error': 'Object or class not found'}))
                raise SystemExit(0)
        except Exception:
            print('RESULT:' + json.dumps({'success': False, 'error': 'Object not found'}))
            raise SystemExit(0)
    
    info = {
        'class': obj.get_class().get_name() if hasattr(obj, 'get_class') else str(type(obj)),
        'name': obj.get_name() if hasattr(obj, 'get_name') else '',
        'path': path,
        'properties': [],
        'functions': [],
        'flags': []
    }
    
    # Get parent class
    try:
        if hasattr(obj, 'get_class'):
            cls = obj.get_class()
            if hasattr(cls, 'get_super_class'):
                super_cls = cls.get_super_class()
                if super_cls:
                    info['parent'] = super_cls.get_name()
    except Exception:
        pass
    
    # Get object flags
    try:
        if hasattr(obj, 'has_any_flags'):
            flags = []
            if obj.has_any_flags(unreal.ObjectFlags.RF_PUBLIC):
                flags.append('Public')
            if obj.has_any_flags(unreal.ObjectFlags.RF_TRANSIENT):
                flags.append('Transient')
            if obj.has_any_flags(unreal.ObjectFlags.RF_DEFAULT_SUB_OBJECT):
                flags.append('DefaultSubObject')
            info['flags'] = flags
    except Exception:
        pass
    
    # Get properties - AVOID deprecated properties completely
    props = []
    
    # List of deprecated properties to completely skip
    deprecated_props = [
        'life_span', 'on_actor_touch', 'on_actor_un_touch',
        'get_touching_actors', 'get_touching_components',
        'controller_class', 'look_up_scale', 'sound_wave_param',
        'material_substitute', 'texture_object'
    ]
    
    try:
        cls = obj.get_class() if hasattr(obj, 'get_class') else obj
        
        # Try UE5 reflection API with safe property access
        if hasattr(cls, 'get_properties'):
            for prop in cls.get_properties():
                prop_name = None
                try:
                    # Safe property name extraction
                    if hasattr(prop, 'get_name'):
                        prop_name = prop.get_name()
                    elif hasattr(prop, 'name'):
                        prop_name = prop.name
                    
                    # Skip if deprecated
                    if prop_name and prop_name not in deprecated_props:
                        prop_info = get_property_info(prop, obj if detailed else None)
                        if prop_info.get('name') not in deprecated_props:
                            props.append(prop_info)
                except Exception:
                    pass
        
        # If reflection API didn't work, use a safe property list
        if not props:
            # Only access known safe properties
            safe_properties = [
                'actor_guid', 'actor_instance_guid', 'always_relevant',
                'auto_destroy_when_finished', 'can_be_damaged',
                'content_bundle_guid', 'custom_time_dilation',
                'enable_auto_lod_generation', 'find_camera_component_when_view_target',
                'generate_overlap_events_during_level_streaming', 'hidden',
                'initial_life_span',  # Use new name instead of life_span
                'instigator', 'is_spatially_loaded', 'min_net_update_frequency',
                'net_cull_distance_squared', 'net_dormancy', 'net_priority',
                'net_update_frequency', 'net_use_owner_relevancy',
                'only_relevant_to_owner', 'pivot_offset',
                'replicate_using_registered_sub_object_list', 'replicates',
                'root_component', 'runtime_grid', 'spawn_collision_handling_method',
                'sprite_scale', 'tags', 'location', 'rotation', 'scale'
            ]
            
            for prop_name in safe_properties:
                try:
                    # Use get_editor_property for safer access
                    if hasattr(obj, 'get_editor_property'):
                        val = obj.get_editor_property(prop_name)
                        props.append({
                            'name': prop_name,
                            'type': type(val).__name__ if val is not None else 'None',
                            'value': str(val)[:100] if detailed and val is not None else None
                        })
                    elif hasattr(obj, prop_name):
                        # Direct access only for safe properties
                        val = getattr(obj, prop_name)
                        if not callable(val):
                            props.append({
                                'name': prop_name,
                                'type': type(val).__name__,
                                'value': str(val)[:100] if detailed else None
                            })
                except Exception:
                    pass
    except Exception as e:
        # Minimal fallback with only essential safe properties
        pass
    
    info['properties'] = props
    
    # Get functions/methods if detailed
    if detailed:
        funcs = []
        try:
            cls = obj.get_class() if hasattr(obj, 'get_class') else obj
            
            # Try to get UFunctions
            if hasattr(cls, 'get_functions'):
                for func in cls.get_functions():
                    func_info = {
                        'name': func.get_name(),
                        'parameters': [],
                        'flags': []
                    }
                    # Get parameters if possible
                    if hasattr(func, 'get_params'):
                        for param in func.get_params():
                            func_info['parameters'].append({
                                'name': param.get_name() if hasattr(param, 'get_name') else str(param),
                                'type': 'Unknown'
                            })
                    funcs.append(func_info)
            else:
                # Fallback: use known safe function names
                safe_functions = [
                    'get_actor_location', 'set_actor_location',
                    'get_actor_rotation', 'set_actor_rotation',
                    'get_actor_scale', 'set_actor_scale',
                    'destroy_actor', 'destroy_component',
                    'get_components', 'get_component_by_class',
                    'add_actor_component', 'add_component',
                    'get_world', 'get_name', 'get_path_name',
                    'is_valid', 'is_a', 'has_authority'
                ]
                for func_name in safe_functions:
                    if hasattr(obj, func_name):
                        try:
                            attr_value = getattr(obj, func_name)
                            if callable(attr_value):
                                funcs.append({
                                    'name': func_name,
                                    'parameters': [],
                                    'flags': []
                                })
                        except Exception:
                            pass
        except Exception:
            pass
        
        info['functions'] = funcs
    
    print('RESULT:' + json.dumps({'success': True, 'info': info}))
except Exception as e:
    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
`.trim();
    const resp = await this.executeWithRetry(
      () => this.bridge.executePython(py),
      'inspectObject'
    );
    
    const result = this.parsePythonResult(resp, 'inspectObject');
    
    // Cache the result if successful and not detailed
    if (result.success && result.info && !params.detailed) {
      this.objectCache.set(params.objectPath, result.info);
    }
    
    return result;
  }

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
        
        const res = await this.bridge.httpCall('/remote/object/property', 'PUT', {
          objectPath: params.objectPath,
          propertyName: params.propertyName,
          propertyValue: processedValue
        });
        
        // Clear cache for this object
        this.objectCache.delete(params.objectPath);
        
        return { success: true, result: res };
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
    const py = `
import unreal, json
path = r"${params.objectPath}"
prop_name = r"${params.propertyName}"
try:
    obj = unreal.load_object(None, path)
    if not obj:
        print('RESULT:' + json.dumps({'success': False, 'error': 'Object not found'}))
    else:
        # Try different methods to get property
        value = None
        found = False
        
        # Method 1: Direct attribute access
        if hasattr(obj, prop_name):
            try:
                value = getattr(obj, prop_name)
                found = True
            except Exception:
                pass
        
        # Method 2: get_editor_property (UE4/5)
        if not found and hasattr(obj, 'get_editor_property'):
            try:
                value = obj.get_editor_property(prop_name)
                found = True
            except Exception:
                pass
        
        # Method 3: Try with common property name variations
        if not found:
            # Try common property name variations
            variations = [
                prop_name,
                prop_name.lower(),
                prop_name.upper(),
                prop_name.capitalize(),
                # Convert snake_case to CamelCase
                ''.join(word.capitalize() for word in prop_name.split('_')),
                # Convert CamelCase to snake_case
                ''.join(['_' + c.lower() if c.isupper() else c for c in prop_name]).lstrip('_')
            ]
            for variant in variations:
                if hasattr(obj, variant):
                    try:
                        value = getattr(obj, variant)
                        found = True
                        break
                    except Exception:
                        pass
        
        if found:
            # Convert complex types to string
            if hasattr(value, '__dict__'):
                value = str(value)
            elif isinstance(value, (list, tuple, dict)):
                value = json.dumps(value)
            
            print('RESULT:' + json.dumps({'success': True, 'value': value}))
        else:
            print('RESULT:' + json.dumps({'success': False, 'error': f'Property {prop_name} not found'}))
except Exception as e:
    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
`.trim();

    const resp = await this.executeWithRetry(
      () => this.bridge.executePython(py),
      'getProperty'
    );
    
    return this.parsePythonResult(resp, 'getProperty');
  }

  /**
   * Call a function on an object
   */
  async callFunction(params: {
    objectPath: string;
    functionName: string;
    parameters?: any[];
  }) {
    const py = `
import unreal, json
path = r"${params.objectPath}"
func_name = r"${params.functionName}"
params = ${JSON.stringify(params.parameters || [])}
try:
    obj = unreal.load_object(None, path)
    if not obj:
        # Try loading as class if object fails
        try:
            obj = unreal.load_class(None, path)
        except:
            pass
    
    if not obj:
        print('RESULT:' + json.dumps({'success': False, 'error': 'Object not found'}))
    else:
        # For KismetMathLibrary or similar utility classes, use static method call
        if 'KismetMathLibrary' in path or 'MathLibrary' in path or 'GameplayStatics' in path:
            try:
                # Use Unreal's MathLibrary (KismetMathLibrary is exposed as MathLibrary in Python)
                if func_name.lower() == 'abs':
                    # Use Unreal's MathLibrary.abs function
                    result = unreal.MathLibrary.abs(float(params[0])) if params else 0
                    print('RESULT:' + json.dumps({'success': True, 'result': result}))
                elif func_name.lower() == 'sqrt':
                    # Use Unreal's MathLibrary.sqrt function
                    result = unreal.MathLibrary.sqrt(float(params[0])) if params else 0
                    print('RESULT:' + json.dumps({'success': True, 'result': result}))
                else:
                    # Try to call as static method
                    if hasattr(obj, func_name):
                        func = getattr(obj, func_name)
                        if callable(func):
                            result = func(*params) if params else func()
                            if hasattr(result, '__dict__'):
                                result = str(result)
                            print('RESULT:' + json.dumps({'success': True, 'result': result}))
                        else:
                            print('RESULT:' + json.dumps({'success': False, 'error': f'{func_name} is not callable'}))
                    else:
                        # Try snake_case version
                        snake_case_name = ''.join(['_' + c.lower() if c.isupper() else c for c in func_name]).lstrip('_')
                        if hasattr(obj, snake_case_name):
                            func = getattr(obj, snake_case_name)
                            result = func(*params) if params else func()
                            print('RESULT:' + json.dumps({'success': True, 'result': result}))
                        else:
                            print('RESULT:' + json.dumps({'success': False, 'error': f'Function {func_name} not found'}))
            except Exception as e:
                print('RESULT:' + json.dumps({'success': False, 'error': f'Function call failed: {str(e)}'}))
        else:
            # Regular object method call
            if hasattr(obj, func_name):
                func = getattr(obj, func_name)
                if callable(func):
                    try:
                        result = func(*params) if params else func()
                        # Convert result to serializable format
                        if hasattr(result, '__dict__'):
                            result = str(result)
                        print('RESULT:' + json.dumps({'success': True, 'result': result}))
                    except Exception as e:
                        print('RESULT:' + json.dumps({'success': False, 'error': f'Function call failed: {str(e)}'}))
                else:
                    print('RESULT:' + json.dumps({'success': False, 'error': f'{func_name} is not callable'}))
            else:
                print('RESULT:' + json.dumps({'success': False, 'error': f'Function {func_name} not found'}))
except Exception as e:
    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
`.trim();

    const resp = await this.executeWithRetry(
      () => this.bridge.executePython(py),
      'callFunction'
    );
    
    return this.parsePythonResult(resp, 'callFunction');
  }

  /**
   * Get Class Default Object (CDO) for a class
   */
  async getCDO(className: string) {
    const py = `
import unreal, json
class_name = r"${className}"
try:
    # Try to find the class
    cls = None
    
    # Method 1: Direct class load
    try:
        cls = unreal.load_class(None, class_name)
    except Exception:
        pass
    
    # Method 2: Find class by name
    if not cls:
        try:
            cls = unreal.find_class(class_name)
        except Exception:
            pass
    
    # Method 3: Search in loaded classes
    if not cls:
        for obj in unreal.ObjectLibrary.get_all_objects():
            if hasattr(obj, 'get_class'):
                obj_cls = obj.get_class()
                if obj_cls.get_name() == class_name:
                    cls = obj_cls
                    break
    
    if not cls:
        print('RESULT:' + json.dumps({'success': False, 'error': 'Class not found'}))
    else:
        # Get CDO
        cdo = cls.get_default_object() if hasattr(cls, 'get_default_object') else None
        
        if cdo:
            info = {
                'className': cls.get_name(),
                'cdoPath': cdo.get_path_name() if hasattr(cdo, 'get_path_name') else '',
                'properties': []
            }
            
            # Get default property values using safe property list
            safe_cdo_properties = [
                'initial_life_span', 'hidden', 'can_be_damaged', 'replicates',
                'always_relevant', 'net_dormancy', 'net_priority',
                'net_update_frequency', 'replicate_movement',
                'actor_guid', 'tags', 'root_component',
                'auto_destroy_when_finished', 'enable_auto_lod_generation'
            ]
            for prop_name in safe_cdo_properties:
                try:
                    if hasattr(cdo, 'get_editor_property'):
                        value = cdo.get_editor_property(prop_name)
                        info['properties'].append({
                            'name': prop_name,
                            'defaultValue': str(value)[:100]
                        })
                    elif hasattr(cdo, prop_name):
                        value = getattr(cdo, prop_name)
                        if not callable(value):
                            info['properties'].append({
                                'name': prop_name,
                                'defaultValue': str(value)[:100]
                            })
                except Exception:
                    pass
            
            print('RESULT:' + json.dumps({'success': True, 'cdo': info}))
        else:
            print('RESULT:' + json.dumps({'success': False, 'error': 'Could not get CDO'}))
except Exception as e:
    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
`.trim();

    const resp = await this.executeWithRetry(
      () => this.bridge.executePython(py),
      'getCDO'
    );
    
    return this.parsePythonResult(resp, 'getCDO');
  }

  /**
   * Search for objects by class
   */
  async findObjectsByClass(className: string, limit: number = 100) {
    const py = `
import unreal, json
class_name = r"${className}"
limit = ${limit}
try:
    objects = []
    count = 0
    
    # Use EditorAssetLibrary to find assets
    try:
        all_assets = unreal.EditorAssetLibrary.list_assets("/Game", recursive=True)
        for asset_path in all_assets:
            if count >= limit:
                break
            try:
                asset = unreal.EditorAssetLibrary.load_asset(asset_path)
                if asset:
                    asset_class = asset.get_class() if hasattr(asset, 'get_class') else None
                    if asset_class and class_name in asset_class.get_name():
                        objects.append({
                            'path': asset_path,
                            'name': asset.get_name() if hasattr(asset, 'get_name') else '',
                            'class': asset_class.get_name()
                        })
                        count += 1
            except Exception:
                pass
    except Exception as e:
        print('RESULT:' + json.dumps({'success': False, 'error': f'Asset search failed: {str(e)}'}))
        raise SystemExit(0)
    
    # Also search in level actors
    try:
        actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        if actor_sub:
            for actor in actor_sub.get_all_level_actors():
                if count >= limit:
                    break
                if actor:
                    actor_class = actor.get_class() if hasattr(actor, 'get_class') else None
                    if actor_class and class_name in actor_class.get_name():
                        objects.append({
                            'path': actor.get_path_name() if hasattr(actor, 'get_path_name') else '',
                            'name': actor.get_actor_label() if hasattr(actor, 'get_actor_label') else '',
                            'class': actor_class.get_name()
                        })
                        count += 1
    except Exception:
        pass
    
    print('RESULT:' + json.dumps({'success': True, 'objects': objects, 'count': len(objects)}))
except Exception as e:
    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
`.trim();

    const resp = await this.executeWithRetry(
      () => this.bridge.executePython(py),
      'findObjectsByClass'
    );
    
    return this.parsePythonResult(resp, 'findObjectsByClass');
  }

  /**
   * Clear object cache
   */
  clearCache(): void {
    this.objectCache.clear();
  }
}
