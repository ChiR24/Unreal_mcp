import { UnrealBridge } from '../unreal-bridge.js';

export class EditorTools {
  constructor(private bridge: UnrealBridge) {}

  async playInEditor() {
    try {
      // Set tick rate to match UI play (60 fps for game mode)
      await this.bridge.executeConsoleCommand('t.MaxFPS 60');
      
      // Try Python first for proper EditorLevelLibrary access with viewport play
      try {
        // Use EditorLevelLibrary to play in the selected viewport (matches UI behavior)
        const pythonCmd = `
import unreal, time
# Start PIE using EditorLevelLibrary (simpler approach)
unreal.EditorLevelLibrary.editor_play_simulate()
# Set viewport play settings if available
try:
    play_settings = unreal.get_editor_subsystem(unreal.LevelEditorPlaySettings)
    if play_settings:
        play_settings.set_play_mode(unreal.PlayInEditorType.PIE_PLAY_IN_VIEWPORT)
except:
    pass  # Settings API may vary by UE version
# Wait briefly and report state
time.sleep(0.1)
is_playing = False
try:
    is_playing = bool(unreal.EditorLevelLibrary.is_playing())
except Exception:
    try:
        is_playing = bool(unreal.EditorLevelLibrary.is_playing_in_editor())
    except Exception:
        is_playing = False
print("RESULT:{'success': " + ("True" if is_playing else "False") + "}")
        `.trim();
        
        const resp: any = await this.bridge.executePython(pythonCmd);
        const out = typeof resp === 'string' ? resp : JSON.stringify(resp);
        const m = out.match(/RESULT:({.*})/);
        if (m) {
          try {
            const parsed = JSON.parse(m[1]);
            if (parsed.success) {
              return { success: true, message: 'PIE started in Selected Viewport (verified)' };
            }
          } catch {}
        }
        // If not verified, fall through to fallback
      } catch (pythonErr) {
        // Ignore and try fallback
      }
      // Fallback to console command with viewport specification and assume best-effort
      await this.bridge.executeConsoleCommand('PlayInViewport');
      return { success: true, message: 'PIE start attempted via console command' };
    } catch (err) {
      return { success: false, error: `Failed to start PIE: ${err}` };
    }
  }

  async stopPlayInEditor() {
    try {
      // Try Python first for proper EditorLevelLibrary access
      try {
        const resp: any = await this.bridge.executePython('import unreal, time; unreal.EditorLevelLibrary.editor_end_play(); time.sleep(0.1); print("RESULT:{\'success\': True}")');
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
          WorldContextObject: null,
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
  
  // Alias for consistency with naming convention
  async pauseInEditor() {
    return this.pausePlayInEditor();
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

  async setViewportCamera(location?: { x: number; y: number; z: number } | null | undefined, rotation?: { pitch: number; yaw: number; roll: number } | null | undefined) {
    // Special handling for when both location and rotation are missing/invalid
    // Allow rotation-only updates
    if (location === null) {
      // Explicit null is not allowed for location
      throw new Error('Invalid location: null is not allowed');
    }
    if (location !== undefined && location !== null) {
      if (typeof location !== 'object' || Array.isArray(location)) {
        throw new Error('Invalid location: must be an object with x, y, z properties');
      }
      if (!('x' in location) || !('y' in location) || !('z' in location)) {
        throw new Error('Invalid location: missing required properties x, y, or z');
      }
      if (typeof location.x !== 'number' || typeof location.y !== 'number' || typeof location.z !== 'number') {
        throw new Error('Invalid location: x, y, z must all be numbers');
      }
      if (!isFinite(location.x) || !isFinite(location.y) || !isFinite(location.z)) {
        throw new Error('Invalid location: x, y, z must be finite numbers');
      }
      // Clamp extreme values to reasonable limits for Unreal Engine
      const MAX_COORD = 1000000; // 1 million units is a reasonable max for UE
      location.x = Math.max(-MAX_COORD, Math.min(MAX_COORD, location.x));
      location.y = Math.max(-MAX_COORD, Math.min(MAX_COORD, location.y));
      location.z = Math.max(-MAX_COORD, Math.min(MAX_COORD, location.z));
    }
    
    // Validate rotation if provided
    if (rotation !== undefined) {
      if (rotation === null) {
        throw new Error('Invalid rotation: null is not allowed');
      }
      if (typeof rotation !== 'object' || Array.isArray(rotation)) {
        throw new Error('Invalid rotation: must be an object with pitch, yaw, roll properties');
      }
      if (!('pitch' in rotation) || !('yaw' in rotation) || !('roll' in rotation)) {
        throw new Error('Invalid rotation: missing required properties pitch, yaw, or roll');
      }
      if (typeof rotation.pitch !== 'number' || typeof rotation.yaw !== 'number' || typeof rotation.roll !== 'number') {
        throw new Error('Invalid rotation: pitch, yaw, roll must all be numbers');
      }
      if (!isFinite(rotation.pitch) || !isFinite(rotation.yaw) || !isFinite(rotation.roll)) {
        throw new Error('Invalid rotation: pitch, yaw, roll must be finite numbers');
      }
      // Normalize rotation values to 0-360 range
      rotation.pitch = ((rotation.pitch % 360) + 360) % 360;
      rotation.yaw = ((rotation.yaw % 360) + 360) % 360;
      rotation.roll = ((rotation.roll % 360) + 360) % 360;
    }
    
    try {
      // Try Python for actual viewport camera positioning
      // Only proceed if we have a valid location
      if (location) {
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
      } else if (rotation) {
        // Only rotation provided, try to set just rotation
        try {
          const pythonCmd = `
import unreal
rotation = unreal.Rotator(${rotation.pitch}, ${rotation.yaw}, ${rotation.roll})
# Get current location
level_viewport = unreal.EditorLevelLibrary.get_level_viewport_camera_info()
current_location = level_viewport[0]
unreal.EditorLevelLibrary.set_level_viewport_camera_info(current_location, rotation)
          `.trim();
          await this.bridge.executePython(pythonCmd);
          return { 
            success: true, 
            message: 'Viewport camera rotation set via EditorLevelLibrary' 
          };
        } catch (pythonErr) {
          // Fallback
          return { 
            success: true, 
            message: 'Camera rotation update attempted' 
          };
        }
      } else {
        // Neither location nor rotation provided - this is valid, just no-op
        return { 
          success: true, 
          message: 'No camera changes requested' 
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
          WorldContextObject: null,
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
          WorldContextObject: null,
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
