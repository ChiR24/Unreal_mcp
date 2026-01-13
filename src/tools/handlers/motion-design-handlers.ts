import { executeAutomationRequest } from './common-handlers.js';
import { sanitizePathSafe } from '../../utils/validation.js';
import { ITools } from '../../types/tool-interfaces.js';

/**
 * Handle Motion Design tools
 */
export async function handleMotionDesignTools(
  action: string,
  args: Record<string, unknown>,
  tools: ITools
): Promise<unknown> {

  switch (action) {
    case 'create_cloner': {
      const actorName = args.clonerName as string;
      const clonerType = args.clonerType as string; // Grid, Linear, Radial, etc.
      const sourceActor = args.sourceActor as string | undefined;
      const location = args.location as { x: number; y: number; z: number };
      
      return await executeAutomationRequest(tools, 'manage_motion_design', {
        action: 'create_cloner',
        clonerName: actorName,
        clonerType,
        sourceActor,
        location
      });
    }

    case 'configure_cloner_pattern': {
      const clonerActor = args.clonerActor as string;
      const countX = args.countX as number;
      const countY = args.countY as number;
      const countZ = args.countZ as number;
      const offset = args.offset as { x: number; y: number; z: number };
      const rotation = args.rotation as { roll: number; pitch: number; yaw: number };
      const scale = args.scale as { x: number; y: number; z: number };

      return await executeAutomationRequest(tools, 'manage_motion_design', {
        action: 'configure_cloner_pattern',
        clonerActor,
        countX,
        countY,
        countZ,
        offset,
        rotation,
        scale
      });
    }

    case 'add_effector': {
      const clonerActor = args.clonerActor as string;
      const effectorType = args.effectorType as string; // Noise, Step, etc.
      const effectorName = args.effectorName as string;

      return await executeAutomationRequest(tools, 'manage_motion_design', {
        action: 'add_effector',
        clonerActor,
        effectorType,
        effectorName
      });
    }

    case 'animate_effector': {
      const effectorActor = args.effectorActor as string;
      const propertyName = args.propertyName as string;
      const startValue = args.startValue as number;
      const endValue = args.endValue as number;
      const duration = args.duration as number;

      return await executeAutomationRequest(tools, 'manage_motion_design', {
        action: 'animate_effector',
        effectorActor,
        propertyName,
        startValue,
        endValue,
        duration
      });
    }

    case 'create_mograph_sequence': {
      const sequencePath = sanitizePathSafe(args.sequencePath as string);

      return await executeAutomationRequest(tools, 'manage_motion_design', {
        action: 'create_mograph_sequence',
        sequencePath
      });
    }

    case 'create_radial_cloner': {
      const actorName = args.clonerName as string;
      const radius = args.radius as number;
      const count = args.count as number;
      const axis = args.axis as string;
      const align = args.align as boolean;
      const location = args.location as { x: number; y: number; z: number };

      return await executeAutomationRequest(tools, 'manage_motion_design', {
        action: 'create_radial_cloner',
        clonerName: actorName,
        radius,
        count,
        axis,
        align,
        location
      });
    }

    case 'create_spline_cloner': {
      const actorName = args.clonerName as string;
      const splineActor = args.splineActor as string;
      const count = args.count as number;
      const location = args.location as { x: number; y: number; z: number };

      return await executeAutomationRequest(tools, 'manage_motion_design', {
        action: 'create_spline_cloner',
        clonerName: actorName,
        splineActor,
        count,
        location
      });
    }

    case 'add_noise_effector': {
      const clonerActor = args.clonerActor as string;
      const effectorName = args.effectorName as string;
      const strength = args.strength as number;
      const frequency = args.frequency as number;

      return await executeAutomationRequest(tools, 'manage_motion_design', {
        action: 'add_noise_effector',
        clonerActor,
        effectorName,
        strength,
        frequency
      });
    }

    case 'configure_step_effector': {
      const effectorActor = args.effectorActor as string;
      const stepCount = args.stepCount as number;
      const operation = args.operation as string;

      return await executeAutomationRequest(tools, 'manage_motion_design', {
        action: 'configure_step_effector',
        effectorActor,
        stepCount,
        operation
      });
    }

    case 'export_mograph_to_sequence': {
      const sourceActor = args.clonerActor as string;
      const sequencePath = sanitizePathSafe(args.sequencePath as string);

      return await executeAutomationRequest(tools, 'manage_motion_design', {
        action: 'export_mograph_to_sequence',
        sourceActor,
        sequencePath
      });
    }

    default:
      throw new Error(`Unknown Motion Design action: ${action}`);
  }
}
