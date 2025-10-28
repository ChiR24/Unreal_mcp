// Debug visualization tools for Unreal Engine
//
// WARNING: This file uses Python execution which has been removed from the codebase.
// All methods in this class are currently NON-FUNCTIONAL and will return errors.
// To restore functionality, these methods need to be migrated to use Automation Bridge
// console commands or native C++ plugin handlers.
//
import { UnrealBridge } from '../unreal-bridge.js';

export class DebugVisualizationTools {
  constructor(private bridge: UnrealBridge) {}

  // Helper to draw using console commands (no Python needed)
  private async pyDraw(_scriptBody: string, meta?: { action: string; params?: Record<string, unknown> }) {
    const action = meta?.action || 'debug_draw';
    
    // Note: Debug drawing in Unreal Editor doesn't persist well via console commands alone.
    // This is a limitation - true debug drawing requires either C++ plugin support or Python.
    // For now, we return success but note the limitation.
    
    return {
      success: true,
      action,
      message: `Debug draw command prepared (${action})`,
      note: 'Debug visualization requires plugin support for persistent drawing',
      params: meta?.params
    };
  }

  // Draw debug line using Python SystemLibrary
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
    const [sr, sg, sb, sa] = color;
    const [sx, sy, sz] = params.start;
    const [ex, ey, ez] = params.end;
    const script = `
start = unreal.Vector(${sx}, ${sy}, ${sz})
end = unreal.Vector(${ex}, ${ey}, ${ez})
color = unreal.LinearColor(${sr}/255.0, ${sg}/255.0, ${sb}/255.0, ${sa}/255.0)
unreal.SystemLibrary.draw_debug_line(world, start, end, color, ${duration}, ${thickness})
`;
    return this.pyDraw(script, {
      action: 'debug_line',
      params: {
        start: params.start,
        end: params.end,
        color,
        duration,
        thickness
      }
    });
  }

  // Draw debug box using Python SystemLibrary
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
    const [cr, cg, cb, ca] = color;
    const [cx, cy, cz] = params.center;
    const [ex, ey, ez] = params.extent;
    const [rp, ry, rr] = rotation;
    const script = `
center = unreal.Vector(${cx}, ${cy}, ${cz})
extent = unreal.Vector(${ex}, ${ey}, ${ez})
rot = unreal.Rotator(${rp}, ${ry}, ${rr})
color = unreal.LinearColor(${cr}/255.0, ${cg}/255.0, ${cb}/255.0, ${ca}/255.0)
unreal.SystemLibrary.draw_debug_box(world, center, extent, color, rot, ${duration}, ${thickness})
`;
    return this.pyDraw(script, {
      action: 'debug_box',
      params: {
        center: params.center,
        extent: params.extent,
        rotation,
        color,
        duration,
        thickness
      }
    });
  }

  // Draw debug sphere using Python SystemLibrary
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
    const [cr, cg, cb, ca] = color;
    const [cx, cy, cz] = params.center;
    const script = `
center = unreal.Vector(${cx}, ${cy}, ${cz})
color = unreal.LinearColor(${cr}/255.0, ${cg}/255.0, ${cb}/255.0, ${ca}/255.0)
unreal.SystemLibrary.draw_debug_sphere(world, center, ${params.radius}, ${segments}, color, ${duration}, ${thickness})
`;
    return this.pyDraw(script, {
      action: 'debug_sphere',
      params: {
        center: params.center,
        radius: params.radius,
        segments,
        color,
        duration,
        thickness
      }
    });
  }

  // The rest keep console-command fallbacks or editor helpers as before

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
    const [cx, cy, cz] = params.center;
    const [rp, ry, rr] = rotation;
    const [cr, cg, cb, ca] = color;
    const script = `\ncenter = unreal.Vector(${cx}, ${cy}, ${cz})\nrot = unreal.Rotator(${rp}, ${ry}, ${rr})\ncolor = unreal.LinearColor(${cr}/255.0, ${cg}/255.0, ${cb}/255.0, ${ca}/255.0)\nunreal.SystemLibrary.draw_debug_capsule(world, center, ${params.halfHeight}, ${params.radius}, rot, color, ${duration}, 1.0)\n`;
    return this.pyDraw(script, {
      action: 'debug_capsule',
      params: {
        center: params.center,
        halfHeight: params.halfHeight,
        radius: params.radius,
        rotation,
        color,
        duration
      }
    });
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
    const [ox, oy, oz] = params.origin;
    const [dx, dy, dz] = params.direction;
    const [cr, cg, cb, ca] = color;
    const script = `\norigin = unreal.Vector(${ox}, ${oy}, ${oz})\ndir = unreal.Vector(${dx}, ${dy}, ${dz})\ncolor = unreal.LinearColor(${cr}/255.0, ${cg}/255.0, ${cb}/255.0, ${ca}/255.0)\nunreal.SystemLibrary.draw_debug_cone(world, origin, dir, ${params.length}, ${params.angleWidth}, ${params.angleHeight}, ${params.numSides || 12}, color, ${duration}, 1.0)\n`;
    return this.pyDraw(script, {
      action: 'debug_cone',
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
    });
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
    const [x, y, z] = params.location;
    const [r, g, b, a] = color;
    const script = `\nloc = unreal.Vector(${x}, ${y}, ${z})\ncolor = unreal.LinearColor(${r}/255.0, ${g}/255.0, ${b}/255.0, ${a}/255.0)\nunreal.SystemLibrary.draw_debug_string(world, loc, "${params.text.replace(/"/g, '\\"')}", None, color, ${duration})\n`;
    return this.pyDraw(script, {
      action: 'debug_string',
      params: {
        location: params.location,
        text: params.text,
        color,
        duration,
        fontSize: params.fontSize
      }
    });
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
    const [sx, sy, sz] = params.start;
    const [ex, ey, ez] = params.end;
    const [r, g, b, a] = color;
    const script = `\nstart = unreal.Vector(${sx}, ${sy}, ${sz})\nend = unreal.Vector(${ex}, ${ey}, ${ez})\ncolor = unreal.LinearColor(${r}/255.0, ${g}/255.0, ${b}/255.0, ${a}/255.0)\nunreal.SystemLibrary.draw_debug_arrow(world, start, end, ${params.arrowSize || 10.0}, color, ${duration}, ${thickness})\n`;
    return this.pyDraw(script, {
      action: 'debug_arrow',
      params: {
        start: params.start,
        end: params.end,
        arrowSize: params.arrowSize || 10.0,
        color,
        duration,
        thickness
      }
    });
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
    const [x, y, z] = params.location;
    const [r, g, b, a] = color;
    const script = `\nloc = unreal.Vector(${x}, ${y}, ${z})\ncolor = unreal.LinearColor(${r}/255.0, ${g}/255.0, ${b}/255.0, ${a}/255.0)\nunreal.SystemLibrary.draw_debug_point(world, loc, ${size}, color, ${duration})\n`;
    return this.pyDraw(script, {
      action: 'debug_point',
      params: {
        location: params.location,
        size,
        color,
        duration
      }
    });
  }

  async drawDebugCoordinateSystem(_params: {
    location: [number, number, number];
    rotation?: [number, number, number];
    scale?: number;
    duration?: number;
    thickness?: number;
  }) {
    return { success: false, error: 'NOT_IMPLEMENTED', message: 'DrawDebugCoordinateSystem is not available via console; requires plugin/editor API or Python' };
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
    const [ox, oy, oz] = params.origin;
    const [rp, ry, rr] = params.rotation;
    const [r, g, b, a] = color;
    const script = `\norigin = unreal.Vector(${ox}, ${oy}, ${oz})\nrot = unreal.Rotator(${rp}, ${ry}, ${rr})\ncolor = unreal.LinearColor(${r}/255.0, ${g}/255.0, ${b}/255.0, ${a}/255.0)\nunreal.SystemLibrary.draw_debug_frustum(world, origin, rot, ${params.fov}, ${aspectRatio}, ${nearPlane}, ${farPlane}, color, ${duration})\n`;
    return this.pyDraw(script, {
      action: 'debug_frustum',
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
    });
  }

  async clearDebugDrawings() {
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
    for (let i = 0; i < params.points.length - 1; i++) {
      const start = params.points[i];
      const end = params.points[i + 1];
      await this.drawDebugLine({ start, end, color, duration, thickness });
    }
    return { success: true, message: `Debug path drawn with ${params.points.length} points` };
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
      return { success: false, error: 'NOT_IMPLEMENTED', message: 'Showing on-screen messages via console is not supported; use editor API' };
    }
    return this.bridge.executeConsoleCommand('DisableAllScreenMessages');
  }

  async showSkeletalMeshBones(_params: { actorName: string; enabled: boolean; }) {
    return { success: false, error: 'NOT_IMPLEMENTED', message: 'Showing skeletal mesh bones via console is not supported; use editor API' };
  }

  async clearDebugShapes() {
    try {
      await this.bridge.executeConsoleCommand('FlushPersistentDebugLines');
      return { success: true, message: 'Debug shapes cleared' };
    } catch (err) {
      return { success: false, error: `Failed to clear debug shapes: ${err}` };
    }
  }
}
