import { UnrealBridge } from '../unreal-bridge.js';
import { ensureRotation, ensureVector3 } from '../utils/validation.js';

export class ActorTools {
  constructor(private bridge: UnrealBridge) {}

  async spawn(params: { classPath: string; location?: { x: number; y: number; z: number }; rotation?: { pitch: number; yaw: number; roll: number }; actorName?: string }) {
    if (!params.classPath || typeof params.classPath !== 'string' || params.classPath.trim().length === 0) {
      throw new Error(`Invalid classPath: ${params.classPath}`);
    }

    const className = params.classPath.trim();
    const requestedActorName = typeof params.actorName === 'string' ? params.actorName.trim() : undefined;
    if (params.actorName !== undefined && (!requestedActorName || requestedActorName.length === 0)) {
      throw new Error(`Invalid actorName: ${params.actorName}`);
    }
    const sanitizedActorName = requestedActorName?.replace(/[^A-Za-z0-9_-]/g, '_');
    const lowerName = className.toLowerCase();

    const shapeMapping: Record<string, string> = {
      cube: '/Engine/BasicShapes/Cube',
      sphere: '/Engine/BasicShapes/Sphere',
      cylinder: '/Engine/BasicShapes/Cylinder',
      cone: '/Engine/BasicShapes/Cone',
      plane: '/Engine/BasicShapes/Plane',
      torus: '/Engine/BasicShapes/Torus'
    };

    const mappedClassPath = shapeMapping[lowerName] ?? this.resolveActorClass(className);

    const [locX, locY, locZ] = ensureVector3(
      params.location ?? { x: 0, y: 0, z: 100 },
      'actor location'
    );
    const [rotPitch, rotYaw, rotRoll] = ensureRotation(
      params.rotation ?? { pitch: 0, yaw: 0, roll: 0 },
      'actor rotation'
    );

    // Use automation bridge for actor spawning
    try {
      const response = await (this.bridge as any).automationBridge?.sendAutomationRequest('spawn_actor', {
        classPath: mappedClassPath,
        location: { x: locX, y: locY, z: locZ },
        rotation: { pitch: rotPitch, yaw: rotYaw, roll: rotRoll },
        actorName: sanitizedActorName
      });

      if (!response || !response.success) {
        throw new Error(response?.error || response?.message || 'Failed to spawn actor');
      }

      const result: Record<string, unknown> = {
        success: true,
        message: response.message || `Spawned actor ${className}`,
        actorName: response.actorName,
        actorPath: response.actorPath,
        resolvedClass: mappedClassPath,
        requestedClass: className,
        location: { x: locX, y: locY, z: locZ },
        rotation: { pitch: rotPitch, yaw: rotYaw, roll: rotRoll }
      };

      if (response.warnings?.length) {
        result.warnings = response.warnings;
      }
      if (response.details?.length) {
        result.details = response.details;
      }
      if (response.componentPaths?.length) {
        result.componentPaths = response.componentPaths;
      }

      return result;
    } catch (err) {
      throw new Error(`Failed to spawn actor: ${err}`);
    }
  }

  async delete(params: { actorName: string }) {
    if (!params.actorName || typeof params.actorName !== 'string') {
      throw new Error('Invalid actorName');
    }

    try {
      const response = await (this.bridge as any).automationBridge?.sendAutomationRequest('delete_actor', {
        actorName: params.actorName
      });

      if (!response || !response.success) {
        throw new Error(response?.error || response?.message || 'Failed to delete actor');
      }

      return {
        success: true,
        message: response.message || `Deleted actor ${params.actorName}`,
        deleted: params.actorName
      };
    } catch (err) {
      throw new Error(`Failed to delete actor: ${err}`);
    }
  }

  async applyForce(params: { actorName: string; force: { x: number; y: number; z: number } }) {
    if (!params.actorName || typeof params.actorName !== 'string') {
      throw new Error('Invalid actorName');
    }
    if (!params.force || typeof params.force !== 'object') {
      throw new Error('Invalid force vector');
    }

    const [forceX, forceY, forceZ] = ensureVector3(params.force, 'force vector');

    try {
      const response = await (this.bridge as any).automationBridge?.sendAutomationRequest('apply_force', {
        actorName: params.actorName,
        force: { x: forceX, y: forceY, z: forceZ }
      });

      if (!response || !response.success) {
        throw new Error(response?.error || response?.message || 'Failed to apply force');
      }

      return {
        success: true,
        message: response.message || `Applied force to ${params.actorName}`,
        physicsEnabled: response.physicsEnabled ?? true
      };
    } catch (err) {
      throw new Error(`Failed to apply force: ${err}`);
    }
  }

  private resolveActorClass(classPath: string): string {
    // Map common names to full Unreal class paths
    const classMap: { [key: string]: string } = {
      'PointLight': '/Script/Engine.PointLight',
      'DirectionalLight': '/Script/Engine.DirectionalLight',
      'SpotLight': '/Script/Engine.SpotLight',
      'RectLight': '/Script/Engine.RectLight',
      'SkyLight': '/Script/Engine.SkyLight',
      'StaticMeshActor': '/Script/Engine.StaticMeshActor',
      'PlayerStart': '/Script/Engine.PlayerStart',
      'Camera': '/Script/Engine.CameraActor',
      'CameraActor': '/Script/Engine.CameraActor',
      'Pawn': '/Script/Engine.DefaultPawn',
      'Character': '/Script/Engine.Character',
      'TriggerBox': '/Script/Engine.TriggerBox',
      'TriggerSphere': '/Script/Engine.TriggerSphere',
      'BlockingVolume': '/Script/Engine.BlockingVolume',
      'PostProcessVolume': '/Script/Engine.PostProcessVolume',
      'LightmassImportanceVolume': '/Script/Engine.LightmassImportanceVolume',
      'NavMeshBoundsVolume': '/Script/Engine.NavMeshBoundsVolume',
      'ExponentialHeightFog': '/Script/Engine.ExponentialHeightFog',
      'AtmosphericFog': '/Script/Engine.AtmosphericFog',
      'SphereReflectionCapture': '/Script/Engine.SphereReflectionCapture',
      'BoxReflectionCapture': '/Script/Engine.BoxReflectionCapture',
      // PlaneReflectionCapture is abstract and cannot be spawned
      'DecalActor': '/Script/Engine.DecalActor'
    };
    
    // Check if it's a simple name that needs mapping
    if (classMap[classPath]) {
      return classMap[classPath];
    }
    
    // Check if it already looks like a full path
    if (classPath.startsWith('/Script/') || classPath.startsWith('/Game/')) {
      return classPath;
    }

    if (classPath.startsWith('/Engine/')) {
      return classPath;
    }
    
    // Check for Blueprint paths
    if (classPath.includes('Blueprint') || classPath.includes('BP_')) {
      // Ensure it has the proper prefix
      if (!classPath.startsWith('/Game/')) {
        return '/Game/' + classPath;
      }
      return classPath;
    }
    
    // Default: assume it's an engine class
    return '/Script/Engine.' + classPath;
  }
  
  private getConsoleClassName(classPath: string): string {
    // Normalize class path for console 'summon'
    const input = classPath;

    // Engine classes: reduce '/Script/Engine.ClassName' to 'ClassName'
    if (input.startsWith('/Script/Engine.')) {
      return input.replace('/Script/Engine.', '');
    }

    // If it's already a simple class name (no path) and not a /Game asset, strip optional _C and return
    if (!input.startsWith('/Game/') && !input.includes('/')) {
      if (input.endsWith('_C')) return input.slice(0, -2);
      return input;
    }

    // Blueprint assets under /Game: ensure '/Game/Path/Asset.Asset_C'
    if (input.startsWith('/Game/')) {
      // Remove any existing ".Something" suffix to rebuild normalized class ref
      const pathWithoutSuffix = input.split('.')[0];
      const parts = pathWithoutSuffix.split('/');
      const assetName = parts[parts.length - 1].replace(/_C$/, '');
      const normalized = `${pathWithoutSuffix}.${assetName}_C`;
      return normalized;
    }

    // Fallback: return input unchanged
    return input;
  }
}
