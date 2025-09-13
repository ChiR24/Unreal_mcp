import { UnrealBridge } from '../unreal-bridge.js';

export class SequenceTools {
  constructor(private bridge: UnrealBridge) {}

  async create(params: { name: string; path?: string }) {
    const name = params.name?.trim();
    const base = (params.path || '/Game/Sequences').replace(/\/$/, '');
    if (!name) return { success: false, error: 'name is required' };
    const py = `\nimport unreal, json\nname = r"${name}"\nbase = r"${base}"\nfull = f"{base}/{name}"\ntry:\n    # Ensure directory exists\n    try:\n        if not unreal.EditorAssetLibrary.does_directory_exist(base):\n            unreal.EditorAssetLibrary.make_directory(base)\n    except Exception:\n        pass\n\n    if unreal.EditorAssetLibrary.does_asset_exist(full):\n        print('RESULT:' + json.dumps({'success': True, 'sequencePath': full, 'existing': True}))\n    else:\n        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()\n        factory = unreal.LevelSequenceFactoryNew()\n        seq = asset_tools.create_asset(asset_name=name, package_path=base, asset_class=unreal.LevelSequence, factory=factory)\n        if seq:\n            unreal.EditorAssetLibrary.save_asset(full)\n            print('RESULT:' + json.dumps({'success': True, 'sequencePath': full}))\n        else:\n            print('RESULT:' + json.dumps({'success': False, 'error': 'Create returned None'}))\nexcept Exception as e:\n    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))\n`.trim();
const resp = await this.bridge.executePython(py);
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
    return { success: false, error: 'Sequence creation did not return a result' };
  }

  async open(params: { path: string }) {
    const py = `\nimport unreal, json\npath = r"${params.path}"\ntry:\n    seq = unreal.load_asset(path)\n    if not seq:\n        print('RESULT:' + json.dumps({'success': False, 'error': 'Sequence not found'}))\n    else:\n        unreal.LevelSequenceEditorBlueprintLibrary.open_level_sequence(seq)\n        print('RESULT:' + json.dumps({'success': True, 'sequencePath': path}))\nexcept Exception as e:\n    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))\n`.trim();
const resp = await this.bridge.executePython(py);
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
    return { success: false, error: 'Open sequence did not return a result' };
  }

  async addCamera(params: { spawnable?: boolean }) {
    const py = `\nimport unreal, json\ntry:\n    ls = unreal.get_editor_subsystem(unreal.LevelSequenceEditorSubsystem)\n    if not ls:\n        print('RESULT:' + json.dumps({'success': False, 'error': 'LevelSequenceEditorSubsystem unavailable'}))\n    else:\n        cam = ls.create_camera(spawnable=${params.spawnable !== false ? 'True' : 'False'})\n        print('RESULT:' + json.dumps({'success': True, 'cameraBindingId': str(cam.get_binding_id()) if hasattr(cam, 'get_binding_id') else ''}))\nexcept Exception as e:\n    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))\n`.trim();
const resp = await this.bridge.executePython(py);
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
    return { success: false, error: 'Add camera did not return a result' };
  }

  async addActor(params: { actorName: string }) {
    const py = `\nimport unreal, json\ntry:\n    actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)\n    ls = unreal.get_editor_subsystem(unreal.LevelSequenceEditorSubsystem)\n    if not ls or not actor_sub:\n        print('RESULT:' + json.dumps({'success': False, 'error': 'Subsystem unavailable'}))\n    else:\n        target = None\n        for a in actor_sub.get_all_level_actors():\n            if not a: continue\n            if a.get_actor_label() == r"${params.actorName}" or a.get_name() == r"${params.actorName}":\n                target = a; break\n        if not target:\n            print('RESULT:' + json.dumps({'success': False, 'error': 'Actor not found'}))\n        else:\n            bindings = ls.add_actors([target])\n            print('RESULT:' + json.dumps({'success': True, 'count': len(bindings)}))\nexcept Exception as e:\n    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))\n`.trim();
const resp = await this.bridge.executePython(py);
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
    return { success: false, error: 'Add actor did not return a result' };
  }
}
