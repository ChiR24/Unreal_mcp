import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs, AnimationArgs, ComponentInfo, AutomationResponse } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';

/** Response from getComponents */
interface ComponentsResponse {
  success?: boolean;
  components?: ComponentInfo[];
  [key: string]: unknown;
}

/** Extended component info with skeletal mesh specific properties */
interface SkeletalMeshComponentInfo extends ComponentInfo {
  type?: string;
  className?: string;
  skeletalMesh?: string;
  path?: string;
}

/** Result payload structure for animation responses */
interface ResultPayload {
  error?: string;
  message?: string;
  [key: string]: unknown;
}

export async function handleAnimationTools(action: string, args: HandlerArgs, tools: ITools): Promise<Record<string, unknown>> {
  const argsTyped = args as AnimationArgs;
  const animAction = String(action || '').toLowerCase();

  // Route specific actions to their dedicated handlers
  if (animAction === 'create_animation_blueprint' || animAction === 'create_anim_blueprint' || animAction === 'create_animation_bp') {
    const name = argsTyped.name ?? argsTyped.blueprintName;
    let skeletonPath = argsTyped.skeletonPath ?? argsTyped.targetSkeleton;
    let meshPath = argsTyped.meshPath;
    const savePath = argsTyped.savePath ?? argsTyped.path ?? '/Game/Animations';

    // Auto-resolve skeleton/mesh from actorName if not provided
    if (!skeletonPath && argsTyped.actorName) {
      try {
        const compsRes = await executeAutomationRequest(tools, 'control_actor', { action: 'get_components', actorName: argsTyped.actorName }) as ComponentsResponse;
        if (compsRes && Array.isArray(compsRes.components)) {
          const meshComp = compsRes.components.find((c): c is SkeletalMeshComponentInfo => 
            (c as SkeletalMeshComponentInfo).type === 'SkeletalMeshComponent' || 
            (c as SkeletalMeshComponentInfo).className === 'SkeletalMeshComponent'
          );
          // Write back resolved path to the outgoing payload
          if (meshComp) {
            if (!meshPath && meshComp.path) meshPath = meshComp.path;
            if (!meshPath && meshComp.skeletalMesh) meshPath = meshComp.skeletalMesh;
          }
        }
      } catch (_e) { }
    }

    const payload = {
      ...args,
      name,
      skeletonPath,
      meshPath,
      savePath
    };

    const res = await executeAutomationRequest(tools, 'create_animation_blueprint', payload, 'Automation bridge not available for animation blueprint creation');
    return res as Record<string, unknown>;
  }

  if (animAction === 'play_anim_montage' || animAction === 'play_montage') {
    const resp = await executeAutomationRequest(
      tools,
      'play_anim_montage',
      args,
      'Automation bridge not available for montage playback'
    ) as AutomationResponse;
    const result = (resp?.result ?? resp ?? {}) as ResultPayload;

    const errorCode = typeof result.error === 'string' ? result.error.toUpperCase() : '';
    const message = typeof result.message === 'string' ? result.message : '';
    const msgLower = message.toLowerCase();

    // Check for actor not found - return proper failure state
    if (msgLower.includes('actor not found') || msgLower.includes('no animation played') || errorCode === 'ACTOR_NOT_FOUND') {
      return cleanObject({
        success: false,
        error: 'ACTOR_NOT_FOUND',
        message: message || 'Actor not found; no animation played',
        actorName: argsTyped.actorName
      });
    }

    if (
      errorCode === 'INVALID_ARGUMENT' &&
      msgLower.includes('actorname required') &&
      typeof argsTyped.playRate === 'number' &&
      argsTyped.playRate === 0
    ) {
      return cleanObject({
        success: true,
        noOp: true,
        message: 'Montage playback skipped: playRate 0 with missing actorName treated as no-op.'
      });
    }

    return cleanObject(resp);
  }

  if (animAction === 'setup_ragdoll' || animAction === 'activate_ragdoll') {
    // Auto-resolve meshPath from actorName if missing
    const mutableArgs = { ...argsTyped } as AnimationArgs & Record<string, unknown>;
    
    if (argsTyped.actorName && !argsTyped.meshPath && !argsTyped.skeletonPath) {
      try {
        const compsRes = await executeAutomationRequest(tools, 'control_actor', { action: 'get_components', actorName: argsTyped.actorName }) as ComponentsResponse;
        if (compsRes && Array.isArray(compsRes.components)) {
          const meshComp = compsRes.components.find((c): c is SkeletalMeshComponentInfo => 
            (c as SkeletalMeshComponentInfo).type === 'SkeletalMeshComponent' || 
            (c as SkeletalMeshComponentInfo).className === 'SkeletalMeshComponent'
          );
          if (meshComp && meshComp.path) {
            mutableArgs.meshPath = meshComp.path;
          }
        }
      } catch (_e) {
        // Ignore component lookup errors, fallback to C++ handling
      }
    }

    const resp = await executeAutomationRequest(tools, 'setup_ragdoll', mutableArgs, 'Automation bridge not available for ragdoll setup') as AutomationResponse;
    const result = (resp?.result ?? resp ?? {}) as ResultPayload;

    const message = typeof result.message === 'string' ? result.message : '';
    const msgLower = message.toLowerCase();

    // Check for actor not found - return proper failure state
    if (msgLower.includes('actor not found') || msgLower.includes('no ragdoll applied')) {
      return cleanObject({
        success: false,
        error: 'ACTOR_NOT_FOUND',
        message: message || 'Actor not found; no ragdoll applied',
        actorName: argsTyped.actorName
      });
    }

    return cleanObject(resp);
  }

  // Flatten blend space axis parameters for C++ handler
  const mutableArgs = { ...argsTyped } as AnimationArgs & Record<string, unknown>;
  if (animAction === 'create_blend_space' || animAction === 'create_blend_tree') {
    if (argsTyped.horizontalAxis) {
      mutableArgs.minX = argsTyped.horizontalAxis.minValue;
      mutableArgs.maxX = argsTyped.horizontalAxis.maxValue;
    }
    if (argsTyped.verticalAxis) {
      mutableArgs.minY = argsTyped.verticalAxis.minValue;
      mutableArgs.maxY = argsTyped.verticalAxis.maxValue;
    }
  }

  switch (animAction) {
    case 'create_blend_space': {
      // Use executeAutomationRequest to pass all params including flattened axis params
      const payload = {
        name: mutableArgs.name,
        path: mutableArgs.path || mutableArgs.savePath,
        savePath: mutableArgs.savePath || mutableArgs.path,
        skeletonPath: mutableArgs.skeletonPath,
        horizontalAxis: mutableArgs.horizontalAxis,
        verticalAxis: mutableArgs.verticalAxis,
        // Pass flattened axis params for C++ handler
        minX: mutableArgs.minX,
        maxX: mutableArgs.maxX,
        minY: mutableArgs.minY,
        maxY: mutableArgs.maxY,
        subAction: 'create_blend_space'
      };
      const res = await executeAutomationRequest(tools, 'animation_physics', payload, 'Automation bridge not available for blend space creation');
      return cleanObject(res) as Record<string, unknown>;
    }
    case 'create_state_machine':
      return cleanObject(await executeAutomationRequest(tools, 'manage_animation_authoring', {
        subAction: 'add_state_machine',
        machineName: mutableArgs.machineName || mutableArgs.name,
        states: mutableArgs.states as unknown[],
        transitions: mutableArgs.transitions as unknown[],
        blueprintPath: mutableArgs.blueprintPath || mutableArgs.path || mutableArgs.savePath
      })) as Record<string, unknown>;
    case 'setup_ik':
      return cleanObject(await executeAutomationRequest(tools, 'animation_physics', {
        action: 'setup_ik',
        actorName: mutableArgs.actorName,
        ikBones: mutableArgs.ikBones as unknown[],
        enableFootPlacement: mutableArgs.enableFootPlacement
      })) as Record<string, unknown>;
    case 'create_procedural_anim': {
      // TODO: Requires C++ implementation for procedural animation system creation
      return cleanObject({
        success: false,
        isError: true,
        error: 'NOT_IMPLEMENTED',
        message: 'create_procedural_anim requires engine-side implementation. C++ handler needed.'
      });
    }
    case 'create_blend_tree': {
      // TODO: Requires C++ implementation for blend tree creation
      return cleanObject({
        success: false,
        isError: true,
        error: 'NOT_IMPLEMENTED',
        message: 'create_blend_tree requires engine-side implementation. C++ handler needed.'
      });
    }
    case 'cleanup':
      return cleanObject(await executeAutomationRequest(tools, 'animation_physics', {
        action: 'cleanup',
        artifacts: mutableArgs.artifacts as unknown[]
      })) as Record<string, unknown>;
    case 'create_animation_asset': {
      let assetType = mutableArgs.assetType;
      if (!assetType && mutableArgs.name) {
        if (mutableArgs.name.toLowerCase().endsWith('montage') || mutableArgs.name.toLowerCase().includes('montage')) {
          assetType = 'montage';
        }
      }
      return cleanObject(await executeAutomationRequest(tools, 'animation_physics', {
        action: 'create_animation_asset',
        name: mutableArgs.name,
        savePath: mutableArgs.path || mutableArgs.savePath,
        skeletonPath: mutableArgs.skeletonPath,
        assetType
      })) as Record<string, unknown>;
    }
    case 'add_notify':
      return cleanObject(await executeAutomationRequest(tools, 'animation_physics', {
        action: 'add_notify',
        assetPath: mutableArgs.animationPath || mutableArgs.assetPath,
        notifyName: mutableArgs.notifyName || mutableArgs.name,
        time: mutableArgs.time ?? mutableArgs.startTime
      })) as Record<string, unknown>;
    case 'configure_vehicle':
      // configureVehicle uses console commands via automation bridge
      return cleanObject(await executeAutomationRequest(tools, 'animation_physics', {
        action: 'configure_vehicle',
        vehicleName: mutableArgs.vehicleName,
        vehicleType: mutableArgs.vehicleType,
        wheels: mutableArgs.wheels as unknown[],
        engine: mutableArgs.engine,
        transmission: mutableArgs.transmission
      })) as Record<string, unknown>;
    case 'setup_physics_simulation': {
      // Support both meshPath/skeletonPath and actorName parameters
      const payload: Record<string, unknown> = {
        meshPath: mutableArgs.meshPath,
        skeletonPath: mutableArgs.skeletonPath,
        physicsAssetName: mutableArgs.physicsAssetName,
        savePath: mutableArgs.savePath
      };

      // If actorName is provided but no meshPath, resolve the skeletal mesh from the actor
      if (mutableArgs.actorName && !mutableArgs.meshPath && !mutableArgs.skeletonPath) {
        payload.actorName = mutableArgs.actorName;
      }

      // Ensure at least one source is provided
      if (!payload.meshPath && !payload.skeletonPath && !payload.actorName) {
        return cleanObject({
          success: false,
          error: 'INVALID_ARGUMENT',
          message: 'setup_physics_simulation requires meshPath, skeletonPath, or actorName parameter'
        });
      }

      return cleanObject(await executeAutomationRequest(tools, 'animation_physics', {
        action: 'setup_physics_simulation',
        ...payload
      })) as Record<string, unknown>;
    }
    default: {
      const res = await executeAutomationRequest(tools, 'animation_physics', args, 'Automation bridge not available for animation/physics operations');
      return cleanObject(res) as Record<string, unknown>;
    }
  }
}
