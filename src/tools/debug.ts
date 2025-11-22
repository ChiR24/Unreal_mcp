// Debug visualization tools for Unreal Engine
// Uses Automation Bridge and console commands for all operations

import { UnrealBridge } from '../unreal-bridge.js';
import type { AutomationBridge } from '../automation-bridge.js';

export class DebugVisualizationTools {
  private bridge: UnrealBridge;
  private automationBridge?: AutomationBridge;

  constructor(bridge: UnrealBridge) {
    this.bridge = bridge;
  }

  setAutomationBridge(automationBridge: AutomationBridge) {
    this.automationBridge = automationBridge;
  }

  // Helper to use Automation Bridge for debug operations
  private async useAutomationBridge(action: string, params: any) {
    if (!this.automationBridge) {
      return { success: false, error: 'AUTOMATION_BRIDGE_NOT_AVAILABLE', message: 'Automation Bridge not available for debug operations' };
    }

    try {
      const response = await this.automationBridge.sendAutomationRequest('create_effect', {
        action: 'debug_shape',
        shapeType: action,
        ...params
      });
      return response;
    } catch (error) {
      return { success: false, error: 'AUTOMATION_BRIDGE_ERROR', message: String(error) };
    }
  }

  // Draw debug line using Automation Bridge
  async drawDebugLine(params: {
    start: [number, number, number];
    end: [number, number, number];
    color?: [number, number, number, number];
    duration?: number;
    thickness?: number;
  }) {
    const color = params.color || [255, 0, 0, 255];
    const duration = params.duration ?? 5.0;
    const thickness = params.thickness ?? 1.0;

    // Try Automation Bridge first
    const result = await this.useAutomationBridge('line', {
      start: params.start,
      end: params.end,
      color,
      duration,
      thickness
    });

    if (result.success) {
      return result;
    }

    return {
      success: false,
      error: result.error || 'AUTOMATION_BRIDGE_REQUIRED',
      message: result.message || 'Drawing debug lines requires Automation Bridge debug_shape handler',
      params: {
        start: params.start,
        end: params.end,
        color,
        duration,
        thickness
      }
    };
  }

  // Draw debug box using Automation Bridge
  async drawDebugBox(params: {
    center: [number, number, number];
    extent: [number, number, number];
    color?: [number, number, number, number];
    rotation?: [number, number, number];
    duration?: number;
    thickness?: number;
  }) {
    const color = params.color || [0, 255, 0, 255];
    const rotation = params.rotation || [0, 0, 0];
    const duration = params.duration ?? 5.0;
    const thickness = params.thickness ?? 1.0;

    const result = await this.useAutomationBridge('box', {
      center: params.center,
      extent: params.extent,
      rotation,
      color,
      duration,
      thickness
    });

    if (result.success) {
      return result;
    }

    return {
      success: false,
      error: result.error || 'AUTOMATION_BRIDGE_REQUIRED',
      message: result.message || 'Drawing debug boxes requires Automation Bridge debug_shape handler',
      params: {
        center: params.center,
        extent: params.extent,
        rotation,
        color,
        duration,
        thickness
      }
    };
  }

  // Draw debug sphere using Automation Bridge
  async drawDebugSphere(params: {
    center: [number, number, number];
    radius: number;
    segments?: number;
    color?: [number, number, number, number];
    duration?: number;
    thickness?: number;
  }) {
    const segments = params.segments ?? 12;
    const color = params.color || [0, 0, 255, 255];
    const duration = params.duration ?? 5.0;
    const thickness = params.thickness ?? 1.0;

    const result = await this.useAutomationBridge('sphere', {
      center: params.center,
      radius: params.radius,
      segments,
      color,
      duration,
      thickness
    });

    if (result.success) {
      return result;
    }

    return {
      success: false,
      error: result.error || 'AUTOMATION_BRIDGE_REQUIRED',
      message: result.message || 'Drawing debug spheres requires Automation Bridge debug_shape handler',
      params: {
        center: params.center,
        radius: params.radius,
        segments,
        color,
        duration,
        thickness
      }
    };
  }

  async drawDebugCapsule(params: {
    center: [number, number, number];
    halfHeight: number;
    radius: number;
    rotation?: [number, number, number];
    color?: [number, number, number, number];
    duration?: number;
  }) {
    const rotation = params.rotation || [0, 0, 0];
    const color = params.color || [255, 255, 0, 255];
    const duration = params.duration || 5.0;

    const result = await this.useAutomationBridge('capsule', {
      center: params.center,
      halfHeight: params.halfHeight,
      radius: params.radius,
      rotation,
      color,
      duration
    });

    if (result.success) {
      return result;
    }

    return {
      success: false,
      error: result.error || 'AUTOMATION_BRIDGE_REQUIRED',
      message: result.message || 'Drawing debug capsules requires Automation Bridge debug_shape handler',
      params: {
        center: params.center,
        halfHeight: params.halfHeight,
        radius: params.radius,
        rotation,
        color,
        duration
      }
    };
  }

  async drawDebugCone(params: {
    origin: [number, number, number];
    direction: [number, number, number];
    length: number;
    angleWidth: number;
    angleHeight: number;
    numSides?: number;
    color?: [number, number, number, number];
    duration?: number;
  }) {
    const color = params.color || [255, 0, 255, 255];
    const duration = params.duration || 5.0;

    const result = await this.useAutomationBridge('cone', {
      origin: params.origin,
      direction: params.direction,
      length: params.length,
      angleWidth: params.angleWidth,
      angleHeight: params.angleHeight,
      numSides: params.numSides || 12,
      color,
      duration
    });

    if (result.success) {
      return result;
    }

    return {
      success: false,
      error: result.error || 'AUTOMATION_BRIDGE_REQUIRED',
      message: result.message || 'Drawing debug cones requires Automation Bridge debug_shape handler',
      params: {
        origin: params.origin,
        direction: params.direction,
        length: params.length,
        angleWidth: params.angleWidth,
        angleHeight: params.angleHeight,
        numSides: params.numSides || 12,
        color,
        duration
      }
    };
  }

  async drawDebugString(params: {
    location: [number, number, number];
    text: string;
    color?: [number, number, number, number];
    duration?: number;
    fontSize?: number;
  }) {
    const color = params.color || [255, 255, 255, 255];
    const duration = params.duration || 5.0;

    const result = await this.useAutomationBridge('string', {
      location: params.location,
      text: params.text,
      color,
      duration,
      fontSize: params.fontSize
    });

    if (result.success) {
      return result;
    }

    return {
      success: false,
      error: result.error || 'AUTOMATION_BRIDGE_REQUIRED',
      message: result.message || 'Drawing debug strings requires Automation Bridge debug_shape handler',
      params: {
        location: params.location,
        text: params.text,
        color,
        duration,
        fontSize: params.fontSize
      }
    };
  }

  async drawDebugArrow(params: {
    start: [number, number, number];
    end: [number, number, number];
    arrowSize?: number;
    color?: [number, number, number, number];
    duration?: number;
    thickness?: number;
  }) {
    const color = params.color || [0, 255, 255, 255];
    const duration = params.duration || 5.0;
    const thickness = params.thickness || 2.0;

    const result = await this.useAutomationBridge('arrow', {
      start: params.start,
      end: params.end,
      arrowSize: params.arrowSize || 10.0,
      color,
      duration,
      thickness
    });

    if (result.success) {
      return result;
    }

    return {
      success: false,
      error: result.error || 'AUTOMATION_BRIDGE_REQUIRED',
      message: result.message || 'Drawing debug arrows requires Automation Bridge debug_shape handler',
      params: {
        start: params.start,
        end: params.end,
        arrowSize: params.arrowSize || 10.0,
        color,
        duration,
        thickness
      }
    };
  }

  async drawDebugPoint(params: {
    location: [number, number, number];
    size?: number;
    color?: [number, number, number, number];
    duration?: number;
  }) {
    const size = params.size || 10.0;
    const color = params.color || [255, 255, 255, 255];
    const duration = params.duration || 5.0;

    const result = await this.useAutomationBridge('point', {
      location: params.location,
      size,
      color,
      duration
    });

    if (result.success) {
      return result;
    }

    return {
      success: false,
      error: result.error || 'AUTOMATION_BRIDGE_REQUIRED',
      message: result.message || 'Drawing debug points requires Automation Bridge debug_shape handler',
      params: {
        location: params.location,
        size,
        color,
        duration
      }
    };
  }

  async drawDebugCoordinateSystem(params: {
    location: [number, number, number];
    rotation?: [number, number, number];
    scale?: number;
    duration?: number;
    thickness?: number;
  }) {
    const result = await this.useAutomationBridge('coordinate_system', {
      location: params.location,
      rotation: params.rotation || [0, 0, 0],
      scale: params.scale || 1.0,
      duration: params.duration || 5.0,
      thickness: params.thickness || 1.0
    });

    return result;
  }

  async drawDebugFrustum(params: {
    origin: [number, number, number];
    rotation: [number, number, number];
    fov: number;
    aspectRatio?: number;
    nearPlane?: number;
    farPlane?: number;
    color?: [number, number, number, number];
    duration?: number;
  }) {
    const aspectRatio = params.aspectRatio || 1.77;
    const nearPlane = params.nearPlane || 10.0;
    const farPlane = params.farPlane || 1000.0;
    const color = params.color || [128, 128, 255, 255];
    const duration = params.duration || 5.0;

    const result = await this.useAutomationBridge('frustum', {
      origin: params.origin,
      rotation: params.rotation,
      fov: params.fov,
      aspectRatio,
      nearPlane,
      farPlane,
      color,
      duration
    });

    if (result.success) {
      return result;
    }

    return {
      success: false,
      error: result.error || 'AUTOMATION_BRIDGE_REQUIRED',
      message: result.message || 'Drawing debug frustums requires Automation Bridge debug_shape handler',
      params: {
        origin: params.origin,
        rotation: params.rotation,
        fov: params.fov,
        aspectRatio,
        nearPlane,
        farPlane,
        color,
        duration
      }
    };
  }

  async clearDebugDrawings() {
    if (this.automationBridge) {
      const response = await this.automationBridge.sendAutomationRequest('clear_debug_shapes', {});
      if (response.success) {
        return response;
      }
    }

    return this.bridge.executeConsoleCommand('FlushPersistentDebugLines');
  }

  async showCollision(params: {
    enabled: boolean;
    type?: 'Simple' | 'Complex' | 'Both';
  }) {
    const commands: string[] = [];
    if (params.enabled) {
      const typeCmd = params.type === 'Simple' ? '1' : params.type === 'Complex' ? '2' : '3';
      commands.push(`show Collision ${typeCmd}`);
    } else {
      commands.push('show Collision 0');
    }
    await this.bridge.executeConsoleCommands(commands);
    return { success: true, message: `Collision visualization ${params.enabled ? 'enabled' : 'disabled'}` };
  }

  async showBounds(params: { enabled: boolean; }) {
    const command = params.enabled ? 'show Bounds' : 'show Bounds 0';
    return this.bridge.executeConsoleCommand(command);
  }

  async setViewMode(params: {
    mode: 'Lit' | 'Unlit' | 'Wireframe' | 'DetailLighting' | 'LightingOnly' | 'LightComplexity' | 'ShaderComplexity' | 'LightmapDensity' | 'StationaryLightOverlap' | 'ReflectionOverride' | 'CollisionPawn' | 'CollisionVisibility';
  }) {
    // Map non-viewmode requests to appropriate show flags for safety
    if (params.mode === 'CollisionPawn' || params.mode === 'CollisionVisibility') {
      // Use collision visualization instead of viewmode (UE doesn't have these as view modes)
      await this.showCollision({ enabled: true, type: 'Both' });
      return { success: true, message: 'Collision visualization enabled (use show flags, not viewmode)' } as any;
    }

    const VALID_VIEWMODES = new Set([
      'Lit', 'Unlit', 'Wireframe', 'DetailLighting', 'LightingOnly', 'LightComplexity', 'ShaderComplexity', 'LightmapDensity', 'StationaryLightOverlap', 'ReflectionOverride'
    ]);

    if (!VALID_VIEWMODES.has(params.mode)) {
      // Fallback to Lit if unknown
      await this.bridge.executeConsoleCommand('viewmode Lit');
      return { success: false, warning: `Unknown or unsupported viewmode '${params.mode}'. Reverted to Lit.` } as any;
    }

    const UNSAFE_VIEWMODES = [
      'LightComplexity', 'ShaderComplexity', 'LightmapDensity', 'StationaryLightOverlap'
    ];
    if (UNSAFE_VIEWMODES.includes(params.mode)) {
      console.error(`⚠️ Viewmode '${params.mode}' may be unstable in some UE configurations.`);
      try { await this.bridge.executeConsoleCommand('stop'); } catch {}
      await new Promise(resolve => setTimeout(resolve, 100));
    }

    try {
      const command = `viewmode ${params.mode}`;
      const result = await this.bridge.executeConsoleCommand(command);
      if (UNSAFE_VIEWMODES.includes(params.mode)) {
        setTimeout(async () => {
          try { await this.bridge.executeConsoleCommand('stat unit'); }
          catch { await this.bridge.executeConsoleCommand('viewmode Lit'); }
        }, 2000);
      }
      return { ...result, warning: UNSAFE_VIEWMODES.includes(params.mode) ? `Viewmode '${params.mode}' applied. This mode may be unstable.` : undefined };
    } catch (error) {
      await this.bridge.executeConsoleCommand('viewmode Lit');
      throw new Error(`Failed to set viewmode '${params.mode}': ${error}. Reverted to Lit mode.`);
    }
  }

  async showDebugInfo(params: {
    category: 'AI' | 'Animation' | 'Audio' | 'Collision' | 'Camera' | 'Game' | 'Hitboxes' | 'Input' | 'Net' | 'Physics' | 'Slate' | 'Streaming' | 'Particles' | 'Navigation';
    enabled: boolean;
  }) {
    const command = `showdebug ${params.enabled ? params.category : 'None'}`;
    return this.bridge.executeConsoleCommand(command);
  }

  async showActorNames(params: { enabled: boolean; }) {
    const command = params.enabled ? 'show ActorNames' : 'show ActorNames 0';
    return this.bridge.executeConsoleCommand(command);
  }

  async drawDebugPath(params: {
    points: Array<[number, number, number]>;
    color?: [number, number, number, number];
    duration?: number;
    thickness?: number;
  }) {
    const color = params.color || [255, 128, 0, 255];
    const duration = params.duration || 5.0;
    const thickness = params.thickness || 2.0;

    // Try Automation Bridge for path drawing
    const result = await this.useAutomationBridge('path', {
      points: params.points,
      color,
      duration,
      thickness
    });

    if (result.success) {
      return result;
    }

    // If Automation Bridge is unavailable or fails, do not claim success
    return {
      success: false,
      error: result.error || 'AUTOMATION_BRIDGE_REQUIRED',
      message: result.message || 'Drawing debug paths requires Automation Bridge debug_shape handler',
      params: {
        points: params.points,
        color,
        duration,
        thickness
      }
    };
  }

  async showNavigationMesh(params: { enabled: boolean; }) {
    const command = params.enabled ? 'show Navigation' : 'show Navigation 0';
    return this.bridge.executeConsoleCommand(command);
  }

  async enableOnScreenMessages(params: {
    enabled: boolean;
    key?: number;
    message?: string;
    duration?: number;
    color?: [number, number, number, number];
  }) {
    if (params.enabled && params.message) {
      // Use Automation Bridge for on-screen messages
      const result = await this.useAutomationBridge('message', {
        message: params.message,
        duration: params.duration || 5.0,
        color: params.color || [255, 255, 255, 255]
      });

      if (result.success) {
        return result;
      }

      return {
        success: false,
        error: 'AUTOMATION_BRIDGE_REQUIRED',
        message: 'Showing on-screen messages requires Automation Bridge'
      };
    }

    const command = params.enabled ? 'EnableAllScreenMessages' : 'DisableAllScreenMessages';
    return this.bridge.executeConsoleCommand(command);
  }

  async showSkeletalMeshBones(params: { actorName: string; enabled: boolean; }) {
    // Use Automation Bridge for skeletal mesh visualization
    const result = await this.useAutomationBridge('skeletal_meshes', {
      actorName: params.actorName,
      enabled: params.enabled
    });

    if (result.success) {
      return result;
    }

    return {
      success: false,
      error: 'AUTOMATION_BRIDGE_REQUIRED',
      message: 'Showing skeletal mesh bones requires Automation Bridge'
    };
  }

  async clearDebugShapes() {
    if (this.automationBridge) {
      const response = await this.automationBridge.sendAutomationRequest('clear_debug_shapes', {});
      if (response.success) {
        return response;
      }
    }

    try {
      await this.bridge.executeConsoleCommand('FlushPersistentDebugLines');
      return { success: true, message: 'Debug shapes cleared' };
    } catch (err) {
      return { success: false, error: `Failed to clear debug shapes: ${err}` };
    }
  }
}
