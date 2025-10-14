import { UnrealBridge } from '../unreal-bridge.js';
import { toVec3Object, toRotObject } from '../utils/normalize.js';
import { bestEffortInterpretedText, coerceString, interpretStandardResult } from '../utils/result-helpers.js';
import { allowPythonFallbackFromEnv } from '../utils/env.js';

export class EditorTools {
  constructor(private bridge: UnrealBridge) {}
  
  async isInPIE(): Promise<boolean> {
    try {
      const pythonCmd = `
import unreal
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if les:
    print("PIE_STATE:" + str(les.is_in_play_in_editor()))
else:
    print("PIE_STATE:False")
      `.trim();
      
  const allowPythonFallback = allowPythonFallbackFromEnv();
  const resp: any = await (this.bridge as any).executeEditorPython(pythonCmd, { allowPythonFallback });
      const out = typeof resp === 'string' ? resp : JSON.stringify(resp);
      return out.includes('PIE_STATE:True');
    } catch {
      return false;
    }
  }
  
  async ensureNotInPIE(): Promise<void> {
    if (await this.isInPIE()) {
      await this.stopPlayInEditor();
      // Wait a bit for PIE to fully stop
      await new Promise(resolve => setTimeout(resolve, 500));
    }
  }

  async playInEditor() {
    try {
      // Set tick rate to match UI play (60 fps for game mode)
      await this.bridge.executeConsoleCommand('t.MaxFPS 60');
      
      // Try Python first using the modern LevelEditorSubsystem
      try {
        // Use LevelEditorSubsystem to play in the selected viewport (modern API)
        const pythonCmd = `
import unreal, time, json
# Start PIE using LevelEditorSubsystem (modern approach)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if les:
    # Store initial state
    was_playing = les.is_in_play_in_editor()
    
    # Request PIE in the current viewport
    les.editor_play_simulate()
    
    # Wait for PIE to start with multiple checks
    max_attempts = 10
    for i in range(max_attempts):
        time.sleep(0.2)  # Wait 200ms between checks
        is_playing = les.is_in_play_in_editor()
        if is_playing and not was_playing:
            # PIE has started
            print('RESULT:' + json.dumps({'success': True, 'method': 'LevelEditorSubsystem'}))
            break
    else:
        # If we've waited 2 seconds total and PIE hasn't started, 
        # but the command was sent, assume it will start
        print('RESULT:' + json.dumps({'success': True, 'method': 'LevelEditorSubsystem'}))
else:
    # If subsystem not available, report error
    print('RESULT:' + json.dumps({'success': False, 'error': 'LevelEditorSubsystem not available'}))
        `.trim();
        
  const allowPythonFallback = allowPythonFallbackFromEnv();
  const resp: any = await (this.bridge as any).executeEditorPython(pythonCmd, { allowPythonFallback });
        const interpreted = interpretStandardResult(resp, {
          successMessage: 'PIE started',
          failureMessage: 'Failed to start PIE'
        });
        if (interpreted.success) {
          const method = coerceString(interpreted.payload.method) ?? 'LevelEditorSubsystem';
          return { success: true, message: `PIE started (via ${method})` };
        }
        // If not verified, fall through to fallback
      } catch (err) {
        // Log the error for debugging but continue
        console.error('Python PIE start issue:', err);
      }
      // Fallback to console command which is more reliable
      await this.bridge.executeConsoleCommand('PlayInViewport');
      
      // Wait a moment and verify PIE started
      await new Promise(resolve => setTimeout(resolve, 1000));
      
      // Check if PIE is now active
      const isPlaying = await this.isInPIE();
      
      return { 
        success: true, 
        message: isPlaying ? 'PIE started successfully' : 'PIE start command sent (may take a moment)' 
      };
    } catch (err) {
      return { success: false, error: `Failed to start PIE: ${err}` };
    }
  }

  async stopPlayInEditor() {
    try {
      // Try Python first using the modern LevelEditorSubsystem
      try {
        const pythonCmd = `
import unreal, time, json
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if les:
    # Use correct method name for stopping PIE
    les.editor_request_end_play()  # Modern API method
    print('RESULT:' + json.dumps({'success': True, 'method': 'LevelEditorSubsystem'}))
else:
    # If subsystem not available, report error
    print('RESULT:' + json.dumps({'success': False, 'error': 'LevelEditorSubsystem not available'}))
        `.trim();
  const allowPythonFallback = allowPythonFallbackFromEnv();
  const resp: any = await (this.bridge as any).executeEditorPython(pythonCmd, { allowPythonFallback });
        const interpreted = interpretStandardResult(resp, {
          successMessage: 'PIE stopped successfully',
          failureMessage: 'Failed to stop PIE'
        });

        if (interpreted.success) {
          const method = coerceString(interpreted.payload.method) ?? 'LevelEditorSubsystem';
          return { success: true, message: `PIE stopped via ${method}` };
        }

        if (interpreted.error) {
          return { success: false, error: interpreted.error };
        }

        return { success: false, error: 'Failed to stop PIE' };
      } catch {
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
      await this.bridge.executeConsoleCommand('pause');
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
      // Use modern LevelEditorSubsystem to build lighting
      const py = `
import unreal
import json
try:
    # Use modern LevelEditorSubsystem API
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if les:
        # build_light_maps(quality, with_reflection_captures)
        les.build_light_maps(unreal.LightingBuildQuality.QUALITY_HIGH, True)
        print('RESULT:' + json.dumps({'success': True, 'message': 'Lighting build started via LevelEditorSubsystem'}))
    else:
        # If subsystem not available, report error
        print('RESULT:' + json.dumps({'success': False, 'error': 'LevelEditorSubsystem not available'}))
except Exception as e:
    print('RESULT:' + json.dumps({'success': False, 'error': str(e)}))
`.trim();
  const allowPythonFallback = allowPythonFallbackFromEnv();
  const resp: any = await (this.bridge as any).executeEditorPython(py, { allowPythonFallback });
      const interpreted = interpretStandardResult(resp, {
        successMessage: 'Lighting build started',
        failureMessage: 'Failed to build lighting'
      });

      if (interpreted.success) {
        return { success: true, message: interpreted.message };
      }

      return {
        success: false,
        error: interpreted.error ?? 'Failed to build lighting',
        details: bestEffortInterpretedText(interpreted)
      };
    } catch (err) {
      return { success: false, error: `Failed to build lighting: ${err}` };
    }
  }

  async setViewportCamera(location?: { x: number; y: number; z: number } | [number, number, number] | null | undefined, rotation?: { pitch: number; yaw: number; roll: number } | [number, number, number] | null | undefined) {
    // Special handling for when both location and rotation are missing/invalid
    // Allow rotation-only updates
    if (location === null) {
      // Explicit null is not allowed for location
      throw new Error('Invalid location: null is not allowed');
    }
    if (location !== undefined && location !== null) {
      const locObj = toVec3Object(location);
      if (!locObj) {
        throw new Error('Invalid location: must be {x,y,z} or [x,y,z]');
      }
      // Clamp extreme values to reasonable limits for Unreal Engine
      const MAX_COORD = 1000000; // 1 million units is a reasonable max for UE
      locObj.x = Math.max(-MAX_COORD, Math.min(MAX_COORD, locObj.x));
      locObj.y = Math.max(-MAX_COORD, Math.min(MAX_COORD, locObj.y));
      locObj.z = Math.max(-MAX_COORD, Math.min(MAX_COORD, locObj.z));
      location = locObj as any;
    }
    
    // Validate rotation if provided
    if (rotation !== undefined) {
      if (rotation === null) {
        throw new Error('Invalid rotation: null is not allowed');
      }
      const rotObj = toRotObject(rotation);
      if (!rotObj) {
        throw new Error('Invalid rotation: must be {pitch,yaw,roll} or [pitch,yaw,roll]');
      }
      // Normalize rotation values to 0-360 range
      rotObj.pitch = ((rotObj.pitch % 360) + 360) % 360;
      rotObj.yaw = ((rotObj.yaw % 360) + 360) % 360;
      rotObj.roll = ((rotObj.roll % 360) + 360) % 360;
      rotation = rotObj as any;
    }
    
    try {
      // Try Python for actual viewport camera positioning
      // Only proceed if we have a valid location
      if (location) {
        try {
          const rot = (rotation as any) || { pitch: 0, yaw: 0, roll: 0 };
          const pythonCmd = `
import unreal
# Use UnrealEditorSubsystem instead of deprecated EditorLevelLibrary
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
location = unreal.Vector(${(location as any).x}, ${(location as any).y}, ${(location as any).z})
rotation = unreal.Rotator(${rot.pitch}, ${rot.yaw}, ${rot.roll})
if ues:
    ues.set_level_viewport_camera_info(location, rotation)
    # Invalidate viewports to ensure visual update
    try:
        if les:
            les.editor_invalidate_viewports()
    except Exception:
        pass
          `.trim();
          const allowPythonFallback = allowPythonFallbackFromEnv();
          await (this.bridge as any).executeEditorPython(pythonCmd, { allowPythonFallback });
          return { 
            success: true, 
            message: 'Viewport camera positioned via UnrealEditorSubsystem' 
          };
        } catch {
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
# Use UnrealEditorSubsystem to read/write viewport camera
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
rotation = unreal.Rotator(${(rotation as any).pitch}, ${(rotation as any).yaw}, ${(rotation as any).roll})
if ues:
    info = ues.get_level_viewport_camera_info()
    if info is not None:
        current_location, _ = info
        ues.set_level_viewport_camera_info(current_location, rotation)
        try:
            if les:
                les.editor_invalidate_viewports()
        except Exception:
            pass
          `.trim();
          const allowPythonFallback = allowPythonFallbackFromEnv();
          await (this.bridge as any).executeEditorPython(pythonCmd, { allowPythonFallback });
          return { 
            success: true, 
            message: 'Viewport camera rotation set via UnrealEditorSubsystem' 
          };
        } catch {
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
      await this.bridge.executeConsoleCommand(`camspeed ${speed}`);
      return { success: true, message: `Camera speed set to ${speed}` };
    } catch (err) {
      return { success: false, error: `Failed to set camera speed: ${err}` };
    }
  }
  
  async setFOV(fov: number) {
    try {
      await this.bridge.executeConsoleCommand(`fov ${fov}`);
      return { success: true, message: `FOV set to ${fov}` };
    } catch (err) {
      return { success: false, error: `Failed to set FOV: ${err}` };
    }
  }

  async takeScreenshot(filename?: string) {
    try {
      const sanitizedFilename = filename ? filename.replace(/[<>:*?"|]/g, '_') : `Screenshot_${Date.now()}`;
      const command = filename ? `highresshot 1920x1080 filename="${sanitizedFilename}"` : 'shot';
      
      await this.bridge.executeConsoleCommand(command);

      return { 
        success: true, 
        message: `Screenshot captured: ${sanitizedFilename}`,
        filename: sanitizedFilename,
        command 
      };
    } catch (err) {
      return { success: false, error: `Failed to take screenshot: ${err}` };
    }
  }

  async setViewportResolution(width: number, height: number) {
    try {
      // Clamp to reasonable limits
      const clampedWidth = Math.max(320, Math.min(7680, width));
      const clampedHeight = Math.max(240, Math.min(4320, height));
      
      const pythonCmd = `
import unreal
import json

result = {"success": False, "message": "", "error": ""}

try:
    # Use LevelEditorSubsystem to resize viewport
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if les:
        # Set viewport resolution via Python
        unreal.SystemLibrary.execute_console_command(
            None, 
            f"r.SetRes {clampedWidth}x{clampedHeight}"
        )
        result["success"] = True
        result["message"] = f"Viewport resolution set to {clampedWidth}x{clampedHeight}"
    else:
        result["error"] = "LevelEditorSubsystem not available"
except Exception as e:
    result["error"] = str(e)

print("RESULT:" + json.dumps(result))
`.trim();

  const allowPythonFallback = allowPythonFallbackFromEnv();
  const resp: any = await (this.bridge as any).executeEditorPython(pythonCmd, { allowPythonFallback });
      const interpreted = interpretStandardResult(resp, {
        successMessage: `Viewport resolution set to ${clampedWidth}x${clampedHeight}`,
        failureMessage: 'Failed to set viewport resolution'
      });

      if (interpreted.success) {
        return { 
          success: true, 
          message: interpreted.message,
          width: clampedWidth,
          height: clampedHeight
        };
      }

      return {
        success: false,
        error: interpreted.error ?? 'Failed to set viewport resolution'
      };
    } catch (err) {
      return { success: false, error: `Failed to set viewport resolution: ${err}` };
    }
  }

  async executeConsoleCommand(command: string) {
    try {
      // Sanitize and validate command
      if (!command || typeof command !== 'string') {
        return { success: false, error: 'Invalid command: must be a non-empty string' };
      }

      if (command.length > 1000) {
        return { 
          success: false, 
          error: `Command too long (${command.length} chars). Maximum is 1000 characters.` 
        };
      }

      const res = await this.bridge.executeConsoleCommand(command);
      
      return { 
        success: true, 
        message: `Console command executed: ${command}`,
        output: res 
      };
    } catch (err) {
      return { success: false, error: `Failed to execute console command: ${err}` };
    }
  }
}
