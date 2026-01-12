import { BaseTool } from './base-tool.js';
import { IEditorTools, StandardActionResponse } from '../types/tool-interfaces.js';
import { toVec3Object, toRotObject } from '../utils/normalize.js';
import { DEFAULT_SCREENSHOT_RESOLUTION } from '../constants.js';
import { EditorResponse } from '../types/automation-responses.js';
import { wasmIntegration } from '../wasm/index.js';

export class EditorTools extends BaseTool implements IEditorTools {
  
  async isInPIE(): Promise<boolean> {
    try {
      const response = await this.sendAutomationRequest<EditorResponse>(
        'check_pie_state',
        {},
        { timeoutMs: 5000 }
      );

      if (response && response.success !== false) {
        return response.isInPIE === true || (response.result as Record<string, unknown> | undefined)?.isInPIE === true;
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

  async playInEditor(timeoutMs: number = 30000): Promise<StandardActionResponse> {
    try {
      try {
        const response = await this.sendAutomationRequest<EditorResponse>(
          'control_editor',
          { action: 'play' },
          { timeoutMs }
        );
        if (response && response.success === true) {
          return { success: true, message: response.message || 'PIE started' };
        }
        return { success: false, error: response?.error || response?.message || 'Failed to start PIE' };
      } catch (err: unknown) {
        // If it's a timeout, return error instead of falling back
        const errMsg = err instanceof Error ? err.message : String(err);
        if (errMsg && /time.*out/i.test(errMsg)) {
          return { success: false, error: `Timeout waiting for PIE to start: ${errMsg}` };
        }

        // Fallback to console commands if automation bridge is unavailable or fails (non-timeout)
        await this.bridge.executeConsoleCommand('t.MaxFPS 60');
        await this.bridge.executeConsoleCommand('PlayInViewport');
        return { success: true, message: 'PIE start command sent' };
      }
    } catch (err: unknown) {
      return { success: false, error: `Failed to start PIE: ${err}` };
    }
  }

  async stopPlayInEditor(): Promise<StandardActionResponse> {
    try {
      try {
        const response = await this.sendAutomationRequest<EditorResponse>(
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

        return {
          success: false,
          error: response.error || response.message || 'Failed to stop PIE'
        };
      } catch (_pluginErr) {
        // Fallback to console command if plugin fails
        await this.bridge.executeConsoleCommand('stop');
        return { success: true, message: 'PIE stopped via console command' };
      }
    } catch (err: unknown) {
      return { success: false, error: `Failed to stop PIE: ${err}` };
    }
  }

  async pausePlayInEditor(): Promise<StandardActionResponse> {
    try {
      // Pause/Resume PIE
      await this.bridge.executeConsoleCommand('pause');
      return { success: true, message: 'PIE paused/resumed' };
    } catch (err: unknown) {
      return { success: false, error: `Failed to pause PIE: ${err}` };
    }
  }

  // Alias for consistency with naming convention
  async pauseInEditor(): Promise<StandardActionResponse> {
    return this.pausePlayInEditor();
  }

  async buildLighting(): Promise<StandardActionResponse> {
    try {
      // Use console command to build lighting
      await this.bridge.executeConsoleCommand('BuildLighting');
      return { success: true, message: 'Lighting build started' };
    } catch (err: unknown) {
      return { success: false, error: `Failed to build lighting: ${err}` };
    }
  }

  async setViewportCamera(location?: { x: number; y: number; z: number } | [number, number, number] | null | undefined, rotation?: { pitch: number; yaw: number; roll: number } | [number, number, number] | null | undefined): Promise<StandardActionResponse> {
    // Special handling for when both location and rotation are missing/invalid
    // Allow rotation-only updates
    if (location === null) {
      // Explicit null is not allowed for location
      throw new Error('Invalid location: null is not allowed');
    }
    if (location !== undefined) {
      const locObj = toVec3Object(location);
      if (!locObj) {
        throw new Error('Invalid location: must be {x,y,z} or [x,y,z]');
      }
      // Clamp extreme values to reasonable limits for Unreal Engine
      const MAX_COORD = 1000000; // 1 million units is a reasonable max for UE
      locObj.x = Math.max(-MAX_COORD, Math.min(MAX_COORD, locObj.x));
      locObj.y = Math.max(-MAX_COORD, Math.min(MAX_COORD, locObj.y));
      locObj.z = Math.max(-MAX_COORD, Math.min(MAX_COORD, locObj.z));
      location = locObj as { x: number; y: number; z: number };
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
      rotation = rotObj as { pitch: number; yaw: number; roll: number };
    }

    // Use native control_editor.set_camera when available
    try {
      // Use WASM composeTransform for camera transform calculation
      const locRecord = location as unknown as Record<string, number> | undefined;
      const rotRecord = rotation as unknown as Record<string, number> | undefined;
      const locArray: [number, number, number] = location
        ? [Number(locRecord?.x ?? 0), Number(locRecord?.y ?? 0), Number(locRecord?.z ?? 0)]
        : [0, 0, 0];
      const rotArray: [number, number, number] = rotation
        ? [Number(rotRecord?.pitch ?? 0), Number(rotRecord?.yaw ?? 0), Number(rotRecord?.roll ?? 0)]
        : [0, 0, 0];
      // Compose transform to validate and process camera positioning via WASM
      wasmIntegration.composeTransform(locArray, rotArray, [1, 1, 1]);

      const resp = await this.sendAutomationRequest<EditorResponse>('control_editor', {
        action: 'set_camera',
        location: location,
        rotation: rotation
      }, { timeoutMs: 10000 });
      if (resp && resp.success === true) {
        return { success: true, message: resp.message || 'Camera set', location, rotation };
      }
      return { success: false, error: resp?.error || resp?.message || 'Failed to set camera' };
    } catch (err: unknown) {
      return { success: false, error: `Camera control failed: ${err}` };
    }
  }

  async setCameraSpeed(speed: number): Promise<StandardActionResponse> {
    try {
      await this.bridge.executeConsoleCommand(`camspeed ${speed}`);
      return { success: true, message: `Camera speed set to ${speed}` };
    } catch (err: unknown) {
      return { success: false, error: `Failed to set camera speed: ${err}` };
    }
  }

  async setFOV(fov: number): Promise<StandardActionResponse> {
    try {
      await this.bridge.executeConsoleCommand(`fov ${fov}`);
      return { success: true, message: `FOV set to ${fov}` };
    } catch (err: unknown) {
      return { success: false, error: `Failed to set FOV: ${err}` };
    }
  }

  async takeScreenshot(filename?: string, resolution?: string): Promise<StandardActionResponse> {
    try {
      if (resolution && !/^\d+x\d+$/.test(resolution)) {
        return { success: false, error: 'Invalid resolution format. Use WxH (e.g. 1920x1080)' };
      }

      const sanitizedFilename = filename ? filename.replace(/[<>:*?"|]/g, '_') : `Screenshot_${Date.now()}`;
      const resString = resolution || DEFAULT_SCREENSHOT_RESOLUTION;
      const command = filename ? `highresshot ${resString} filename="${sanitizedFilename}"` : 'shot';

      await this.bridge.executeConsoleCommand(command);

      return {
        success: true,
        message: `Screenshot captured: ${sanitizedFilename}`,
        filename: sanitizedFilename,
        command
      };
    } catch (err: unknown) {
      return { success: false, error: `Failed to take screenshot: ${err}` };
    }
  }

  async resumePlayInEditor(): Promise<StandardActionResponse> {
    try {
      // Use console command to toggle pause (resumes if paused)
      await this.bridge.executeConsoleCommand('pause');
      return {
        success: true,
        message: 'PIE resume toggled via pause command'
      };
    } catch (err: unknown) {
      return { success: false, error: `Failed to resume PIE: ${err}` };
    }
  }

  async stepPIEFrame(steps: number = 1): Promise<StandardActionResponse> {
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
    } catch (err: unknown) {
      return { success: false, error: `Failed to step PIE: ${err}` };
    }
  }

  async startRecording(options?: { filename?: string; frameRate?: number; durationSeconds?: number; metadata?: Record<string, unknown> }): Promise<StandardActionResponse> {
    try {
      const resp = await this.sendAutomationRequest<EditorResponse>('control_editor', {
        action: 'start_recording',
        filename: options?.filename,
        frameRate: options?.frameRate,
        durationSeconds: options?.durationSeconds,
        metadata: options?.metadata
      });
      return {
        success: resp.success ?? false,
        message: resp.message || 'Recording started',
        recording: (resp.result as Record<string, unknown> | undefined)?.recording
      };
    } catch (err) {
      return { success: false, error: String(err) };
    }
  }

  async stopRecording(): Promise<StandardActionResponse> {
    try {
      const resp = await this.sendAutomationRequest<EditorResponse>('control_editor', {
        action: 'stop_recording'
      });
      return {
        success: resp.success ?? false,
        message: resp.message || 'Recording stopped',
        recording: (resp.result as Record<string, unknown> | undefined)?.recording
      };
    } catch (err) {
      return { success: false, error: String(err) };
    }
  }

  async createCameraBookmark(name: string): Promise<StandardActionResponse> {
    try {
      const resp = await this.sendAutomationRequest<EditorResponse>('control_editor', {
        action: 'create_bookmark',
        bookmarkName: name
      });
      return {
        success: resp.success ?? false,
        message: resp.message || 'Bookmark created',
        bookmark: (resp.result as Record<string, unknown> | undefined)?.bookmark
      };
    } catch (err) {
      return { success: false, error: String(err) };
    }
  }

  async jumpToCameraBookmark(name: string): Promise<StandardActionResponse> {
    try {
      const resp = await this.sendAutomationRequest<EditorResponse>('control_editor', {
        action: 'jump_to_bookmark',
        bookmarkName: name
      });
      return {
        success: resp.success ?? false,
        message: resp.message || 'Jumped to bookmark'
      };
    } catch (err) {
      return { success: false, error: String(err) };
    }
  }

  async setEditorPreferences(category: string | undefined, preferences: Record<string, unknown>): Promise<StandardActionResponse> {
    try {
      const resp = await this.sendAutomationRequest<EditorResponse>('control_editor', {
        action: 'set_preferences',
        category: category,
        preferences: preferences
      });
      return {
        success: resp.success ?? false,
        message: resp.message || 'Preferences updated'
      };
    } catch (err) {
      return { success: false, error: String(err) };
    }
  }

  async setViewportResolution(width: number, height: number): Promise<StandardActionResponse> {
    try {
      const resp = await this.sendAutomationRequest<EditorResponse>('control_editor', {
        action: 'set_viewport_resolution',
        width,
        height
      });
      return {
        success: resp.success ?? false,
        message: resp.message || `Viewport resolution set to ${width}x${height}`
      };
    } catch (err) {
      return { success: false, error: String(err) };
    }
  }

  async setViewportRealtime(enabled: boolean): Promise<StandardActionResponse> {
    try {
      const resp = await this.sendAutomationRequest<EditorResponse>('control_editor', {
        action: 'set_viewport_realtime',
        enabled
      });
      return {
        success: resp.success ?? false,
        message: resp.message || `Viewport realtime set to ${enabled}`
      };
    } catch (err) {
      return { success: false, error: String(err) };
    }
  }

  async executeConsoleCommand(command: string): Promise<StandardActionResponse> {
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
    } catch (err: unknown) {
      return { success: false, error: `Failed to execute console command: ${err}` };
    }
  }
}
