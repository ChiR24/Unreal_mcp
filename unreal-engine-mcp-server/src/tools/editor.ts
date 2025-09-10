import { UnrealBridge } from '../unreal-bridge.js';

export class EditorTools {
  constructor(private bridge: UnrealBridge) {}

  async playInEditor() {
    try {
      // Try Python first for proper EditorLevelLibrary access
      try {
        await this.bridge.executePython('import unreal; unreal.EditorLevelLibrary.editor_play_simulate()');
        return { success: true, message: 'PIE started via EditorLevelLibrary' };
      } catch (pythonErr) {
        // Fallback to console command
        await this.bridge.executeConsoleCommand('play');
        return { success: true, message: 'PIE started via console command' };
      }
    } catch (err) {
      return { success: false, error: `Failed to start PIE: ${err}` };
    }
  }

  async stopPlayInEditor() {
    try {
      // Try Python first for proper EditorLevelLibrary access
      try {
        await this.bridge.executePython('import unreal; unreal.EditorLevelLibrary.editor_end_play()');
        return { success: true, message: 'PIE stopped via EditorLevelLibrary' };
      } catch (pythonErr) {
        // Fallback to console command
        await this.bridge.executeConsoleCommand('stop');
        return { success: true, message: 'PIE stopped via console command' };
      }
    } catch (err) {
      return { success: false, error: `Failed to stop PIE: ${err}` };
    }
  }
  
  async pausePlayInEditor() {
    try {
      // Pause/Resume PIE
      const res = await this.bridge.httpCall('/remote/object/call', 'PUT', {
        objectPath: '/Script/Engine.Default__KismetSystemLibrary',
        functionName: 'ExecuteConsoleCommand',
        parameters: {
          Command: 'pause',
          SpecificPlayer: null
        },
        generateTransaction: false
      });
      return { success: true, message: 'PIE paused/resumed' };
    } catch (err) {
      return { success: false, error: `Failed to pause PIE: ${err}` };
    }
  }

  async buildLighting() {
    try {
      const res = await this.bridge.httpCall('/remote/object/call', 'PUT', {
        objectPath: '/Script/Engine.Default__KismetSystemLibrary',
        functionName: 'ExecuteConsoleCommand',
        parameters: {
          WorldContextObject: null,
          Command: 'BuildLighting'
        }
      });
      return { success: true, message: 'Lighting build started' };
    } catch (err) {
      return { success: false, error: `Failed to build lighting: ${err}` };
    }
  }

  async setViewportCamera(location: { x: number; y: number; z: number }, rotation?: { pitch: number; yaw: number; roll: number }) {
    try {
      // Try Python for actual viewport camera positioning
      try {
        const rot = rotation || { pitch: 0, yaw: 0, roll: 0 };
        const pythonCmd = `
import unreal
location = unreal.Vector(${location.x}, ${location.y}, ${location.z})
rotation = unreal.Rotator(${rot.pitch}, ${rot.yaw}, ${rot.roll})
unreal.EditorLevelLibrary.set_level_viewport_camera_info(location, rotation)
        `.trim();
        await this.bridge.executePython(pythonCmd);
        return { 
          success: true, 
          message: 'Viewport camera positioned via EditorLevelLibrary' 
        };
      } catch (pythonErr) {
        // Fallback to camera speed control
        await this.bridge.executeConsoleCommand('camspeed 4');
        return { 
          success: true, 
          message: 'Camera speed set. Use debug camera (toggledebugcamera) for manual positioning' 
        };
      }
    } catch (err) {
      return { success: false, error: `Failed to set camera: ${err}` };
    }
  }
  
  async setCameraSpeed(speed: number) {
    try {
      const res = await this.bridge.httpCall('/remote/object/call', 'PUT', {
        objectPath: '/Script/Engine.Default__KismetSystemLibrary',
        functionName: 'ExecuteConsoleCommand',
        parameters: {
          Command: `camspeed ${speed}`,
          SpecificPlayer: null
        },
        generateTransaction: false
      });
      return { success: true, message: `Camera speed set to ${speed}` };
    } catch (err) {
      return { success: false, error: `Failed to set camera speed: ${err}` };
    }
  }
  
  async setFOV(fov: number) {
    try {
      const res = await this.bridge.httpCall('/remote/object/call', 'PUT', {
        objectPath: '/Script/Engine.Default__KismetSystemLibrary',
        functionName: 'ExecuteConsoleCommand',
        parameters: {
          Command: `fov ${fov}`,
          SpecificPlayer: null
        },
        generateTransaction: false
      });
      return { success: true, message: `FOV set to ${fov}` };
    } catch (err) {
      return { success: false, error: `Failed to set FOV: ${err}` };
    }
  }
}
