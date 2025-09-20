import { UnrealBridge } from '../unreal-bridge.js';
import { Logger } from '../utils/logger.js';

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
  private parsePythonResult(resp: any, operationName: string): any {
    let out = '';
    if (resp?.LogOutput && Array.isArray((resp as any).LogOutput)) {
      out = (resp as any).LogOutput.map((l: any) => l.Output || '').join('');
    } else if (typeof resp === 'string') {
      out = resp;
    } else {
      out = JSON.stringify(resp);
    }
    
    const m = out.match(/RESULT:({.*})/);
    if (m) {
      try {
        return JSON.parse(m[1]);
      } catch (e) {
        this.log.error(`Failed to parse ${operationName} result: ${e}`);
      }
    }
    
    // Check for common error patterns
    if (out.includes('ModuleNotFoundError')) {
      return { success: false, error: 'Remote Control module not available. Ensure Remote Control plugin is enabled.' };
    }
    if (out.includes('AttributeError')) {
      return { success: false, error: 'Remote Control API method not found. Check Unreal Engine version compatibility.' };
    }
    
    return { success: false, error: `${operationName} did not return a valid result: ${out.substring(0, 200)}` };
  }

  // Create a Remote Control Preset asset
  async createPreset(params: { name: string; path?: string }) {
    const name = params.name?.trim();
    const path = (params.path || '/Game/RCPresets').replace(/\/$/, '');
    if (!name) return { success: false, error: 'Preset name is required' };
    const python = `
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
                print('RESULT:' + json.dumps({'success': True, 'presetPath': full_path}))
            except Exception as save_err:
                # Asset was created but save had warnings - still consider success
                print('RESULT:' + json.dumps({'success': True, 'presetPath': full_path, 'warning': 'Asset created with validation warnings'}))
        else:
            print('RESULT:' + json.dumps({'success': False, 'error': 'Preset creation returned None'}))
except Exception as e:
    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
`.trim();
    const resp = await this.executeWithRetry(
      () => this.bridge.executePython(python),
      'createPreset'
    );
    
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
    const python = `\nimport unreal, json\npreset_path = r"${params.presetPath}"\nactor_name = r"${params.actorName}"\ntry:\n    preset = unreal.EditorAssetLibrary.load_asset(preset_path)\n    if not preset:\n        print('RESULT:' + json.dumps({'success': False, 'error': 'Preset not found'}))\n    else:\n        actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)\n        target = None\n        for a in actor_sub.get_all_level_actors():\n            if not a: continue\n            try:\n                if a.get_actor_label() == actor_name or a.get_name() == actor_name:\n                    target = a; break\n            except Exception: pass\n        if not target:\n            print('RESULT:' + json.dumps({'success': False, 'error': 'Actor not found'}))\n        else:\n            try:\n                # Expose with a default-initialized optional args struct (cannot pass None)\n                args = unreal.RemoteControlOptionalExposeArgs()\n                unreal.RemoteControlFunctionLibrary.expose_actor(preset, target, args)\n                unreal.EditorAssetLibrary.save_asset(preset_path)\n                print('RESULT:' + json.dumps({'success': True}))\n            except Exception as e:\n                print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))\nexcept Exception as e:\n    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))\n`.trim();
    const resp = await this.executeWithRetry(
      () => this.bridge.executePython(python),
      'exposeActor'
    );
    
    const result = this.parsePythonResult(resp, 'exposeActor');
    
    // Clear cache for this preset to force refresh
    if (result.success) {
      this.presetCache.delete(params.presetPath);
    }
    
    return result;
  }

  // Expose a property on an object into a preset
  async exposeProperty(params: { presetPath: string; objectPath: string; propertyName: string }) {
    const python = `\nimport unreal, json\npreset_path = r"${params.presetPath}"\nobj_path = r"${params.objectPath}"\nprop_name = r"${params.propertyName}"\ntry:\n    preset = unreal.EditorAssetLibrary.load_asset(preset_path)\n    obj = unreal.load_object(None, obj_path)\n    if not preset or not obj:\n        print('RESULT:' + json.dumps({'success': False, 'error': 'Preset or object not found'}))\n    else:\n        try:\n            # Expose with default optional args struct (cannot pass None)\n            args = unreal.RemoteControlOptionalExposeArgs()\n            unreal.RemoteControlFunctionLibrary.expose_property(preset, obj, prop_name, args)\n            unreal.EditorAssetLibrary.save_asset(preset_path)\n            print('RESULT:' + json.dumps({'success': True}))\n        except Exception as e:\n            print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))\nexcept Exception as e:\n    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))\n`.trim();
    const resp = await this.executeWithRetry(
      () => this.bridge.executePython(python),
      'exposeProperty'
    );
    
    const result = this.parsePythonResult(resp, 'exposeProperty');
    
    // Clear cache for this preset to force refresh
    if (result.success) {
      this.presetCache.delete(params.presetPath);
    }
    
    return result;
  }

  // List exposed fields (best-effort)
  async listFields(params: { presetPath: string }) {
    const python = `
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
    const resp = await this.executeWithRetry(
      () => this.bridge.executePython(python),
      'listFields'
    );
    
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
            processedValue = {
              X: params.value.x || params.value.X || 0,
              Y: params.value.y || params.value.Y || 0,
              Z: params.value.z || params.value.Z || 0
            };
          }
        }
        
        const res = await this.bridge.httpCall('/remote/object/property', 'PUT', {
          objectPath: params.objectPath,
          propertyName: params.propertyName,
          propertyValue: processedValue
        });
        return { success: true, result: res };
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
        const res = await this.bridge.httpCall('/remote/object/property', 'GET', {
          objectPath: params.objectPath,
          propertyName: params.propertyName
        });
        return { success: true, value: res };
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
    const python = `
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

    const resp = await this.executeWithRetry(
      () => this.bridge.executePython(python),
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
    const python = `
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

    const resp = await this.executeWithRetry(
      () => this.bridge.executePython(python),
      'deletePreset'
    );
    
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
  }): Promise<{ success: boolean; result?: any; error?: string }> {
    try {
      const res = await this.bridge.httpCall('/remote/object/call', 'PUT', {
        objectPath: params.presetPath,
        functionName: params.functionName,
        parameters: params.parameters || {}
      });
      return { success: true, result: res };
    } catch (err: any) {
      return { success: false, error: String(err?.message || err) };
    }
  }

  /**
   * Validate connection to Remote Control
   */
  async validateConnection(): Promise<boolean> {
    try {
      await this.bridge.httpCall('/remote/info', 'GET', {});
      return true;
    } catch {
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
