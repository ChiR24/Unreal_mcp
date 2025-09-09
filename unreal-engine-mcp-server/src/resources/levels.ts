import { UnrealBridge } from '../unreal-bridge.js';

export class LevelResources {
  constructor(private bridge: UnrealBridge) {}

  async getCurrentLevel() {
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
