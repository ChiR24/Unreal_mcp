import { UnrealBridge } from '../unreal-bridge.js';

export class LevelResources {
  constructor(private bridge: UnrealBridge) {}

  async getCurrentLevel() {
    // Use UnrealEditorSubsystem instead of deprecated EditorLevelLibrary
    try {
      const py = `\nimport unreal, json\ntry:\n    # Use UnrealEditorSubsystem instead of deprecated EditorLevelLibrary\n    editor_subsys = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)\n    world = editor_subsys.get_editor_world()\n    name = world.get_name() if world else 'None'\n    path = world.get_path_name() if world else 'None'\n    print('RESULT:' + json.dumps({'success': True, 'name': name, 'path': path}))\nexcept Exception as e:\n    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))\n`.trim();
      const resp: any = await this.bridge.executePython(py);
      // Handle LogOutput format from executePython
      let out = '';
      if (resp?.LogOutput && Array.isArray(resp.LogOutput)) {
        out = resp.LogOutput.map((log: any) => log.Output || '').join('');
      } else if (typeof resp === 'string') {
        out = resp;
      } else {
        out = JSON.stringify(resp);
      }
      const m = out.match(/RESULT:({.*})/);
      if (m) { 
        const parsed = JSON.parse(m[1]); 
        if (parsed.success) return parsed;
      }
      // If Python failed, return error
      return { error: 'Failed to get current level', success: false };
    } catch (err) {
      return { error: `Failed to get current level: ${err}`, success: false };
    }
  }

  async getLevelName() {
    // Return camera/world info via Python first
    try {
      const py = `\nimport unreal, json\ntry:\n    # Use UnrealEditorSubsystem instead of deprecated EditorLevelLibrary\n    editor_subsys = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)\n    world = editor_subsys.get_editor_world()\n    path = world.get_path_name() if world else ''\n    print('RESULT:' + json.dumps({'success': True, 'path': path}))\nexcept Exception as e:\n    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))\n`.trim();
      const resp: any = await this.bridge.executePython(py);
      // Handle LogOutput format from executePython
      let out = '';
      if (resp?.LogOutput && Array.isArray(resp.LogOutput)) {
        out = resp.LogOutput.map((log: any) => log.Output || '').join('');
      } else if (typeof resp === 'string') {
        out = resp;
      } else {
        out = JSON.stringify(resp);
      }
      const m = out.match(/RESULT:({.*})/);
      if (m) { 
        const parsed = JSON.parse(m[1]); 
        if (parsed.success) return parsed;
      }
      // If Python failed, return error
      return { error: 'Failed to get level name', success: false };
    } catch (err) {
      return { error: `Failed to get level name: ${err}`, success: false };
    }
  }

  async saveCurrentLevel() {
    // Prefer Python save (or LevelEditorSubsystem) then fallback
    try {
      const py = `\nimport unreal, json\ntry:\n    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)\n    if les: les.save_current_level()\n    else: unreal.EditorLevelLibrary.save_current_level()\n    print('RESULT:' + json.dumps({'success': True}))\nexcept Exception as e:\n    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))\n`.trim();
      const resp: any = await this.bridge.executePython(py);
      // Handle LogOutput format from executePython
      let out = '';
      if (resp?.LogOutput && Array.isArray(resp.LogOutput)) {
        out = resp.LogOutput.map((log: any) => log.Output || '').join('');
      } else if (typeof resp === 'string') {
        out = resp;
      } else {
        out = JSON.stringify(resp);
      }
      const m = out.match(/RESULT:({.*})/);
      if (m) { 
        const parsed = JSON.parse(m[1]); 
        if (parsed.success) return { success: true, message: 'Level saved' };
      }
      // If Python failed, return error
      return { error: 'Failed to save level', success: false };
    } catch (err) {
      return { error: `Failed to save level: ${err}`, success: false };
    }
  }
}
