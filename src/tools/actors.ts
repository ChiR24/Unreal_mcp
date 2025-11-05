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

  async spawnBlueprint(params: { blueprintPath: string; actorName?: string; location?: { x: number; y: number; z: number }; rotation?: { pitch: number; yaw: number; roll: number } }) {
    const blueprintPath = typeof params.blueprintPath === 'string' ? params.blueprintPath.trim() : '';
    if (!blueprintPath) {
      throw new Error('Invalid blueprintPath');
    }

    const actorName = typeof params.actorName === 'string' && params.actorName.trim().length > 0 ? params.actorName.trim() : undefined;
    const location = params.location ? ensureVector3(params.location, 'spawn_blueprint location') : undefined;
    const rotation = params.rotation ? ensureRotation(params.rotation, 'spawn_blueprint rotation') : undefined;

    const automation = (this.bridge as any).automationBridge;
    if (!automation || typeof automation.sendAutomationRequest !== 'function') {
      throw new Error('Automation bridge not available for spawn_blueprint');
    }

    const payload: Record<string, unknown> = { blueprintPath };
    if (actorName) payload.actorName = actorName;
    if (location) payload.location = { x: location[0], y: location[1], z: location[2] };
    if (rotation) payload.rotation = { pitch: rotation[0], yaw: rotation[1], roll: rotation[2] };

    const resp = await automation.sendAutomationRequest('control_actor', { action: 'spawn_blueprint', ...payload });
    if (!resp || resp.success === false) {
      throw new Error(resp?.error || resp?.message || 'Failed to spawn blueprint actor');
    }
    return resp.result ?? resp;
  }

  async setTransform(params: { actorName: string; location?: { x: number; y: number; z: number }; rotation?: { pitch: number; yaw: number; roll: number }; scale?: { x: number; y: number; z: number } }) {
    const actorName = typeof params.actorName === 'string' ? params.actorName.trim() : '';
    if (!actorName) {
      throw new Error('Invalid actorName');
    }

    const payload: Record<string, unknown> = { action: 'set_transform', actorName };
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

    const automation = (this.bridge as any).automationBridge;
    const resp = await automation?.sendAutomationRequest('control_actor', payload);
    if (!resp || resp.success === false) {
      throw new Error(resp?.error || resp?.message || 'Failed to set transform');
    }
    return resp.result ?? resp;
  }

  async getTransform(actorName: string) {
    if (typeof actorName !== 'string' || actorName.trim().length === 0) {
      throw new Error('Invalid actorName');
    }
    const automation = (this.bridge as any).automationBridge;
    const resp = await automation?.sendAutomationRequest('control_actor', { action: 'get_transform', actorName });
    if (!resp || resp.success === false) {
      throw new Error(resp?.error || resp?.message || 'Failed to get transform');
    }
    return resp.result ?? resp;
  }

  async setVisibility(params: { actorName: string; visible: boolean }) {
    const actorName = typeof params.actorName === 'string' ? params.actorName.trim() : '';
    if (!actorName) {
      throw new Error('Invalid actorName');
    }
    const automation = (this.bridge as any).automationBridge;
    const resp = await automation?.sendAutomationRequest('control_actor', { action: 'set_visibility', actorName, visible: Boolean(params.visible) });
    if (!resp || resp.success === false) {
      throw new Error(resp?.error || resp?.message || 'Failed to set visibility');
    }
    return resp.result ?? resp;
  }

  async addComponent(params: { actorName: string; componentType: string; componentName?: string; properties?: Record<string, unknown> }) {
    const actorName = typeof params.actorName === 'string' ? params.actorName.trim() : '';
    const componentType = typeof params.componentType === 'string' ? params.componentType.trim() : '';
    if (!actorName) throw new Error('Invalid actorName');
    if (!componentType) throw new Error('Invalid componentType');

    const automation = (this.bridge as any).automationBridge;
    const resp = await automation?.sendAutomationRequest('control_actor', {
      action: 'add_component',
      actorName,
      componentType,
      componentName: typeof params.componentName === 'string' ? params.componentName : undefined,
      properties: params.properties
    });
    if (!resp || resp.success === false) {
      throw new Error(resp?.error || resp?.message || 'Failed to add component');
    }
    return resp.result ?? resp;
  }

  async setComponentProperties(params: { actorName: string; componentName: string; properties: Record<string, unknown> }) {
    const actorName = typeof params.actorName === 'string' ? params.actorName.trim() : '';
    const componentName = typeof params.componentName === 'string' ? params.componentName.trim() : '';
    if (!actorName) throw new Error('Invalid actorName');
    if (!componentName) throw new Error('Invalid componentName');

    const automation = (this.bridge as any).automationBridge;
    const resp = await automation?.sendAutomationRequest('control_actor', {
      action: 'set_component_properties',
      actorName,
      componentName,
      properties: params.properties ?? {}
    });
    if (!resp || resp.success === false) {
      throw new Error(resp?.error || resp?.message || 'Failed to set component properties');
    }
    return resp.result ?? resp;
  }

  async getComponents(actorName: string) {
    if (typeof actorName !== 'string' || actorName.trim().length === 0) {
      throw new Error('Invalid actorName');
    }
    const automation = (this.bridge as any).automationBridge;
    const resp = await automation?.sendAutomationRequest('control_actor', { action: 'get_components', actorName });
    if (!resp || resp.success === false) {
      throw new Error(resp?.error || resp?.message || 'Failed to get components');
    }
    return resp.result ?? resp;
  }

  async duplicate(params: { actorName: string; newName?: string; offset?: { x: number; y: number; z: number } }) {
    const actorName = typeof params.actorName === 'string' ? params.actorName.trim() : '';
    if (!actorName) throw new Error('Invalid actorName');

    const payload: Record<string, unknown> = { action: 'duplicate', actorName };
    if (typeof params.newName === 'string' && params.newName.trim().length > 0) {
      payload.newName = params.newName.trim();
    }
    if (params.offset) {
      const offs = ensureVector3(params.offset, 'duplicate offset');
      payload.offset = { x: offs[0], y: offs[1], z: offs[2] };
    }

    const automation = (this.bridge as any).automationBridge;
    const resp = await automation?.sendAutomationRequest('control_actor', payload);
    if (!resp || resp.success === false) {
      throw new Error(resp?.error || resp?.message || 'Failed to duplicate actor');
    }
    return resp.result ?? resp;
  }

  async addTag(params: { actorName: string; tag: string }) {
    const actorName = typeof params.actorName === 'string' ? params.actorName.trim() : '';
    const tag = typeof params.tag === 'string' ? params.tag.trim() : '';
    if (!actorName) throw new Error('Invalid actorName');
    if (!tag) throw new Error('Invalid tag');

    const automation = (this.bridge as any).automationBridge;
    const resp = await automation?.sendAutomationRequest('control_actor', { action: 'add_tag', actorName, tag });
    if (!resp || resp.success === false) {
      throw new Error(resp?.error || resp?.message || 'Failed to add tag');
    }
    return resp.result ?? resp;
  }

  async findByTag(params: { tag: string; matchType?: string }) {
    const tag = typeof params.tag === 'string' ? params.tag.trim() : '';
    if (!tag) throw new Error('Invalid tag');

    const automation = (this.bridge as any).automationBridge;
    const resp = await automation?.sendAutomationRequest('control_actor', {
      action: 'find_by_tag',
      tag,
      matchType: typeof params.matchType === 'string' ? params.matchType : undefined
    });
    if (!resp || resp.success === false) {
      throw new Error(resp?.error || resp?.message || 'Failed to find actors by tag');
    }
    return resp.result ?? resp;
  }

  async findByName(name: string) {
    if (typeof name !== 'string' || name.trim().length === 0) {
      throw new Error('Invalid actor name query');
    }
    const automation = (this.bridge as any).automationBridge;
    const resp = await automation?.sendAutomationRequest('control_actor', { action: 'find_by_name', name: name.trim() });
    if (!resp || resp.success === false) {
      throw new Error(resp?.error || resp?.message || 'Failed to find actors by name');
    }
    return resp.result ?? resp;
  }

  async detach(actorName: string) {
    if (typeof actorName !== 'string' || actorName.trim().length === 0) {
      throw new Error('Invalid actorName');
    }
    const automation = (this.bridge as any).automationBridge;
    const resp = await automation?.sendAutomationRequest('control_actor', { action: 'detach', actorName });
    if (!resp || resp.success === false) {
      throw new Error(resp?.error || resp?.message || 'Failed to detach actor');
    }
    return resp.result ?? resp;
  }

  async attach(params: { childActor: string; parentActor: string }) {
    const child = typeof params.childActor === 'string' ? params.childActor.trim() : '';
    const parent = typeof params.parentActor === 'string' ? params.parentActor.trim() : '';
    if (!child) throw new Error('Invalid childActor');
    if (!parent) throw new Error('Invalid parentActor');

    const automation = (this.bridge as any).automationBridge;
    const resp = await automation?.sendAutomationRequest('control_actor', { action: 'attach', childActor: child, parentActor: parent });
    if (!resp || resp.success === false) {
      throw new Error(resp?.error || resp?.message || 'Failed to attach actors');
    }
    return resp.result ?? resp;
  }

  async deleteByTag(tag: string) {
    if (typeof tag !== 'string' || tag.trim().length === 0) {
      throw new Error('Invalid tag');
    }
    const automation = (this.bridge as any).automationBridge;
    const resp = await automation?.sendAutomationRequest('control_actor', { action: 'delete_by_tag', tag: tag.trim() });
    if (!resp || resp.success === false) {
      throw new Error(resp?.error || resp?.message || 'Failed to delete actors by tag');
    }
    return resp.result ?? resp;
  }

  async setBlueprintVariables(params: { actorName: string; variables: Record<string, unknown> }) {
    const actorName = typeof params.actorName === 'string' ? params.actorName.trim() : '';
    if (!actorName) throw new Error('Invalid actorName');
    const automation = (this.bridge as any).automationBridge;
    const resp = await automation?.sendAutomationRequest('control_actor', { action: 'set_blueprint_variables', actorName, variables: params.variables ?? {} });
    if (!resp || resp.success === false) {
      throw new Error(resp?.error || resp?.message || 'Failed to set blueprint variables');
    }
    return resp.result ?? resp;
  }

  async createSnapshot(params: { actorName: string; snapshotName: string }) {
    const actorName = typeof params.actorName === 'string' ? params.actorName.trim() : '';
    const snapshotName = typeof params.snapshotName === 'string' ? params.snapshotName.trim() : '';
    if (!actorName) throw new Error('Invalid actorName');
    if (!snapshotName) throw new Error('Invalid snapshotName');
    const automation = (this.bridge as any).automationBridge;
    const resp = await automation?.sendAutomationRequest('control_actor', { action: 'create_snapshot', actorName, snapshotName });
    if (!resp || resp.success === false) {
      throw new Error(resp?.error || resp?.message || 'Failed to create snapshot');
    }
    return resp.result ?? resp;
  }

  async listActors() {
    const automation = (this.bridge as any).automationBridge;
    const resp = await automation?.sendAutomationRequest('control_actor', { action: 'list' });
    if (!resp || resp.success === false) {
      throw new Error(resp?.error || resp?.message || 'Failed to list actors');
    }
    return resp.result ?? resp;
  }
}
