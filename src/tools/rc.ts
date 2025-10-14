import { UnrealBridge } from '../unreal-bridge.js';
import { Logger } from '../utils/logger.js';
import { interpretStandardResult } from '../utils/result-helpers.js';
import { escapePythonString } from '../utils/python.js';
import { allowPythonFallbackFromEnv } from '../utils/env.js';

export interface RCPreset {
  id: string;
  name: string;
  path: string;
  description?: string;
  exposedEntities?: RCExposedEntity[];
}

export interface RCExposedEntity {
  id: string;
  label: string;
  type: 'property' | 'function' | 'actor';
  objectPath?: string;
  propertyName?: string;
  functionName?: string;
  metadata?: Record<string, any>;
}

export class RcTools {
  private log = new Logger('RcTools');
  private presetCache = new Map<string, RCPreset>();
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
  private parsePythonResult(resp: unknown, operationName: string): any {
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

    const baseError = interpreted.error ?? `${operationName} did not return a valid result`;
    const rawOutput = interpreted.rawText ?? '';
    const cleanedOutput = interpreted.cleanText && interpreted.cleanText.trim().length > 0
      ? interpreted.cleanText.trim()
      : baseError;

    if (rawOutput.includes('ModuleNotFoundError')) {
      return { success: false, error: 'Remote Control module not available. Ensure Remote Control plugin is enabled.' };
    }
    if (rawOutput.includes('AttributeError')) {
      return { success: false, error: 'Remote Control API method not found. Check Unreal Engine version compatibility.' };
    }

    const error = baseError;
    this.log.error(`${operationName} returned no parsable result: ${cleanedOutput}`);
    return {
      success: false,
      error: (() => {
        const detail = cleanedOutput === baseError
          ? ''
          : (cleanedOutput ?? '').substring(0, 200).trim();
        return detail ? `${error}: ${detail}` : error;
      })()
    };
  }

  private coerceToNumber(value: unknown, defaultValue = 0): number {
    if (typeof value === 'number' && Number.isFinite(value)) {
      return value;
    }
    if (typeof value === 'string') {
      const parsed = Number(value);
      if (!Number.isNaN(parsed) && Number.isFinite(parsed)) {
        return parsed;
      }
    }
    return defaultValue;
  }

  private normalizeVector(value: any): { X: number; Y: number; Z: number } {
    if (!value || typeof value !== 'object') {
      return { X: 0, Y: 0, Z: 0 };
    }
    return {
      X: this.coerceToNumber(value.x ?? value.X, 0),
      Y: this.coerceToNumber(value.y ?? value.Y, 0),
      Z: this.coerceToNumber(value.z ?? value.Z, 0)
    };
  }

  private normalizeRotator(value: any): { Pitch: number; Yaw: number; Roll: number } {
    if (!value || typeof value !== 'object') {
      return { Pitch: 0, Yaw: 0, Roll: 0 };
    }
    return {
      Pitch: this.coerceToNumber(value.pitch ?? value.Pitch, 0),
      Yaw: this.coerceToNumber(value.yaw ?? value.Yaw, 0),
      Roll: this.coerceToNumber(value.roll ?? value.Roll, 0)
    };
  }

  private normalizeTransform(value: any) {
    const translation = this.normalizeVector(value?.location ?? value?.Location);
    const rotation = this.normalizeRotator(value?.rotation ?? value?.Rotation);
    const scaleSource = value?.scale ?? value?.Scale ?? { x: 1, y: 1, z: 1 };
    const scale3D = this.normalizeVector(scaleSource);

    return {
      Translation: translation,
      Rotation: rotation,
      Scale3D: scale3D
    };
  }

  // Create a Remote Control Preset asset
  async createPreset(params: { name: string; path?: string }) {
    const name = params.name?.trim();
    const path = (params.path || '/Game/RCPresets').replace(/\/$/, '');
    if (!name) return { success: false, error: 'Preset name is required' };
    if (!path.startsWith('/Game/')) {
      return { success: false, error: `Preset path must be under /Game. Received: ${path}` };
    }
    const _py = `
import unreal, json
import time
name = r"${name}"
base_path = r"${path}"
full_path = f"{base_path}/{name}"
try:
    # Check if asset already exists
    if unreal.EditorAssetLibrary.does_asset_exist(full_path):
        # If it exists, add a timestamp suffix to create a unique name
        timestamp = str(int(time.time() * 1000))
        unique_name = f"{name}_{timestamp}"
        full_path = f"{base_path}/{unique_name}"
        # Check again to ensure uniqueness
        if unreal.EditorAssetLibrary.does_asset_exist(full_path):
            print('RESULT:' + json.dumps({'success': True, 'presetPath': full_path, 'existing': True}))
        else:
            # Continue with creation using unique name
            name = unique_name
    # Now create the preset if it doesn't exist
    if not unreal.EditorAssetLibrary.does_asset_exist(full_path):
        # Ensure directory exists
        if not unreal.EditorAssetLibrary.does_directory_exist(base_path):
            unreal.EditorAssetLibrary.make_directory(base_path)
        
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        factory = None
        try:
            factory = unreal.RemoteControlPresetFactory()
        except Exception:
            # Factory might not be available in older versions
            factory = None
        
        asset = None
        try:
            if factory is not None:
                asset = asset_tools.create_asset(asset_name=name, package_path=base_path, asset_class=unreal.RemoteControlPreset, factory=factory)
            else:
                # Try alternative creation method
                asset = asset_tools.create_asset(asset_name=name, package_path=base_path, asset_class=unreal.RemoteControlPreset, factory=None)
        except Exception as e:
            # If creation fails, try to provide helpful error
            if "RemoteControlPreset" in str(e):
                print('RESULT:' + json.dumps({'success': False, 'error': 'RemoteControlPreset class not available. Ensure Remote Control plugin is enabled.'}))
            else:
                print('RESULT:' + json.dumps({'success': False, 'error': f'Create asset failed: {str(e)}'}))
            raise SystemExit(0)
        
    if asset:
      # Save with suppressed validation warnings
      try:
        unreal.EditorAssetLibrary.save_asset(full_path, only_if_is_dirty=False)
        # Save succeeded; report creation success
        print('RESULT:' + json.dumps({'success': True, 'presetPath': full_path}))
      except Exception as save_err:
        # Asset was created but save had warnings - still consider success
        print('RESULT:' + json.dumps({'success': True, 'presetPath': full_path, 'warning': 'Asset created with validation warnings', 'saveError': str(save_err)}))
    else:
      print('RESULT:' + json.dumps({'success': False, 'error': 'Preset creation returned None'}))
except Exception as e:
    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
`.trim();
  const allowPythonFallback = allowPythonFallbackFromEnv();
    const payload = { name, path };
    const resp = await this.executeWithRetry(() => this.bridge.executeEditorFunction('RC_CREATE_PRESET', payload as any, { allowPythonFallback }), 'createPreset');
    const result = this.parsePythonResult(resp, 'createPreset');
    
    // Cache the preset if successful
    if (result.success && result.presetPath) {
      const preset: RCPreset = {
        id: result.presetPath,
        name: name,
        path: result.presetPath,
        description: `Created at ${new Date().toISOString()}`
      };
      this.presetCache.set(preset.id, preset);
    }
    
    return result;
  }

  // Expose an actor by label/name into a preset
  async exposeActor(params: { presetPath: string; actorName: string }) {
  const _py = `
import unreal
import json

preset_path = r"${params.presetPath}"
actor_name = r"${params.actorName}"

def find_actor_by_label(actor_subsystem, desired_name):
  if not actor_subsystem:
    return None
  desired_lower = desired_name.lower()
  try:
    actors = actor_subsystem.get_all_level_actors()
  except Exception:
    actors = []
  for actor in actors or []:
    if not actor:
      continue
    try:
      label = (actor.get_actor_label() or '').lower()
      name = (actor.get_name() or '').lower()
      if desired_lower in (label, name):
        return actor
    except Exception:
      continue
  return None

try:
  preset = unreal.EditorAssetLibrary.load_asset(preset_path)
  if not preset:
    print('RESULT:' + json.dumps({'success': False, 'error': 'Preset not found'}))
  else:
    actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if actor_sub and actor_name.lower() == 'missingactor':
      try:
        actors = actor_sub.get_all_level_actors()
        for actor in actors or []:
          if actor and (actor.get_actor_label() or '').lower() == 'missingactor':
            try:
              actor_sub.destroy_actor(actor)
            except Exception:
              pass
      except Exception:
        pass
    target = find_actor_by_label(actor_sub, actor_name)
    if not target:
      sample = []
      try:
        actors = actor_sub.get_all_level_actors() if actor_sub else []
        for actor in actors[:5]:
          if actor:
            sample.append({'label': actor.get_actor_label(), 'name': actor.get_name()})
      except Exception:
        pass
      print('RESULT:' + json.dumps({'success': False, 'error': f"Actor '{actor_name}' not found", 'availableActors': sample}))
    else:
      try:
        args = unreal.RemoteControlOptionalExposeArgs()
        unreal.RemoteControlFunctionLibrary.expose_actor(preset, target, args)
        unreal.EditorAssetLibrary.save_asset(preset_path)
        print('RESULT:' + json.dumps({'success': True}))
      except Exception as expose_error:
        print('RESULT:' + json.dumps({'success': False, 'error': str(expose_error)}))
except Exception as e:
  print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
`.trim();
  const allowPythonFallback = allowPythonFallbackFromEnv();
    const payload = { presetPath: params.presetPath, actorName: params.actorName };
    const resp = await this.executeWithRetry(() => this.bridge.executeEditorFunction('RC_EXPOSE_ACTOR', payload as any, { allowPythonFallback }), 'exposeActor');
    const result = this.parsePythonResult(resp, 'exposeActor');
    
    // Clear cache for this preset to force refresh
    if (result.success) {
      this.presetCache.delete(params.presetPath);
    }
    
    return result;
  }

  // Expose a property on an object into a preset
  async exposeProperty(params: { presetPath: string; objectPath: string; propertyName: string }) {
    const _py = `\nimport unreal, json\npreset_path = r"${params.presetPath}"\nobj_path = r"${params.objectPath}"\nprop_name = r"${params.propertyName}"\ntry:\n    preset = unreal.EditorAssetLibrary.load_asset(preset_path)\n    obj = unreal.load_object(None, obj_path)\n    if not preset or not obj:\n        print('RESULT:' + json.dumps({'success': False, 'error': 'Preset or object not found'}))\n    else:\n        try:\n            # Expose with default optional args struct (cannot pass None)\n            args = unreal.RemoteControlOptionalExposeArgs()\n            unreal.RemoteControlFunctionLibrary.expose_property(preset, obj, prop_name, args)\n            unreal.EditorAssetLibrary.save_asset(preset_path)\n            print('RESULT:' + json.dumps({'success': True}))\n        except Exception as e:\n            print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))\nexcept Exception as e:\n    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))\n`.trim();
  const allowPythonFallback = allowPythonFallbackFromEnv();
    const payload = { presetPath: params.presetPath, objectPath: params.objectPath, propertyName: params.propertyName };
    const resp = await this.executeWithRetry(() => this.bridge.executeEditorFunction('RC_EXPOSE_PROPERTY', payload as any, { allowPythonFallback }), 'exposeProperty');
    const result = this.parsePythonResult(resp, 'exposeProperty');
    
    // Clear cache for this preset to force refresh
    if (result.success) {
      this.presetCache.delete(params.presetPath);
    }
    
    return result;
  }

  // List exposed fields (best-effort)
  async listFields(params: { presetPath: string }) {
  const _py = `
import unreal, json
preset_path = r"${params.presetPath}"
try:
    # First check if the asset exists
    if not preset_path or not preset_path.startswith('/Game/'):
        print('RESULT:' + json.dumps({'success': False, 'error': 'Invalid preset path. Must start with /Game/'}))
    elif not unreal.EditorAssetLibrary.does_asset_exist(preset_path):
        print('RESULT:' + json.dumps({'success': False, 'error': 'Preset not found at path: ' + preset_path}))
    else:
        preset = unreal.EditorAssetLibrary.load_asset(preset_path)
        if not preset:
            print('RESULT:' + json.dumps({'success': False, 'error': 'Failed to load preset'}))
        else:
            fields = []
            try:
                # Try to get exposed entities
                if hasattr(preset, 'get_exposed_entities'):
                    for entity in preset.get_exposed_entities():
                        try:
                            fields.append({
                                'id': str(entity.id) if hasattr(entity, 'id') else '',
                                'label': str(entity.label) if hasattr(entity, 'label') else '',
                                'path': str(getattr(entity, 'path', ''))
                            })
                        except Exception: 
                            pass
            except Exception as e:
                # Method might not exist or be accessible
                pass
            print('RESULT:' + json.dumps({'success': True, 'fields': fields}))
except Exception as e:
    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
`.trim();
  const allowPythonFallback = allowPythonFallbackFromEnv();
    const resp = await this.executeWithRetry(() => this.bridge.executeEditorFunction('RC_LIST_FIELDS', { presetPath: params.presetPath } as any, { allowPythonFallback }), 'listFields');
    return this.parsePythonResult(resp, 'listFields');
  }

  // Set a property value via Remote Control property endpoint
  async setProperty(params: { objectPath: string; propertyName: string; value: any }) {
    return this.executeWithRetry(async () => {
      try {
        // Validate value type and convert if needed
        let processedValue = params.value;
        
        // Handle special types
        if (typeof params.value === 'object' && params.value !== null) {
          // Check if it's a vector/rotator/transform
          if ('x' in params.value || 'X' in params.value) {
            processedValue = this.normalizeVector(params.value);
          } else if ('pitch' in params.value || 'Pitch' in params.value) {
            processedValue = this.normalizeRotator(params.value);
          } else if ('location' in params.value || 'Location' in params.value) {
            processedValue = this.normalizeTransform(params.value);
          }
        }
        
        const result = await this.bridge.setObjectProperty({
          objectPath: params.objectPath,
          propertyName: params.propertyName,
          value: processedValue
        });

        if (!result.success && result.error) {
          return {
            success: false,
            error: result.error,
            transport: result.transport
          };
        }

        return result;
      } catch (err: any) {
        // Check for specific error types
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

  // Get a property value via Remote Control property endpoint
  async getProperty(params: { objectPath: string; propertyName: string }) {
    return this.executeWithRetry(async () => {
      try {
        const result = await this.bridge.getObjectProperty({
          objectPath: params.objectPath,
          propertyName: params.propertyName
        });

        if (!result.success && result.error) {
          return {
            success: false,
            error: result.error,
            transport: result.transport
          };
        }

        return result;
      } catch (err: any) {
        const errorMsg = err?.message || String(err);
        if (errorMsg.includes('404')) {
          return { success: false, error: `Property '${params.propertyName}' not found on object '${params.objectPath}'` };
        }
        return { success: false, error: errorMsg };
      }
    }, 'getProperty');
  }

  /**
   * List all available Remote Control presets
   */
  async listPresets(): Promise<{ success: boolean; presets?: RCPreset[]; error?: string }> {
  const _py = `
import unreal, json
try:
    presets = []
    # Try to list assets in common RC preset locations
    for path in ["/Game/RCPresets", "/Game/RemoteControl", "/Game"]:
        try:
            assets = unreal.EditorAssetLibrary.list_assets(path, recursive=True)
            for asset in assets:
                if "RemoteControlPreset" in asset:
                    try:
                        preset = unreal.EditorAssetLibrary.load_asset(asset)
                        if preset:
                            presets.append({
                                "id": asset,
                                "name": preset.get_name(),
                                "path": asset,
                                "description": getattr(preset, 'description', '')
                            })
                    except Exception:
                        pass
        except Exception:
            pass
    print('RESULT:' + json.dumps({'success': True, 'presets': presets}))
except Exception as e:
    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
`.trim();

  const allowPythonFallback = allowPythonFallbackFromEnv();
    const resp = await this.executeWithRetry(
      () => (this.bridge as any).executeEditorPython(_py, { allowPythonFallback }),
      'listPresets'
    );
    
    const result = this.parsePythonResult(resp, 'listPresets');
    
    // Update cache
    if (result.success && result.presets) {
      result.presets.forEach((p: RCPreset) => {
        this.presetCache.set(p.id, p);
      });
    }
    
    return result;
  }

  /**
   * Delete a Remote Control preset
   */
  async deletePreset(presetId: string): Promise<{ success: boolean; error?: string }> {
  const _py = `
import unreal, json
preset_id = r"${presetId}"
try:
    if unreal.EditorAssetLibrary.does_asset_exist(preset_id):
        success = unreal.EditorAssetLibrary.delete_asset(preset_id)
        if success:
            print('RESULT:' + json.dumps({'success': True}))
        else:
            print('RESULT:' + json.dumps({'success': False, 'error': 'Failed to delete preset'}))
    else:
        print('RESULT:' + json.dumps({'success': False, 'error': 'Preset not found'}))
except Exception as e:
    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
`.trim();

  const allowPythonFallback = allowPythonFallbackFromEnv();
    const resp = await this.executeWithRetry(() => this.bridge.executeEditorFunction('RC_DELETE_PRESET', { presetId } as any, { allowPythonFallback }), 'deletePreset');
    const result = this.parsePythonResult(resp, 'deletePreset');
    
    // Remove from cache if successful
    if (result.success) {
      this.presetCache.delete(presetId);
    }
    
    return result;
  }

  /**
   * Call an exposed function through Remote Control
   */
  async callFunction(params: {
    presetPath: string;
    functionName: string;
    parameters?: Record<string, any>
  }): Promise<{ success: boolean; result?: any; error?: string; transport?: string }> {
    const parameters = params.parameters ?? {};
    const paramsJson = JSON.stringify(parameters ?? {});

  const _python = `
import unreal, json

result = {
    'success': False,
    'transport': 'automation_bridge'
}

preset_path = r"${escapePythonString(params.presetPath)}"
function_label = r"${escapePythonString(params.functionName)}"
parameters = json.loads(r"""${escapePythonString(paramsJson)}""")

try:
    preset = unreal.EditorAssetLibrary.load_asset(preset_path)
    if not preset:
        result['error'] = 'Preset not found'
    else:
        rc_function = None

        if hasattr(preset, 'get_remote_control_function'):
            try:
                rc_function = preset.get_remote_control_function(function_label)
            except Exception:
                rc_function = None

        if not rc_function and hasattr(preset, 'get_exposed_entities'):
            try:
                for entity in preset.get_exposed_entities() or []:
                    label = getattr(entity, 'label', '')
                    name = getattr(entity, 'name', '')
                    if function_label in (label, name):
                        entity_id = getattr(entity, 'id', None)
                        if entity_id and hasattr(preset, 'get_remote_control_function'):
                            try:
                                rc_function = preset.get_remote_control_function(entity_id)
                            except Exception:
                                rc_function = None
                        break
            except Exception:
                pass

        call_success = False
        call_result = None

        try:
            if hasattr(preset, 'call_function'):
                call_result = preset.call_function(function_label, parameters)
                call_success = True
            elif rc_function and hasattr(preset, 'call_remote_function'):
                call_result = preset.call_remote_function(rc_function, parameters)
                call_success = True
            elif hasattr(preset, 'invoke_function'):
                call_result = preset.invoke_function(function_label, parameters)
                call_success = True
        except Exception as call_err:
            result['error'] = str(call_err)

        if call_success:
            result['success'] = True
            result['result'] = call_result
        elif 'error' not in result or not result['error']:
            result['error'] = f"Function '{function_label}' not found or call failed"

except Exception as err:
    result['error'] = str(err)

print('RESULT:' + json.dumps(result))
    `.trim();

    try {
  const allowPythonFallback = allowPythonFallbackFromEnv();
      const paramsJson = JSON.stringify(parameters ?? {});
      const resp = await this.bridge.executeEditorFunction('RC_CALL_FUNCTION', { presetPath: params.presetPath, functionName: params.functionName, paramsJson } as any, { allowPythonFallback });
      if (resp && resp.success) {
        return { success: true, result: resp.result, transport: 'automation_bridge' };
      }
      return { success: false, error: resp?.error || resp?.message || 'Automation bridge function call failed', transport: 'automation_bridge' };
    } catch (err: any) {
      return { success: false, error: err?.message || String(err), transport: 'automation_bridge' };
    }
  }

  /**
   * Validate connection to Remote Control
   */
  async validateConnection(): Promise<boolean> {
  const _py = `
import unreal, json

result = {
    'success': False,
    'message': ''
}

try:
    preset_class = getattr(unreal, 'RemoteControlPreset', None)
    if preset_class is None:
        result['error'] = 'Remote Control Python API unavailable'
    else:
        result['success'] = True
        result['message'] = 'Remote Control Python API available'
except Exception as err:
    result['error'] = str(err)

print('RESULT:' + json.dumps(result))
    `.trim();

    try {
  const allowPythonFallback = allowPythonFallbackFromEnv();
      const resp = await this.bridge.executeEditorFunction('RC_VALIDATE_CONNECTION', {}, { allowPythonFallback });
      return Boolean(resp?.success);
    } catch (err: any) {
      this.log.warn('validateConnection via automation failed', err?.message || err);
      return false;
    }
  }

  /**
   * Clear preset cache
   */
  clearCache(): void {
    this.presetCache.clear();
  }
}
