import { UnrealBridge } from '../unreal-bridge.js';
import { coerceString, interpretStandardResult } from '../utils/result-helpers.js';
import { allowPythonFallbackFromEnv } from '../utils/env.js';

export class LevelResources {
  constructor(private bridge: UnrealBridge) {}

  async getCurrentLevel() {
    // Use UnrealEditorSubsystem instead of deprecated EditorLevelLibrary
    try {
      const py = '\nimport unreal, json\ntry:\n    # Use UnrealEditorSubsystem instead of deprecated EditorLevelLibrary\n    editor_subsys = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)\n    world = editor_subsys.get_editor_world()\n    name = world.get_name() if world else \'None\'\n    path = world.get_path_name() if world else \'None\'\n    print(\'RESULT:\' + json.dumps({\'success\': True, \'name\': name, \'path\': path}))\nexcept Exception as e:\n    print(\'RESULT:\' + json.dumps({\'success\': False, \'error\': str(e)}))\n'.trim();
  const allowPythonFallback = allowPythonFallbackFromEnv();
  const resp: any = await (this.bridge as any).executeEditorPython(py, { allowPythonFallback });
      const interpreted = interpretStandardResult(resp, {
        successMessage: 'Retrieved current level',
        failureMessage: 'Failed to get current level'
      });

      if (interpreted.success) {
        return {
          success: true,
          name: coerceString(interpreted.payload.name) ?? coerceString(interpreted.payload.level_name) ?? 'None',
          path: coerceString(interpreted.payload.path) ?? 'None'
        };
      }

      return { success: false, error: interpreted.error ?? interpreted.message };
    } catch (err) {
      return { error: `Failed to get current level: ${err}`, success: false };
    }
  }

  async getLevelName() {
    // Return camera/world info via Python first
    try {
      const py = '\nimport unreal, json\ntry:\n    # Use UnrealEditorSubsystem instead of deprecated EditorLevelLibrary\n    editor_subsys = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)\n    world = editor_subsys.get_editor_world()\n    path = world.get_path_name() if world else \'\'\n    print(\'RESULT:\' + json.dumps({\'success\': True, \'path\': path}))\nexcept Exception as e:\n    print(\'RESULT:\' + json.dumps({\'success\': False, \'error\': str(e)}))\n'.trim();
  const allowPythonFallback = allowPythonFallbackFromEnv();
  const resp: any = await (this.bridge as any).executeEditorPython(py, { allowPythonFallback });
      const interpreted = interpretStandardResult(resp, {
        successMessage: 'Retrieved level path',
        failureMessage: 'Failed to get level name'
      });

      if (interpreted.success) {
        return {
          success: true,
          path: coerceString(interpreted.payload.path) ?? ''
        };
      }

      return { success: false, error: interpreted.error ?? interpreted.message };
    } catch (err) {
      return { error: `Failed to get level name: ${err}`, success: false };
    }
  }

  async saveCurrentLevel() {
    // Strict modern API: require LevelEditorSubsystem
    try {
      const py = '\nimport unreal, json\ntry:\n    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)\n    if not les:\n        print(\'RESULT:\' + json.dumps({\'success\': False, \'error\': \'LevelEditorSubsystem not available\'}))\n    else:\n        les.save_current_level()\n        print(\'RESULT:\' + json.dumps({\'success\': True}))\nexcept Exception as e:\n    print(\'RESULT:\' + json.dumps({\'success\': False, \'error\': str(e)}))\n'.trim();
  const allowPythonFallback = allowPythonFallbackFromEnv();
  const resp: any = await (this.bridge as any).executeEditorPython(py, { allowPythonFallback });
      const interpreted = interpretStandardResult(resp, {
        successMessage: 'Level saved',
        failureMessage: 'Failed to save level'
      });

      if (interpreted.success) {
        return { success: true, message: interpreted.message };
      }

      return { success: false, error: interpreted.error ?? interpreted.message };
    } catch (err) {
      return { error: `Failed to save level: ${err}`, success: false };
    }
  }
}
