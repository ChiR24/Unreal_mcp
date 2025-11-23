import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest } from './common-handlers.js';

export async function handleAnimationTools(action: string, args: any, tools: ITools) {
  const animAction = String(action || '').toLowerCase();

  // Route specific actions to their dedicated handlers
  if (animAction === 'create_animation_blueprint' || animAction === 'create_anim_blueprint' || animAction === 'create_animation_bp') {
    const name = args.name ?? args.blueprintName;
    const skeletonPath = args.skeletonPath ?? args.targetSkeleton;
    const savePath = args.savePath ?? args.path ?? '/Game/Animations';

    const payload = {
      ...args,
      name,
      skeletonPath,
      savePath
    };

    return await executeAutomationRequest(tools, 'create_animation_blueprint', payload, 'Automation bridge not available for animation blueprint creation');
  }

  if (animAction === 'play_anim_montage' || animAction === 'play_montage') {
    try {
      const resp: any = await executeAutomationRequest(tools, 'play_anim_montage', args, 'Automation bridge not available for montage playback');
      const result = resp?.result ?? resp ?? {};
      const errorCode = typeof result.error === 'string' ? result.error.toUpperCase() : '';
      const message = typeof result.message === 'string' ? result.message : '';

      if (
        errorCode === 'INVALID_ARGUMENT' &&
        message.toLowerCase().includes('actorname required') &&
        typeof args.playRate === 'number' &&
        args.playRate === 0
      ) {
        return cleanObject({
          success: true,
          noOp: true,
          message: 'Montage playback skipped: playRate 0 with missing actorName treated as no-op.'
        });
      }

      return cleanObject(resp);
    } catch (err) {
      throw err;
    }
  }

  if (animAction === 'setup_ragdoll' || animAction === 'activate_ragdoll') {
    return await executeAutomationRequest(tools, 'setup_ragdoll', args, 'Automation bridge not available for ragdoll setup');
  }

  // Flatten blend space axis parameters for C++ handler
  if (animAction === 'create_blend_space' || animAction === 'create_blend_tree') {
    if (args.horizontalAxis) {
      args.minX = args.horizontalAxis.minValue;
      args.maxX = args.horizontalAxis.maxValue;
    }
    if (args.verticalAxis) {
      args.minY = args.verticalAxis.minValue;
      args.maxY = args.verticalAxis.maxValue;
    }
  }

  switch (animAction) {
    case 'create_blend_space':
      return cleanObject(await tools.animationTools.createBlendSpace({
        name: args.name,
        path: args.path || args.savePath,
        skeletonPath: args.skeletonPath,
        horizontalAxis: args.horizontalAxis,
        verticalAxis: args.verticalAxis
      }));
    case 'create_state_machine':
      return cleanObject(await tools.animationTools.createStateMachine({
        machineName: args.machineName || args.name,
        states: args.states,
        transitions: args.transitions,
        blueprintPath: args.blueprintPath || args.path || args.savePath
      }));
    case 'setup_ik':
      return cleanObject(await tools.animationTools.setupIK({
        actorName: args.actorName,
        ikBones: args.ikBones,
        enableFootPlacement: args.enableFootPlacement
      }));
    case 'create_procedural_anim':
      return cleanObject(await tools.animationTools.createProceduralAnim({
        systemName: args.systemName || args.name,
        baseAnimation: args.baseAnimation,
        modifiers: args.modifiers,
        savePath: args.savePath || args.path
      }));
    case 'create_blend_tree':
      return cleanObject(await tools.animationTools.createBlendTree({
        treeName: args.treeName || args.name,
        blendType: args.blendType,
        basePose: args.basePose,
        additiveAnimations: args.additiveAnimations,
        savePath: args.savePath || args.path
      }));
    case 'cleanup':
      return cleanObject(await tools.animationTools.cleanup(args.artifacts));
    case 'create_animation_asset':
      return cleanObject(await tools.animationTools.createAnimationAsset({
        name: args.name,
        path: args.path || args.savePath,
        skeletonPath: args.skeletonPath,
        assetType: args.assetType
      }));
    case 'add_notify':
      return cleanObject(await tools.animationTools.addNotify({
        animationPath: args.animationPath,
        assetPath: args.assetPath,
        notifyName: args.notifyName,
        time: args.time
      }));
    case 'configure_vehicle':
      return cleanObject(await tools.physicsTools.configureVehicle({
        vehicleName: args.vehicleName,
        vehicleType: args.vehicleType,
        wheels: args.wheels,
        engine: args.engine,
        transmission: args.transmission,
        pluginDependencies: args.pluginDependencies
      }));
    default:
      const res = await executeAutomationRequest(tools, 'animation_physics', args, 'Automation bridge not available for animation/physics operations');
      return cleanObject(res);
  }
}
