import { UnrealBridge } from '../unreal-bridge.js';

export class LevelResources {
  constructor(private bridge: UnrealBridge) {}

  async getCurrentLevel() {
    // Prefer Python EditorLevelLibrary/LevelEditorSubsystem for reliability
    try {
      const py = `\nimport unreal, json\ntry:\n    world = unreal.EditorLevelLibrary.get_editor_world()\n    name = world.get_name() if world else 'None'\n    path = world.get_path_name() if world else 'None'\n    print('RESULT:' + json.dumps({'success': True, 'name': name, 'path': path}))\nexcept Exception as e:\n    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))\n`.trim();
      const resp: any = await this.bridge.executePython(py);
      const out = typeof resp === 'string' ? resp : JSON.stringify(resp);
      const m = out.match(/RESULT:({.*})/);
      if (m) { const parsed = JSON.parse(m[1]); if (parsed.success) return parsed; }
    } catch {}
    // Fallback to HTTP
    try {
      const res = await this.bridge.call({
        objectPath: '/Script/UnrealEd.Default__EditorLevelLibrary',
        functionName: 'GetEditorWorld',
        parameters: {}
      });
      return res?.Result ?? res;
    } catch (err) {
      return { error: `Failed to get current level: ${err}` };
    }
  }

  async getLevelName() {
    // Return camera/world info via Python first
    try {
      const py = `\nimport unreal, json\ntry:\n    world = unreal.EditorLevelLibrary.get_editor_world()\n    path = world.get_path_name() if world else ''\n    print('RESULT:' + json.dumps({'success': True, 'path': path}))\nexcept Exception as e:\n    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))\n`.trim();
      const resp: any = await this.bridge.executePython(py);
      const out = typeof resp === 'string' ? resp : JSON.stringify(resp);
      const m = out.match(/RESULT:({.*})/);
      if (m) { const parsed = JSON.parse(m[1]); if (parsed.success) return parsed; }
    } catch {}
    // Fallback to HTTP
    try {
      const res = await this.bridge.call({
        objectPath: '/Script/UnrealEd.Default__EditorLevelLibrary',
        functionName: 'GetLevelViewportCameraInfo',
        parameters: {}
      });
      return res?.Result ?? res;
    } catch (err) {
      return { error: `Failed to get level info: ${err}` };
    }
  }

  async saveCurrentLevel() {
    // Prefer Python save (or LevelEditorSubsystem) then fallback
    try {
      const py = `\nimport unreal, json\ntry:\n    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)\n    if les: les.save_current_level()\n    else: unreal.EditorLevelLibrary.save_current_level()\n    print('RESULT:' + json.dumps({'success': True}))\nexcept Exception as e:\n    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))\n`.trim();
      const resp: any = await this.bridge.executePython(py);
      const out = typeof resp === 'string' ? resp : JSON.stringify(resp);
      const m = out.match(/RESULT:({.*})/);
      if (m) { const parsed = JSON.parse(m[1]); if (parsed.success) return { success: true, message: 'Level saved' }; }
    } catch {}
    // Fallback to HTTP
    try {
      const res = await this.bridge.call({
        objectPath: '/Script/UnrealEd.Default__EditorLevelLibrary',
        functionName: 'SaveCurrentLevel',
        parameters: {}
      });
      return res?.Result ?? res;
    } catch (err) {
      return { error: `Failed to save level: ${err}` };
    }
  }
}
