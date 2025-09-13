import { UnrealBridge } from '../unreal-bridge.js';
import { Logger } from '../utils/logger.js';

export class RcTools {
  private log = new Logger('RcTools');
  constructor(private bridge: UnrealBridge) {}

  // Create a Remote Control Preset asset
  async createPreset(params: { name: string; path?: string }) {
    const name = params.name?.trim();
    const path = (params.path || '/Game/RCPresets').replace(/\/$/, '');
    if (!name) return { success: false, error: 'Preset name is required' };
    const python = `\nimport unreal, json\nname = r"${name}"\nbase_path = r"${path}"\nfull_path = f"{base_path}/{name}"\ntry:\n    if unreal.EditorAssetLibrary.does_asset_exist(full_path):\n        print('RESULT:' + json.dumps({'success': True, 'presetPath': full_path, 'existing': True}))\n    else:\n        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()\n        factory = None\n        try:\n            factory = unreal.RemoteControlPresetFactory()\n        except Exception:\n            factory = None\n        asset = None\n        try:\n            if factory is not None:\n                asset = asset_tools.create_asset(asset_name=name, package_path=base_path, asset_class=unreal.RemoteControlPreset, factory=factory)\n            else:\n                asset = asset_tools.create_asset(asset_name=name, package_path=base_path, asset_class=unreal.RemoteControlPreset, factory=None)\n        except Exception as e:\n            print('RESULT:' + json.dumps({'success': False, 'error': f'Create asset failed: {str(e)}'}))\n            raise SystemExit(0)\n        if asset:\n            unreal.EditorAssetLibrary.save_asset(full_path)\n            print('RESULT:' + json.dumps({'success': True, 'presetPath': full_path}))\n        else:\n            print('RESULT:' + json.dumps({'success': False, 'error': 'Preset creation returned None'}))\nexcept Exception as e:\n    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))\n`.trim();
const resp = await this.bridge.executePython(python);
    let out = '';
    if (resp?.LogOutput && Array.isArray((resp as any).LogOutput)) {
      out = (resp as any).LogOutput.map((l: any) => l.Output || '').join('');
    } else if (typeof resp === 'string') {
      out = resp;
    } else {
      out = JSON.stringify(resp);
    }
    const m = out.match(/RESULT:({.*})/);
    if (m) { try { const parsed = JSON.parse(m[1]); return parsed; } catch {} }
    return { success: false, error: 'Preset creation did not return a result' };
  }

  // Expose an actor by label/name into a preset
  async exposeActor(params: { presetPath: string; actorName: string }) {
    const python = `\nimport unreal, json\npreset_path = r"${params.presetPath}"\nactor_name = r"${params.actorName}"\ntry:\n    preset = unreal.EditorAssetLibrary.load_asset(preset_path)\n    if not preset:\n        print('RESULT:' + json.dumps({'success': False, 'error': 'Preset not found'}))\n    else:\n        actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)\n        target = None\n        for a in actor_sub.get_all_level_actors():\n            if not a: continue\n            try:\n                if a.get_actor_label() == actor_name or a.get_name() == actor_name:\n                    target = a; break\n            except Exception: pass\n        if not target:\n            print('RESULT:' + json.dumps({'success': False, 'error': 'Actor not found'}))\n        else:\n            try:\n                unreal.RemoteControlFunctionLibrary.expose_actor(preset, target, None)\n                unreal.EditorAssetLibrary.save_asset(preset_path)\n                print('RESULT:' + json.dumps({'success': True}))\n            except Exception as e:\n                print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))\nexcept Exception as e:\n    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))\n`.trim();
const resp = await this.bridge.executePython(python);
    let out = '';
    if (resp?.LogOutput && Array.isArray((resp as any).LogOutput)) {
      out = (resp as any).LogOutput.map((l: any) => l.Output || '').join('');
    } else if (typeof resp === 'string') {
      out = resp;
    } else {
      out = JSON.stringify(resp);
    }
    const m = out.match(/RESULT:({.*})/);
    if (m) { try { return JSON.parse(m[1]); } catch {} }
    return { success: false, error: 'Expose actor did not return a result' };
  }

  // Expose a property on an object into a preset
  async exposeProperty(params: { presetPath: string; objectPath: string; propertyName: string }) {
    const python = `\nimport unreal, json\npreset_path = r"${params.presetPath}"\nobj_path = r"${params.objectPath}"\nprop_name = r"${params.propertyName}"\ntry:\n    preset = unreal.EditorAssetLibrary.load_asset(preset_path)\n    obj = unreal.load_object(None, obj_path)\n    if not preset or not obj:\n        print('RESULT:' + json.dumps({'success': False, 'error': 'Preset or object not found'}))\n    else:\n        try:\n            unreal.RemoteControlFunctionLibrary.expose_property(preset, obj, prop_name, None)\n            unreal.EditorAssetLibrary.save_asset(preset_path)\n            print('RESULT:' + json.dumps({'success': True}))\n        except Exception as e:\n            print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))\nexcept Exception as e:\n    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))\n`.trim();
const resp = await this.bridge.executePython(python);
    let out = '';
    if (resp?.LogOutput && Array.isArray((resp as any).LogOutput)) {
      out = (resp as any).LogOutput.map((l: any) => l.Output || '').join('');
    } else if (typeof resp === 'string') {
      out = resp;
    } else {
      out = JSON.stringify(resp);
    }
    const m = out.match(/RESULT:({.*})/);
    if (m) { try { return JSON.parse(m[1]); } catch {} }
    return { success: false, error: 'Expose property did not return a result' };
  }

  // List exposed fields (best-effort)
  async listFields(params: { presetPath: string }) {
    const python = `\nimport unreal, json\npreset_path = r"${params.presetPath}"\ntry:\n    preset = unreal.EditorAssetLibrary.load_asset(preset_path)\n    if not preset:\n        print('RESULT:' + json.dumps({'success': False, 'error': 'Preset not found'}))\n    else:\n        fields = []\n        try:\n            for entity in preset.get_exposed_entities():\n                try:\n                    fields.append({\n                        'id': str(entity.id) if hasattr(entity, 'id') else '',\n                        'label': str(entity.label) if hasattr(entity, 'label') else '',\n                        'path': str(getattr(entity, 'path', ''))\n                    })\n                except Exception: pass\n        except Exception:\n            pass\n        print('RESULT:' + json.dumps({'success': True, 'fields': fields}))\nexcept Exception as e:\n    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))\n`.trim();
const resp = await this.bridge.executePython(python);
    let out = '';
    if (resp?.LogOutput && Array.isArray((resp as any).LogOutput)) {
      out = (resp as any).LogOutput.map((l: any) => l.Output || '').join('');
    } else if (typeof resp === 'string') {
      out = resp;
    } else {
      out = JSON.stringify(resp);
    }
    const m = out.match(/RESULT:({.*})/);
    if (m) { try { return JSON.parse(m[1]); } catch {} }
    return { success: false, error: 'Failed to list fields' };
  }

  // Set a property value via Remote Control property endpoint
  async setProperty(params: { objectPath: string; propertyName: string; value: any }) {
    try {
      const res = await this.bridge.httpCall('/remote/object/property', 'PUT', {
        objectPath: params.objectPath,
        propertyName: params.propertyName,
        propertyValue: params.value
      });
      return { success: true, result: res };
    } catch (err: any) {
      return { success: false, error: String(err?.message || err) };
    }
  }

  // Get a property value via Remote Control property endpoint
  async getProperty(params: { objectPath: string; propertyName: string }) {
    try {
      const res = await this.bridge.httpCall('/remote/object/property', 'GET', {
        objectPath: params.objectPath,
        propertyName: params.propertyName
      });
      return { success: true, value: res };
    } catch (err: any) {
      return { success: false, error: String(err?.message || err) };
    }
  }
}
