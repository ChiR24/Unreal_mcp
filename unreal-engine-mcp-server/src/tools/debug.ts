// Debug visualization tools for Unreal Engine
import { UnrealBridge } from '../unreal-bridge';

export class DebugVisualizationTools {
  constructor(private bridge: UnrealBridge) {}

  // Execute console command
  private async executeCommand(command: string) {
    return this.bridge.httpCall('/remote/object/call', 'PUT', {
      objectPath: '/Script/Engine.Default__KismetSystemLibrary',
      functionName: 'ExecuteConsoleCommand',
      parameters: {
        Command: command,
        SpecificPlayer: null
      },
      generateTransaction: false
    });
  }

  // Draw debug line
  async drawDebugLine(params: {
    start: [number, number, number];
    end: [number, number, number];
    color?: [number, number, number, number];
    duration?: number;
    thickness?: number;
  }) {
    const color = params.color || [255, 0, 0, 255];
    const duration = params.duration || 5.0;
    const thickness = params.thickness || 1.0;
    
    const command = `DrawDebugLine ${params.start.join(' ')} ${params.end.join(' ')} ${color.join(' ')} ${duration} ${thickness}`;
    return this.executeCommand(command);
  }

  // Draw debug box
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
    const duration = params.duration || 5.0;
    const thickness = params.thickness || 1.0;
    
    const command = `DrawDebugBox ${params.center.join(' ')} ${params.extent.join(' ')} ${color.join(' ')} ${rotation.join(' ')} ${duration} ${thickness}`;
    return this.executeCommand(command);
  }

  // Draw debug sphere
  async drawDebugSphere(params: {
    center: [number, number, number];
    radius: number;
    segments?: number;
    color?: [number, number, number, number];
    duration?: number;
    thickness?: number;
  }) {
    const segments = params.segments || 12;
    const color = params.color || [0, 0, 255, 255];
    const duration = params.duration || 5.0;
    const thickness = params.thickness || 1.0;
    
    const command = `DrawDebugSphere ${params.center.join(' ')} ${params.radius} ${segments} ${color.join(' ')} ${duration} ${thickness}`;
    return this.executeCommand(command);
  }

  // Draw debug capsule
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
    
    const command = `DrawDebugCapsule ${params.center.join(' ')} ${params.halfHeight} ${params.radius} ${rotation.join(' ')} ${color.join(' ')} ${duration}`;
    return this.executeCommand(command);
  }

  // Draw debug cone
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
    const numSides = params.numSides || 12;
    const color = params.color || [255, 0, 255, 255];
    const duration = params.duration || 5.0;
    
    const command = `DrawDebugCone ${params.origin.join(' ')} ${params.direction.join(' ')} ${params.length} ${params.angleWidth} ${params.angleHeight} ${numSides} ${color.join(' ')} ${duration}`;
    return this.executeCommand(command);
  }

  // Draw debug string
  async drawDebugString(params: {
    location: [number, number, number];
    text: string;
    color?: [number, number, number, number];
    duration?: number;
    fontSize?: number;
  }) {
    const color = params.color || [255, 255, 255, 255];
    const duration = params.duration || 5.0;
    const fontSize = params.fontSize || 1.0;
    
    const command = `DrawDebugString ${params.location.join(' ')} "${params.text}" ${color.join(' ')} ${duration} ${fontSize}`;
    return this.executeCommand(command);
  }

  // Draw debug arrow
  async drawDebugArrow(params: {
    start: [number, number, number];
    end: [number, number, number];
    arrowSize?: number;
    color?: [number, number, number, number];
    duration?: number;
    thickness?: number;
  }) {
    const arrowSize = params.arrowSize || 10.0;
    const color = params.color || [0, 255, 255, 255];
    const duration = params.duration || 5.0;
    const thickness = params.thickness || 2.0;
    
    const command = `DrawDebugArrow ${params.start.join(' ')} ${params.end.join(' ')} ${arrowSize} ${color.join(' ')} ${duration} ${thickness}`;
    return this.executeCommand(command);
  }

  // Draw debug point
  async drawDebugPoint(params: {
    location: [number, number, number];
    size?: number;
    color?: [number, number, number, number];
    duration?: number;
  }) {
    const size = params.size || 10.0;
    const color = params.color || [255, 255, 255, 255];
    const duration = params.duration || 5.0;
    
    const command = `DrawDebugPoint ${params.location.join(' ')} ${size} ${color.join(' ')} ${duration}`;
    return this.executeCommand(command);
  }

  // Draw debug coordinate system
  async drawDebugCoordinateSystem(params: {
    location: [number, number, number];
    rotation?: [number, number, number];
    scale?: number;
    duration?: number;
    thickness?: number;
  }) {
    const rotation = params.rotation || [0, 0, 0];
    const scale = params.scale || 100.0;
    const duration = params.duration || 5.0;
    const thickness = params.thickness || 2.0;
    
    const command = `DrawDebugCoordinateSystem ${params.location.join(' ')} ${rotation.join(' ')} ${scale} ${duration} ${thickness}`;
    return this.executeCommand(command);
  }

  // Draw debug frustum
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
    
    const command = `DrawDebugFrustum ${params.origin.join(' ')} ${params.rotation.join(' ')} ${params.fov} ${aspectRatio} ${nearPlane} ${farPlane} ${color.join(' ')} ${duration}`;
    return this.executeCommand(command);
  }

  // Clear debug drawings
  async clearDebugDrawings() {
    return this.executeCommand('FlushPersistentDebugLines');
  }

  // Show collision
  async showCollision(params: {
    enabled: boolean;
    type?: 'Simple' | 'Complex' | 'Both';
  }) {
    const commands = [];
    
    if (params.enabled) {
      const typeCmd = params.type === 'Simple' ? '1' : params.type === 'Complex' ? '2' : '3';
      commands.push(`show Collision ${typeCmd}`);
    } else {
      commands.push('show Collision 0');
    }
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
    return { success: true, message: `Collision visualization ${params.enabled ? 'enabled' : 'disabled'}` };
  }

  // Show bounds
  async showBounds(params: {
    enabled: boolean;
  }) {
    const command = params.enabled ? 'show Bounds' : 'show Bounds 0';
    return this.executeCommand(command);
  }

  // Set view mode with crash protection
  async setViewMode(params: {
    mode: 'Lit' | 'Unlit' | 'Wireframe' | 'DetailLighting' | 'LightingOnly' | 'LightComplexity' | 'ShaderComplexity' | 'LightmapDensity' | 'StationaryLightOverlap' | 'ReflectionOverride' | 'CollisionPawn' | 'CollisionVisibility';
  }) {
    // Known problematic viewmodes that can cause crashes
    const UNSAFE_VIEWMODES = [
      'LightComplexity', 'ShaderComplexity', 'LightmapDensity',
      'StationaryLightOverlap', 'CollisionPawn', 'CollisionVisibility'
    ];
    
    // Warn about potentially unsafe viewmodes
    if (UNSAFE_VIEWMODES.includes(params.mode)) {
      console.warn(`⚠️ Viewmode '${params.mode}' may cause crashes in some UE configurations.`);
      
      // Try to ensure we're not in PIE mode first (safer for viewmode changes)
      try {
        await this.executeCommand('stop');
      } catch (e) {
        // Ignore if not in PIE
      }
      
      // Add a small delay to let the engine stabilize
      await new Promise(resolve => setTimeout(resolve, 100));
    }
    
    try {
      const command = `viewmode ${params.mode}`;
      const result = await this.executeCommand(command);
      
      // For unsafe modes, immediately switch back to Lit if there's an issue
      if (UNSAFE_VIEWMODES.includes(params.mode)) {
        // Set a safety timeout to revert to Lit mode
        setTimeout(async () => {
          try {
            // Check if we're still responsive
            await this.executeCommand('stat unit');
          } catch (e) {
            // If unresponsive, try to recover
            console.error('Viewmode may have caused an issue, attempting recovery...');
            await this.executeCommand('viewmode Lit');
          }
        }, 2000);
      }
      
      return { ...result, warning: UNSAFE_VIEWMODES.includes(params.mode) ? `Viewmode '${params.mode}' applied. This mode may be unstable.` : undefined };
    } catch (error) {
      // Fallback to Lit mode on error
      await this.executeCommand('viewmode Lit');
      throw new Error(`Failed to set viewmode '${params.mode}': ${error}. Reverted to Lit mode.`);
    }
  }

  // Show debug info
  async showDebugInfo(params: {
    category: 'AI' | 'Animation' | 'Audio' | 'Collision' | 'Camera' | 'Game' | 'Hitboxes' | 'Input' | 'Net' | 'Physics' | 'Slate' | 'Streaming' | 'Particles' | 'Navigation';
    enabled: boolean;
  }) {
    const command = `showdebug ${params.enabled ? params.category : 'None'}`;
    return this.executeCommand(command);
  }

  // Show actor names
  async showActorNames(params: {
    enabled: boolean;
  }) {
    const command = params.enabled ? 'show ActorNames' : 'show ActorNames 0';
    return this.executeCommand(command);
  }

  // Draw debug path
  async drawDebugPath(params: {
    points: Array<[number, number, number]>;
    color?: [number, number, number, number];
    duration?: number;
    thickness?: number;
  }) {
    const color = params.color || [255, 128, 0, 255];
    const duration = params.duration || 5.0;
    const thickness = params.thickness || 2.0;
    
    const commands = [];
    for (let i = 0; i < params.points.length - 1; i++) {
      const start = params.points[i];
      const end = params.points[i + 1];
      commands.push(`DrawDebugLine ${start.join(' ')} ${end.join(' ')} ${color.join(' ')} ${duration} ${thickness}`);
    }
    
    for (const cmd of commands) {
      await this.executeCommand(cmd);
    }
    
    return { success: true, message: `Debug path drawn with ${params.points.length} points` };
  }

  // Show navigation mesh
  async showNavigationMesh(params: {
    enabled: boolean;
  }) {
    const command = params.enabled ? 'show Navigation' : 'show Navigation 0';
    return this.executeCommand(command);
  }

  // Enable on-screen messages
  async enableOnScreenMessages(params: {
    enabled: boolean;
    key?: number;
    message?: string;
    duration?: number;
    color?: [number, number, number, number];
  }) {
    if (params.enabled && params.message) {
      const key = params.key || -1;
      const duration = params.duration || 5.0;
      const color = params.color || [255, 255, 255, 255];
      const command = `ke * DisplayDebugMessage ${key} "${params.message}" ${duration} ${color.join(' ')}`;
      return this.executeCommand(command);
    } else {
      return this.executeCommand('DisableAllScreenMessages');
    }
  }

  // Show skeletal mesh bones
  async showSkeletalMeshBones(params: {
    actorName: string;
    enabled: boolean;
  }) {
    const command = params.enabled 
      ? `ShowDebugSkelMesh ${params.actorName}` 
      : `HideDebugSkelMesh ${params.actorName}`;
    return this.executeCommand(command);
  }
}
