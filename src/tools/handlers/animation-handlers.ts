import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs, AnimationArgs, ComponentInfo, AutomationResponse } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';
import { normalizeArgs } from './argument-helper.js';
import { TOOL_ACTIONS } from '../../utils/action-constants.js';
import { sanitizePath } from '../../utils/path-security.js';

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

  // Global path security validation - validate ALL path parameters in args
  // This catches injected malicious paths regardless of which parameter they're in
  const allPathParams = [
    'path', 'savePath', 'skeletonPath', 'skeletalMeshPath', 'sourceSkeleton', 'targetSkeleton',
    'assetPath', 'animationPath', 'blueprintPath', 'retargeterPath', 'meshPath', 'montagePath',
    'animSequencePath', 'animPath', 'animAssetPath', 'animMontagePath', 'blendSpacePath', 'rigPath'
  ];
  
  for (const param of allPathParams) {
    const value = (args as Record<string, unknown>)[param];
    if (value && typeof value === 'string') {
      try {
        sanitizePath(value);
      } catch (e) {
        return cleanObject({
          success: false,
          error: 'SECURITY_VIOLATION',
          message: e instanceof Error ? e.message : `Invalid ${param}: path traversal or illegal characters detected`
        });
      }
    }
  }
  
  // Validate paths in arrays (e.g., artifacts in cleanup)
  const artifacts = (args as Record<string, unknown>).artifacts;
  if (Array.isArray(artifacts)) {
    for (const artifact of artifacts) {
      if (typeof artifact === 'string') {
        try {
          sanitizePath(artifact);
        } catch (e) {
          return cleanObject({
            success: false,
            error: 'SECURITY_VIOLATION',
            message: e instanceof Error ? e.message : 'Invalid path in artifacts: path traversal or illegal characters detected'
          });
        }
      }
    }
  }

  // Route specific actions to their dedicated handlers
  if (animAction === 'create_animation_blueprint' || animAction === 'create_anim_blueprint' || animAction === 'create_animation_bp') {
    const name = argsTyped.name ?? argsTyped.blueprintName;
    const skeletonPath = argsTyped.skeletonPath ?? argsTyped.targetSkeleton;
    let meshPath = argsTyped.meshPath;
    
    // Validate and sanitize savePath
    let savePath: string;
    try {
      const rawPath = String(argsTyped.savePath ?? argsTyped.path ?? '/Game/Animations');
      savePath = sanitizePath(rawPath);
    } catch (e) {
      return cleanObject({
        success: false,
        error: 'SECURITY_VIOLATION',
        message: e instanceof Error ? e.message : 'Invalid path: path traversal or illegal characters detected'
      });
    }

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
      // Validate and sanitize paths
      let savePath: string;
      try {
        const rawPath = String(mutableArgs.path || mutableArgs.savePath || '/Game/Animations');
        savePath = sanitizePath(rawPath);
      } catch (e) {
        return cleanObject({
          success: false,
          error: 'SECURITY_VIOLATION',
          message: e instanceof Error ? e.message : 'Invalid path: path traversal or illegal characters detected'
        });
      }
      // Use executeAutomationRequest to pass all params including flattened axis params
      // Note: C++ handler reads 'action' field, not 'subAction'
      const payload = {
        action: 'create_blend_space',
        name: mutableArgs.name,
        path: savePath,
        savePath,
        skeletonPath: mutableArgs.skeletonPath,
        horizontalAxis: mutableArgs.horizontalAxis,
        verticalAxis: mutableArgs.verticalAxis,
        // Pass flattened axis params for C++ handler
        minX: mutableArgs.minX,
        maxX: mutableArgs.maxX,
        minY: mutableArgs.minY,
        maxY: mutableArgs.maxY
      };
      const res = await executeAutomationRequest(tools, 'animation_physics', payload, 'Automation bridge not available for blend space creation');
      return cleanObject(res) as Record<string, unknown>;
    }
    case 'create_state_machine': {
      // Validate blueprint path
      let blueprintPath: string | undefined;
      try {
        const rawPath = mutableArgs.blueprintPath || mutableArgs.path || mutableArgs.savePath;
        if (rawPath && typeof rawPath === 'string') {
          blueprintPath = sanitizePath(rawPath);
        }
      } catch (e) {
        return cleanObject({
          success: false,
          error: 'SECURITY_VIOLATION',
          message: e instanceof Error ? e.message : 'Invalid path: path traversal or illegal characters detected'
        });
      }
      // Note: C++ handler reads 'action' field, not 'subAction'
      return cleanObject(await executeAutomationRequest(tools, TOOL_ACTIONS.ANIMATION_PHYSICS, {
        action: 'create_state_machine',
        machineName: mutableArgs.machineName || mutableArgs.name,
        states: mutableArgs.states as unknown[],
        transitions: mutableArgs.transitions as unknown[],
        blueprintPath
      })) as Record<string, unknown>;
    }
    case 'setup_ik': {
      // Validate and sanitize paths
      let savePath: string;
      try {
        const rawPath = String(mutableArgs.savePath || mutableArgs.path || '/Game/Animations');
        savePath = sanitizePath(rawPath);
      } catch (e) {
        return cleanObject({
          success: false,
          error: 'SECURITY_VIOLATION',
          message: e instanceof Error ? e.message : 'Invalid path: path traversal or illegal characters detected'
        });
      }
      // Pass ALL required parameters to C++ handler
      return cleanObject(await executeAutomationRequest(tools, 'animation_physics', {
        action: 'setup_ik',
        name: mutableArgs.name,
        savePath,
        skeletonPath: mutableArgs.skeletonPath,
        actorName: mutableArgs.actorName,
        ikBones: mutableArgs.ikBones as unknown[],
        enableFootPlacement: mutableArgs.enableFootPlacement
      })) as Record<string, unknown>;
    }
  case 'create_procedural_anim': {
    const params = normalizeArgs(args, [
      { key: 'name', required: true },
      { key: 'path', aliases: ['directory'], default: '/Game/Animations' },
      { key: 'skeletonPath', required: true },
      { key: 'boneTracks', required: true },
      { key: 'numFrames', default: 30 },
      { key: 'frameRate', default: 30 },
      { key: 'save', default: true }
    ]);

    let savePath: string;
    let skeletonPath: string;
    try {
      savePath = sanitizePath(String(params.path || '/Game/Animations'));
      skeletonPath = sanitizePath(String(params.skeletonPath));
    } catch (e) {
      return cleanObject({
        success: false,
        error: 'SECURITY_VIOLATION',
        message: e instanceof Error ? e.message : 'Invalid path: path traversal or illegal characters detected'
      });
    }

    if (!Array.isArray(params.boneTracks)) {
      return cleanObject({
        success: false,
        error: 'MISSING_REQUIRED_PARAM',
        message: 'boneTracks is required and must be an array for create_procedural_anim'
      });
    }
    const boneTracks = params.boneTracks;

    return cleanObject(await executeAutomationRequest(tools, 'animation_physics', {
      action: 'create_procedural_anim',
      subAction: 'create_procedural_anim',
      name: params.name,
      path: savePath,
      savePath,
      skeletonPath,
      boneTracks,
      numFrames: params.numFrames,
      frameRate: params.frameRate,
      save: params.save
    })) as Record<string, unknown>;
  }
    case 'create_blend_tree': {
      // Validate blueprint path
      let blueprintPath: string | undefined;
      try {
        const rawPath = mutableArgs.blueprintPath || mutableArgs.path || mutableArgs.savePath;
        if (rawPath && typeof rawPath === 'string') {
          blueprintPath = sanitizePath(rawPath);
        }
      } catch (e) {
        return cleanObject({
          success: false,
          error: 'SECURITY_VIOLATION',
          message: e instanceof Error ? e.message : 'Invalid path: path traversal or illegal characters detected'
        });
      }

      if (!blueprintPath) {
        return cleanObject({
          success: false,
          error: 'INVALID_ARGUMENT',
          message: 'blueprintPath is required for create_blend_tree'
        });
      }

      // Validate and sanitize animation paths in children array
      const sanitizedChildren: unknown[] = [];
      if (Array.isArray(mutableArgs.children)) {
        for (const child of mutableArgs.children as Record<string, unknown>[]) {
          if (child && typeof child === 'object' && child.animationPath) {
            try {
              const sanitizedPath = sanitizePath(String(child.animationPath));
              sanitizedChildren.push({ ...child, animationPath: sanitizedPath });
            } catch {
              return cleanObject({
                success: false,
                error: 'SECURITY_VIOLATION',
                message: 'Invalid animationPath in children: path traversal or illegal characters detected'
              });
            }
          } else {
            sanitizedChildren.push(child);
          }
        }
      }

      // Build payload for C++ handler
      const payload: Record<string, unknown> = {
        action: 'create_blend_tree',
        blueprintPath,
        treeName: mutableArgs.treeName || mutableArgs.name || 'BlendTree',
        blendParameters: mutableArgs.blendParameters as unknown[],
        children: sanitizedChildren.length > 0 ? sanitizedChildren : mutableArgs.children as unknown[],
        save: mutableArgs.save !== false
      };

      return cleanObject(await executeAutomationRequest(tools, TOOL_ACTIONS.ANIMATION_PHYSICS, payload)) as Record<string, unknown>;
    }
    case 'cleanup':
      return cleanObject(await executeAutomationRequest(tools, 'animation_physics', {
        action: 'cleanup',
        artifacts: mutableArgs.artifacts as unknown[]
      })) as Record<string, unknown>;
    case 'create_animation_asset': {
      // Validate and sanitize path
      let savePath: string;
      try {
        const rawPath = String(mutableArgs.path || mutableArgs.savePath || '/Game/Animations');
        savePath = sanitizePath(rawPath);
      } catch (e) {
        return cleanObject({
          success: false,
          error: 'SECURITY_VIOLATION',
          message: e instanceof Error ? e.message : 'Invalid path: path traversal or illegal characters detected'
        });
      }
      let assetType = mutableArgs.assetType;
      if (!assetType && mutableArgs.name) {
        if (mutableArgs.name.toLowerCase().endsWith('montage') || mutableArgs.name.toLowerCase().includes('montage')) {
          assetType = 'montage';
        }
      }
      return cleanObject(await executeAutomationRequest(tools, 'animation_physics', {
        action: 'create_animation_asset',
        name: mutableArgs.name,
        savePath,
        skeletonPath: mutableArgs.skeletonPath,
        assetType
      })) as Record<string, unknown>;
    }
    case 'add_notify': {
      // Validate asset path
      let assetPath: string | undefined;
      try {
        const rawPath = mutableArgs.animationPath || mutableArgs.assetPath;
        if (rawPath && typeof rawPath === 'string') {
          assetPath = sanitizePath(rawPath);
        }
      } catch (e) {
        return cleanObject({
          success: false,
          error: 'SECURITY_VIOLATION',
          message: e instanceof Error ? e.message : 'Invalid path: path traversal or illegal characters detected'
        });
      }
      return cleanObject(await executeAutomationRequest(tools, 'animation_physics', {
        action: 'add_notify',
        assetPath,
        notifyName: mutableArgs.notifyName || mutableArgs.name,
        time: mutableArgs.time ?? mutableArgs.startTime
      })) as Record<string, unknown>;
    }
 case 'configure_vehicle': {
    const params = normalizeArgs(args, [
      { key: 'actorName', required: false },
      { key: 'vehicleName', required: false },
      { key: 'vehicleType', default: 'WheeledVehicle4W' },
      { key: 'wheels' },
      { key: 'engine' },
      { key: 'transmission' },
      { key: 'mass', default: 1500 },
      { key: 'dragCoefficient', default: 0.3 }
    ]);

    if (!params.actorName && !params.vehicleName) {
      return cleanObject({
        success: false,
        error: 'MISSING_REQUIRED_PARAM',
        message: 'At least one of actorName or vehicleName is required for configure_vehicle'
      });
    }

    return cleanObject(await executeAutomationRequest(tools, 'animation_physics', {
      action: 'configure_vehicle',
      subAction: 'configure_vehicle',
      actorName: params.actorName,
      vehicleName: params.vehicleName,
      vehicleType: params.vehicleType,
      wheels: params.wheels,
      engine: params.engine,
      transmission: params.transmission,
      mass: params.mass,
      dragCoefficient: params.dragCoefficient
    })) as Record<string, unknown>;
  }
    case 'setup_physics_simulation': {
      // Validate and sanitize paths
      let savePath: string | undefined;
      try {
        const rawSavePath = mutableArgs.savePath;
        if (rawSavePath && typeof rawSavePath === 'string') {
          savePath = sanitizePath(rawSavePath);
        }
      } catch (e) {
        return cleanObject({
          success: false,
          error: 'SECURITY_VIOLATION',
          message: e instanceof Error ? e.message : 'Invalid savePath: path traversal or illegal characters detected'
        });
      }

      let meshPath: string | undefined;
      try {
        const rawMeshPath = mutableArgs.meshPath;
        if (rawMeshPath && typeof rawMeshPath === 'string') {
          meshPath = sanitizePath(rawMeshPath);
        }
      } catch (e) {
        return cleanObject({
          success: false,
          error: 'SECURITY_VIOLATION',
          message: e instanceof Error ? e.message : 'Invalid meshPath: path traversal or illegal characters detected'
        });
      }

      let skeletonPath: string | undefined;
      try {
        const rawSkeletonPath = mutableArgs.skeletonPath;
        if (rawSkeletonPath && typeof rawSkeletonPath === 'string') {
          skeletonPath = sanitizePath(rawSkeletonPath);
        }
      } catch (e) {
        return cleanObject({
          success: false,
          error: 'SECURITY_VIOLATION',
          message: e instanceof Error ? e.message : 'Invalid skeletonPath: path traversal or illegal characters detected'
        });
      }

      // Support both meshPath/skeletonPath and actorName parameters
      const payload: Record<string, unknown> = {
        meshPath,
        skeletonPath,
        physicsAssetName: mutableArgs.physicsAssetName,
        savePath
      };

      // If actorName is provided but no meshPath, resolve the skeletal mesh from the actor
      if (mutableArgs.actorName && !meshPath && !skeletonPath) {
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
      // Path parameters already validated by global security check at top of function
      // Just pass args through to the bridge
      const res = await executeAutomationRequest(tools, 'animation_physics', args, 'Automation bridge not available for animation/physics operations');
      return cleanObject(res) as Record<string, unknown>;
    }
  }
}
