import { UnrealBridge } from '../unreal-bridge.js';
import { toVec3Object, toRotObject } from '../utils/normalize.js';

export class EditorTools {
  private cameraBookmarks = new Map<string, { location: [number, number, number]; rotation: [number, number, number]; savedAt: number }>();
  private editorPreferences = new Map<string, Record<string, unknown>>();
  private activeRecording?: { name?: string; options?: Record<string, unknown>; startedAt: number };

  constructor(private bridge: UnrealBridge) {}
  
  async isInPIE(): Promise<boolean> {
    try {
      // Use Automation Bridge to check PIE state
      const automationBridge = (this.bridge as any).automationBridge;
      if (automationBridge && typeof automationBridge.sendAutomationRequest === 'function') {
        const response = await automationBridge.sendAutomationRequest(
          'check_pie_state',
          {},
          { timeoutMs: 5000 }
        );
        
        if (response && response.success !== false) {
          return response.isInPIE === true || response.result?.isInPIE === true;
        }
      }
      
      return false;
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
      // Prefer native control_editor action when available
      const automationBridge = (this.bridge as any).automationBridge;
      if (automationBridge && typeof automationBridge.sendAutomationRequest === 'function') {
        const response = await automationBridge.sendAutomationRequest(
          'control_editor',
          { action: 'play' },
          { timeoutMs: 30000 }
        );
        if (response && response.success === true) {
          return { success: true, message: response.message || 'PIE started' };
        }
        return { success: false, error: response?.error || response?.message || 'Failed to start PIE' };
      }

      // Fallback to console commands if automation bridge is unavailable
      await this.bridge.executeConsoleCommand('t.MaxFPS 60');
      await this.bridge.executeConsoleCommand('PlayInViewport');
      return { success: true, message: 'PIE start command sent' };
    } catch (err) {
      return { success: false, error: `Failed to start PIE: ${err}` };
    }
  }

  async stopPlayInEditor() {
    try {
      // Use the native C++ plugin's control_editor action instead of Python
      const automationBridge = (this.bridge as any).automationBridge;
      if (automationBridge && typeof automationBridge.sendAutomationRequest === 'function') {
        try {
          const response = await automationBridge.sendAutomationRequest(
            'control_editor',
            { action: 'stop' },
            { timeoutMs: 30000 }
          );
          
          if (response.success !== false) {
            return { 
              success: true, 
              message: response.message || 'PIE stopped successfully' 
            };
          }
          
          // Plugin returned error
          return { 
            success: false, 
            error: response.error || response.message || 'Failed to stop PIE' 
          };
        } catch (_pluginErr) {
          // Fallback to console command if plugin fails
          await this.bridge.executeConsoleCommand('stop');
          return { success: true, message: 'PIE stopped via console command' };
        }
      }
      
      // No automation bridge available - use console command
      await this.bridge.executeConsoleCommand('stop');
      return { success: true, message: 'PIE stop command sent' };
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
      // Use console command to build lighting
      await this.bridge.executeConsoleCommand('BuildLighting');
      return { success: true, message: 'Lighting build started' };
    } catch (err) {
      return { success: false, error: `Failed to build lighting: ${err}` };
    }
  }

  private async getViewportCameraInfo(): Promise<{
    success: boolean;
    location?: [number, number, number];
    rotation?: [number, number, number];
    error?: string;
    message?: string;
  }> {
    try {
      const automationBridge = (this.bridge as any).automationBridge;
      const resp = await automationBridge.sendAutomationRequest(
        'control_editor',
        { action: 'get_camera' },
        { timeoutMs: 3000 }
      );
      const result = resp?.result ?? resp;
      const loc = result?.location ?? result?.camera?.location;
      const rot = result?.rotation ?? result?.camera?.rotation;
      const locArr: [number, number, number] | undefined = Array.isArray(loc) && loc.length === 3 ? [Number(loc[0])||0, Number(loc[1])||0, Number(loc[2])||0] : undefined;
      const rotArr: [number, number, number] | undefined = Array.isArray(rot) && rot.length === 3 ? [Number(rot[0])||0, Number(rot[1])||0, Number(rot[2])||0] : undefined;
      if (resp && resp.success !== false && locArr && rotArr) {
        return { success: true, location: locArr, rotation: rotArr };
      }
      return { success: false, error: 'Failed to get camera information' };
    } catch (err) {
      return { success: false, error: `Camera query failed: ${err}` };
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
    
    // Use native control_editor.set_camera when available
    try {
      const automationBridge = (this.bridge as any).automationBridge;
      const resp = await automationBridge.sendAutomationRequest('control_editor', {
        action: 'set_camera',
        location: location as any,
        rotation: rotation as any
      }, { timeoutMs: 10000 });
      if (resp && resp.success === true) {
        return { success: true, message: resp.message || 'Camera set' };
      }
      return { success: false, error: resp?.error || resp?.message || 'Failed to set camera' };
    } catch (err) {
      return { success: false, error: `Camera control failed: ${err}` };
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

  async resumePlayInEditor() {
    try {
      // Use console command to toggle pause (resumes if paused)
      await this.bridge.executeConsoleCommand('pause');
      return {
        success: true,
        message: 'PIE resume toggled via pause command'
      };
    } catch (err) {
      return { success: false, error: `Failed to resume PIE: ${err}` };
    }
  }

  async stepPIEFrame(steps: number = 1) {
    const clampedSteps = Number.isFinite(steps) ? Math.max(1, Math.floor(steps)) : 1;
    try {
      // Use console command to step frames
      for (let index = 0; index < clampedSteps; index += 1) {
        await this.bridge.executeConsoleCommand('Step=1');
      }
      return {
        success: true,
        message: `Advanced PIE by ${clampedSteps} frame(s)`,
        steps: clampedSteps
      };
    } catch (err) {
      return { success: false, error: `Failed to step PIE: ${err}` };
    }
  }

  async startRecording(options?: { filename?: string; frameRate?: number; durationSeconds?: number; metadata?: Record<string, unknown> }) {
    const startedAt = Date.now();
    this.activeRecording = {
      name: typeof options?.filename === 'string' ? options.filename.trim() : undefined,
      options: options ? { ...options } : undefined,
      startedAt
    };

    return {
      success: true as const,
      message: 'Recording session started',
      recording: {
        name: this.activeRecording.name,
        startedAt,
        options: this.activeRecording.options
      }
    };
  }

  async stopRecording() {
    if (!this.activeRecording) {
      return {
        success: true as const,
        message: 'No active recording session to stop'
      };
    }

    const stoppedRecording = this.activeRecording;
    this.activeRecording = undefined;

    return {
      success: true as const,
      message: 'Recording session stopped',
      recording: stoppedRecording
    };
  }

  async createCameraBookmark(name: string) {
    const trimmedName = name.trim();
    if (!trimmedName) {
      return { success: false as const, error: 'bookmarkName is required' };
    }

    const cameraInfo = await this.getViewportCameraInfo();
    if (!cameraInfo.success || !cameraInfo.location || !cameraInfo.rotation) {
      return {
        success: false as const,
        error: cameraInfo.error || 'Failed to capture viewport camera'
      };
    }

    this.cameraBookmarks.set(trimmedName, {
      location: cameraInfo.location,
      rotation: cameraInfo.rotation,
      savedAt: Date.now()
    });

    return {
      success: true as const,
      message: `Bookmark '${trimmedName}' saved`,
      bookmark: {
        name: trimmedName,
        location: cameraInfo.location,
        rotation: cameraInfo.rotation
      }
    };
  }

  async jumpToCameraBookmark(name: string) {
    const trimmedName = name.trim();
    if (!trimmedName) {
      return { success: false as const, error: 'bookmarkName is required' };
    }

    const bookmark = this.cameraBookmarks.get(trimmedName);
    if (!bookmark) {
      return {
        success: false as const,
        error: `Bookmark '${trimmedName}' not found`
      };
    }

    await this.setViewportCamera(
      { x: bookmark.location[0], y: bookmark.location[1], z: bookmark.location[2] },
      { pitch: bookmark.rotation[0], yaw: bookmark.rotation[1], roll: bookmark.rotation[2] }
    );

    return {
      success: true as const,
      message: `Jumped to bookmark '${trimmedName}'`
    };
  }

  async setEditorPreferences(category: string | undefined, preferences: Record<string, unknown>) {
    const resolvedCategory = typeof category === 'string' && category.trim().length > 0 ? category.trim() : 'General';
    const existing = this.editorPreferences.get(resolvedCategory) ?? {};
    this.editorPreferences.set(resolvedCategory, { ...existing, ...preferences });

    return {
      success: true as const,
      message: `Preferences stored for ${resolvedCategory}`,
      preferences: this.editorPreferences.get(resolvedCategory)
    };
  }

  async setViewportResolution(width: number, height: number) {
    try {
      // Clamp to reasonable limits
      const clampedWidth = Math.max(320, Math.min(7680, width));
      const clampedHeight = Math.max(240, Math.min(4320, height));
      
      // Use console command directly instead of Python
      const command = `r.SetRes ${clampedWidth}x${clampedHeight}`;
      await this.bridge.executeConsoleCommand(command);
      
      return {
        success: true,
        message: `Viewport resolution set to ${clampedWidth}x${clampedHeight}`,
        width: clampedWidth,
        height: clampedHeight
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
