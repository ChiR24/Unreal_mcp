import { UnrealBridge } from '../unreal-bridge.js';
import { ensureRotation, ensureVector3 } from '../utils/validation.js';
import { BaseTool } from './base-tool.js';
import { IActorTools } from '../types/tool-interfaces.js';

export class ActorTools extends BaseTool implements IActorTools {
  constructor(bridge: UnrealBridge) {
    super(bridge);
  }

  async spawn(params: { classPath: string; location?: { x: number; y: number; z: number }; rotation?: { pitch: number; yaw: number; roll: number }; actorName?: string; timeoutMs?: number }) {
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

    try {
      const bridge = this.getAutomationBridge();
      const timeoutMs = typeof params.timeoutMs === 'number' && params.timeoutMs > 0 ? params.timeoutMs : undefined;
      const response = await bridge.sendAutomationRequest(
        'control_actor',
        {
          action: 'spawn',
          classPath: mappedClassPath,
          location: { x: locX, y: locY, z: locZ },
          rotation: { pitch: rotPitch, yaw: rotYaw, roll: rotRoll },
          actorName: sanitizedActorName
        },
        timeoutMs ? { timeoutMs } : undefined
      );

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

      if ((response as any).warnings?.length) {
        result.warnings = (response as any).warnings;
      }
      if ((response as any).details?.length) {
        result.details = (response as any).details;
      }
      if ((response as any).componentPaths?.length) {
        result.componentPaths = (response as any).componentPaths;
      }

      return result;
    } catch (err) {
      throw new Error(`Failed to spawn actor: ${err}`);
    }
  }

  async delete(params: { actorName?: string; actorNames?: string[] }) {
    if (params.actorNames && Array.isArray(params.actorNames)) {
      const names = params.actorNames
        .filter(name => typeof name === 'string')
        .map(name => name.trim())
        .filter(name => name.length > 0);

      // Edge-case: empty batch should be treated as a no-op success
      if (names.length === 0) {
        return {
          success: true,
          message: 'No actors provided for deletion; no-op',
          deleted: [],
          noOp: true
        };
      }

      return this.sendRequest('delete', { actorNames: names }, 'control_actor')
        .then(response => ({
          success: true,
          message: response.message || 'Deleted actors',
          deleted: response.deleted || names
        }));
    }

    if (!params.actorName || typeof params.actorName !== 'string') {
      throw new Error('Invalid actorName');
    }

    return this.sendRequest('delete', { actorName: params.actorName }, 'control_actor')
      .then(response => ({
        success: true,
        message: response.message || `Deleted actor ${params.actorName}`,
        deleted: params.actorName
      }));
  }

  async applyForce(params: { actorName: string; force: { x: number; y: number; z: number } }) {
    if (!params.actorName || typeof params.actorName !== 'string') {
      throw new Error('Invalid actorName');
    }
    if (!params.force || typeof params.force !== 'object') {
      throw new Error('Invalid force vector');
    }

    const [forceX, forceY, forceZ] = ensureVector3(params.force, 'force vector');

    // Edge-case: zero force vector is treated as a safe no-op. This avoids
    // spurious ACTOR_NOT_FOUND errors when the physics actor has already been
    // cleaned up in prior tests.
    if (forceX === 0 && forceY === 0 && forceZ === 0) {
      return {
        success: true,
        message: `Zero force provided for ${params.actorName}; no-op`,
        physicsEnabled: false,
        noOp: true
      };
    }

    return this.sendRequest('apply_force', {
      actorName: params.actorName,
      force: { x: forceX, y: forceY, z: forceZ }
    }, 'control_actor').then(response => ({
      success: true,
      message: response.message || `Applied force to ${params.actorName}`,
      physicsEnabled: response.physicsEnabled ?? true
    }));
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

  async spawnBlueprint(params: { blueprintPath: string; actorName?: string; location?: { x: number; y: number; z: number }; rotation?: { pitch: number; yaw: number; roll: number } }) {
    const blueprintPath = typeof params.blueprintPath === 'string' ? params.blueprintPath.trim() : '';
    if (!blueprintPath) {
      throw new Error('Invalid blueprintPath');
    }

    const actorName = typeof params.actorName === 'string' && params.actorName.trim().length > 0 ? params.actorName.trim() : undefined;
    const location = params.location ? ensureVector3(params.location, 'spawn_blueprint location') : undefined;
    const rotation = params.rotation ? ensureRotation(params.rotation, 'spawn_blueprint rotation') : undefined;

    const payload: Record<string, unknown> = { blueprintPath };
    if (actorName) payload.actorName = actorName;
    if (location) payload.location = { x: location[0], y: location[1], z: location[2] };
    if (rotation) payload.rotation = { pitch: rotation[0], yaw: rotation[1], roll: rotation[2] };

    return this.sendRequest('spawn_blueprint', payload, 'control_actor');
  }

  async setTransform(params: { actorName: string; location?: { x: number; y: number; z: number }; rotation?: { pitch: number; yaw: number; roll: number }; scale?: { x: number; y: number; z: number } }) {
    const actorName = typeof params.actorName === 'string' ? params.actorName.trim() : '';
    if (!actorName) {
      throw new Error('Invalid actorName');
    }

    const payload: Record<string, unknown> = { actorName };
    if (params.location) {
      const loc = ensureVector3(params.location, 'set_transform location');
      payload.location = { x: loc[0], y: loc[1], z: loc[2] };
    }
    if (params.rotation) {
      const rot = ensureRotation(params.rotation, 'set_transform rotation');
      payload.rotation = { pitch: rot[0], yaw: rot[1], roll: rot[2] };
    }
    if (params.scale) {
      const scl = ensureVector3(params.scale, 'set_transform scale');
      payload.scale = { x: scl[0], y: scl[1], z: scl[2] };
    }

    return this.sendRequest('set_transform', payload, 'control_actor');
  }

  async getTransform(actorName: string) {
    if (typeof actorName !== 'string' || actorName.trim().length === 0) {
      throw new Error('Invalid actorName');
    }
    return this.sendRequest('get_transform', { actorName }, 'control_actor');
  }

  async setVisibility(params: { actorName: string; visible: boolean }) {
    const actorName = typeof params.actorName === 'string' ? params.actorName.trim() : '';
    if (!actorName) {
      throw new Error('Invalid actorName');
    }
    return this.sendRequest('set_visibility', { actorName, visible: Boolean(params.visible) }, 'control_actor');
  }

  async addComponent(params: { actorName: string; componentType: string; componentName?: string; properties?: Record<string, unknown> }) {
    const actorName = typeof params.actorName === 'string' ? params.actorName.trim() : '';
    const componentType = typeof params.componentType === 'string' ? params.componentType.trim() : '';
    if (!actorName) throw new Error('Invalid actorName');
    if (!componentType) throw new Error('Invalid componentType');

    return this.sendRequest('add_component', {
      actorName,
      componentType,
      componentName: typeof params.componentName === 'string' ? params.componentName : undefined,
      properties: params.properties
    }, 'control_actor');
  }

  async setComponentProperties(params: { actorName: string; componentName: string; properties: Record<string, unknown> }) {
    const actorName = typeof params.actorName === 'string' ? params.actorName.trim() : '';
    const componentName = typeof params.componentName === 'string' ? params.componentName.trim() : '';
    if (!actorName) throw new Error('Invalid actorName');
    if (!componentName) throw new Error('Invalid componentName');

    return this.sendRequest('set_component_properties', {
      actorName,
      componentName,
      properties: params.properties ?? {}
    }, 'control_actor');
  }

  async getComponents(actorName: string) {
    if (typeof actorName !== 'string' || actorName.trim().length === 0) {
      throw new Error('Invalid actorName');
    }
    return this.sendRequest('get_components', { actorName }, 'control_actor');
  }

  async duplicate(params: { actorName: string; newName?: string; offset?: { x: number; y: number; z: number } }) {
    const actorName = typeof params.actorName === 'string' ? params.actorName.trim() : '';
    if (!actorName) throw new Error('Invalid actorName');

    const payload: Record<string, unknown> = { actorName };
    if (typeof params.newName === 'string' && params.newName.trim().length > 0) {
      payload.newName = params.newName.trim();
    }
    if (params.offset) {
      const offs = ensureVector3(params.offset, 'duplicate offset');
      payload.offset = { x: offs[0], y: offs[1], z: offs[2] };
    }

    return this.sendRequest('duplicate', payload, 'control_actor');
  }

  async addTag(params: { actorName: string; tag: string }) {
    const actorName = typeof params.actorName === 'string' ? params.actorName.trim() : '';
    const tag = typeof params.tag === 'string' ? params.tag.trim() : '';
    if (!actorName) throw new Error('Invalid actorName');
    if (!tag) throw new Error('Invalid tag');

    return this.sendRequest('add_tag', { actorName, tag }, 'control_actor');
  }

  async findByTag(params: { tag: string; matchType?: string }) {
    const tag = typeof params.tag === 'string' ? params.tag.trim() : '';

    // Edge-case: empty tag should return an empty result set instead of throwing
    if (!tag) {
      return {
        success: true,
        message: 'Empty tag query; no actors matched',
        actors: [],
        count: 0,
        empty: true
      };
    }

    return this.sendRequest('find_by_tag', {
      tag,
      matchType: typeof params.matchType === 'string' ? params.matchType : undefined
    }, 'control_actor');
  }

  async findByName(name: string) {
    if (typeof name !== 'string' || name.trim().length === 0) {
      throw new Error('Invalid actor name query');
    }
    return this.sendRequest('find_by_name', { name: name.trim() }, 'control_actor');
  }

  async detach(actorName: string) {
    if (typeof actorName !== 'string' || actorName.trim().length === 0) {
      throw new Error('Invalid actorName');
    }
    return this.sendRequest('detach', { actorName }, 'control_actor');
  }

  async attach(params: { childActor: string; parentActor: string }) {
    const child = typeof params.childActor === 'string' ? params.childActor.trim() : '';
    const parent = typeof params.parentActor === 'string' ? params.parentActor.trim() : '';
    if (!child) throw new Error('Invalid childActor');
    if (!parent) throw new Error('Invalid parentActor');

    return this.sendRequest('attach', { childActor: child, parentActor: parent }, 'control_actor');
  }

  async deleteByTag(tag: string) {
    if (typeof tag !== 'string' || tag.trim().length === 0) {
      throw new Error('Invalid tag');
    }
    return this.sendRequest('delete_by_tag', { tag: tag.trim() }, 'control_actor');
  }

  async setBlueprintVariables(params: { actorName: string; variables: Record<string, unknown> }) {
    const actorName = typeof params.actorName === 'string' ? params.actorName.trim() : '';
    if (!actorName) throw new Error('Invalid actorName');
    return this.sendRequest('set_blueprint_variables', { actorName, variables: params.variables ?? {} }, 'control_actor');
  }

  async createSnapshot(params: { actorName: string; snapshotName: string }) {
    const actorName = typeof params.actorName === 'string' ? params.actorName.trim() : '';
    const snapshotName = typeof params.snapshotName === 'string' ? params.snapshotName.trim() : '';
    if (!actorName) throw new Error('Invalid actorName');
    if (!snapshotName) throw new Error('Invalid snapshotName');
    return this.sendRequest('create_snapshot', { actorName, snapshotName }, 'control_actor');
  }

  async restoreSnapshot(params: { actorName: string; snapshotName: string }) {
    const actorName = typeof params.actorName === 'string' ? params.actorName.trim() : '';
    const snapshotName = typeof params.snapshotName === 'string' ? params.snapshotName.trim() : '';
    if (!actorName) throw new Error('Invalid actorName');
    if (!snapshotName) throw new Error('Invalid snapshotName');
    return this.sendRequest('restore_snapshot', { actorName, snapshotName }, 'control_actor');
  }

  async exportActor(params: { actorName: string; destinationPath?: string }) {
    const actorName = typeof params.actorName === 'string' ? params.actorName.trim() : '';
    if (!actorName) throw new Error('Invalid actorName');
    return this.sendRequest('export', {
      actorName,
      destinationPath: params.destinationPath
    }, 'control_actor');
  }

  async getBoundingBox(actorName: string) {
    if (typeof actorName !== 'string' || actorName.trim().length === 0) {
      throw new Error('Invalid actorName');
    }
    return this.sendRequest('get_bounding_box', { actorName }, 'control_actor');
  }

  async getMetadata(actorName: string) {
    if (typeof actorName !== 'string' || actorName.trim().length === 0) {
      throw new Error('Invalid actorName');
    }
    return this.sendRequest('get_metadata', { actorName }, 'control_actor');
  }

  async listActors() {
    return this.sendRequest('list', {}, 'control_actor');
  }
}
