import { UnrealBridge } from '../unreal-bridge.js';
import { validateAssetParams } from '../utils/validation.js';

type CreateAnimationBlueprintSuccess = {
  success: true;
  message: string;
  path: string;
  exists?: boolean;
  skeleton?: string;
  warnings?: string[];
  details?: string[];
};

type CreateAnimationBlueprintFailure = {
  success: false;
  message: string;
  error: string;
  path?: string;
  exists?: boolean;
  skeleton?: string;
  warnings?: string[];
  details?: string[];
};

type PlayAnimationSuccess = {
  success: true;
  message: string;
  warnings?: string[];
  details?: string[];
  actorName?: string;
  animationType?: string;
  assetPath?: string;
};

type PlayAnimationFailure = {
  success: false;
  message: string;
  error: string;
  warnings?: string[];
  details?: string[];
  availableActors?: string[];
  actorName?: string;
  animationType?: string;
  assetPath?: string;
};

export class AnimationTools {
  private managedArtifacts = new Map<string, {
    path?: string;
    type: string;
    metadata?: Record<string, unknown>;
    createdAt: number;
  }>();

  constructor(private bridge: UnrealBridge) {}

  private normalizePath(basePath: string | undefined, name: string): { directory: string; assetName: string; fullPath: string } {
    const directory = (basePath && basePath.trim().replace(/\/+$/, '')) || '/Game/Animations';
    const sanitizedName = name.trim();
    const fullPath = `${directory}/${sanitizedName}`;
    return { directory, assetName: sanitizedName, fullPath };
  }

  private trackArtifact(key: string, info: { path?: string; type: string; metadata?: Record<string, unknown> }) {
    this.managedArtifacts.set(key, { ...info, createdAt: Date.now() });
  }

  private removeArtifacts(keys: string[]): string[] {
    const removed: string[] = [];
    for (const key of keys) {
      if (this.managedArtifacts.delete(key)) {
        removed.push(key);
      }
    }
    return removed;
  }

  async createAnimationBlueprint(params: {
    name: string;
    skeletonPath: string;
    savePath?: string;
  }): Promise<CreateAnimationBlueprintSuccess | CreateAnimationBlueprintFailure> {
    try {
      const targetPath = params.savePath ?? '/Game/Animations';
      const validation = validateAssetParams({ name: params.name, savePath: targetPath });
      if (!validation.valid) {
        const message = validation.error ?? 'Invalid asset parameters';
        return { success: false, message, error: message };
      }

      const sanitized = validation.sanitized;
      const assetName = sanitized.name;
      const assetPath = sanitized.savePath ?? targetPath;
      const fullPath = `${assetPath}/${assetName}`;

      const commands: string[] = [
        `CreateAsset AnimationBlueprint ${assetName} ${assetPath}`,
        `SetAnimBlueprintSkeleton ${assetName} ${params.skeletonPath}`
      ];

      await this.bridge.executeConsoleCommands(commands);
      this.trackArtifact(assetName, { path: fullPath, type: 'AnimationBlueprint' });

      return {
        success: true,
        message: `Animation Blueprint created at ${fullPath}`,
        path: fullPath,
        skeleton: params.skeletonPath
      };
    } catch (err) {
      const error = `Failed to create Animation Blueprint: ${err}`;
      return { success: false, message: error, error: String(err) };
    }
  }

  async addStateMachine(params: {
    blueprintPath: string;
    machineName: string;
    states: Array<{
      name: string;
      animation?: string;
      isEntry?: boolean;
      isExit?: boolean;
    }>;
    transitions?: Array<{
      sourceState: string;
      targetState: string;
      condition?: string;
    }>;
  }): Promise<{ success: true; message: string } | { success: false; error: string }> {
    try {
      if (!params.blueprintPath || !params.machineName) {
        return { success: false, error: 'blueprintPath and machineName are required' };
      }

      const commands: string[] = [
        `AddAnimStateMachine ${params.blueprintPath} ${params.machineName}`
      ];

      for (const state of params.states) {
        const animationName = state.animation ?? '';
        commands.push(
          `AddAnimState ${params.blueprintPath} ${params.machineName} ${state.name} ${animationName}`
        );
        if (state.isEntry) {
          commands.push(`SetAnimStateEntry ${params.blueprintPath} ${params.machineName} ${state.name}`);
        }
        if (state.isExit) {
          commands.push(`SetAnimStateExit ${params.blueprintPath} ${params.machineName} ${state.name}`);
        }
      }

      if (params.transitions) {
        for (const transition of params.transitions) {
          commands.push(
            `AddAnimTransition ${params.blueprintPath} ${params.machineName} ${transition.sourceState} ${transition.targetState}`
          );
          if (transition.condition) {
            commands.push(
              `SetAnimTransitionRule ${params.blueprintPath} ${params.machineName} ${transition.sourceState} ${transition.targetState} ${transition.condition}`
            );
          }
        }
      }

      await this.bridge.executeConsoleCommands(commands);
      return {
        success: true,
        message: `State machine ${params.machineName} added to ${params.blueprintPath}`
      };
    } catch (err) {
      return { success: false, error: `Failed to add state machine: ${err}` };
    }
  }

  async createBlendSpace(params: {
    name: string;
    savePath?: string;
    dimensions?: 1 | 2;
    skeletonPath?: string;
    horizontalAxis?: { name: string; minValue: number; maxValue: number };
    verticalAxis?: { name: string; minValue: number; maxValue: number };
    samples?: Array<{ animation: string; x: number; y?: number }>;
  }): Promise<{ success: true; message: string; path: string } | { success: false; error: string }> {
    try {
      const targetPath = params.savePath ?? '/Game/Animations';
      const validation = validateAssetParams({ name: params.name, savePath: targetPath });
      if (!validation.valid) {
        return { success: false, error: validation.error ?? 'Invalid asset parameters' };
      }

      const sanitized = validation.sanitized;
      const assetName = sanitized.name;
      const assetPath = sanitized.savePath ?? targetPath;
      const dimensions = params.dimensions === 2 ? 2 : 1;
      const blendSpaceType = dimensions === 2 ? 'BlendSpace' : 'BlendSpace1D';

      const commands: string[] = [
        `CreateAsset ${blendSpaceType} ${assetName} ${assetPath}`,
        `echo Creating ${blendSpaceType} ${assetName} at ${assetPath}`
      ];

      if (params.skeletonPath) {
        commands.push(`SetBlendSpaceSkeleton ${assetName} ${params.skeletonPath}`);
      }

      if (params.horizontalAxis) {
        commands.push(
          `SetBlendSpaceAxis ${assetName} Horizontal ${params.horizontalAxis.name} ${params.horizontalAxis.minValue} ${params.horizontalAxis.maxValue}`
        );
      }

      if (dimensions === 2 && params.verticalAxis) {
        commands.push(
          `SetBlendSpaceAxis ${assetName} Vertical ${params.verticalAxis.name} ${params.verticalAxis.minValue} ${params.verticalAxis.maxValue}`
        );
      }

      if (params.samples) {
        for (const sample of params.samples) {
          const coords = dimensions === 1 ? `${sample.x}` : `${sample.x} ${sample.y ?? 0}`;
          commands.push(`AddBlendSpaceSample ${assetName} ${sample.animation} ${coords}`);
        }
      }

      await this.bridge.executeConsoleCommands(commands);
      return {
        success: true,
        message: `Blend Space ${assetName} created`,
        path: `${assetPath}/${assetName}`
      };
    } catch (err) {
      return { success: false, error: `Failed to create blend space: ${err}` };
    }
  }

  async setupControlRig(params: {
    name: string;
    skeletonPath: string;
    savePath?: string;
    controls?: Array<{
      name: string;
      type: 'Transform' | 'Float' | 'Bool' | 'Vector';
      bone?: string;
      defaultValue?: unknown;
    }>;
  }): Promise<{ success: true; message: string; path: string } | { success: false; error: string }> {
    try {
      const targetPath = params.savePath ?? '/Game/Animations';
      const validation = validateAssetParams({ name: params.name, savePath: targetPath });
      if (!validation.valid) {
        return { success: false, error: validation.error ?? 'Invalid asset parameters' };
      }

      const sanitized = validation.sanitized;
      const assetName = sanitized.name;
      const assetPath = sanitized.savePath ?? targetPath;
      const fullPath = `${assetPath}/${assetName}`;

      const commands: string[] = [
        `CreateAsset ControlRig ${assetName} ${assetPath}`,
        `SetControlRigSkeleton ${assetName} ${params.skeletonPath}`
      ];

      if (params.controls) {
        for (const control of params.controls) {
          commands.push(
            `AddControlRigControl ${assetName} ${control.name} ${control.type} ${control.bone ?? ''}`
          );
          if (control.defaultValue !== undefined) {
            commands.push(
              `SetControlRigDefault ${assetName} ${control.name} ${JSON.stringify(control.defaultValue)}`
            );
          }
        }
      }

      await this.bridge.executeConsoleCommands(commands);
      return {
        success: true,
        message: `Control Rig ${assetName} created`,
        path: fullPath
      };
    } catch (err) {
      return { success: false, error: `Failed to setup control rig: ${err}` };
    }
  }

  async createLevelSequence(params: {
    name: string;
    savePath?: string;
    frameRate?: number;
    duration?: number;
    tracks?: Array<{
      actorName: string;
      trackType: 'Transform' | 'Animation' | 'Camera' | 'Event';
      keyframes?: Array<{ time: number; value: unknown }>;
    }>;
  }): Promise<{ success: true; message: string; path: string } | { success: false; error: string }> {
    try {
      const targetPath = params.savePath ?? '/Game/Cinematics';
      const validation = validateAssetParams({ name: params.name, savePath: targetPath });
      if (!validation.valid) {
        return { success: false, error: validation.error ?? 'Invalid asset parameters' };
      }

      const sanitized = validation.sanitized;
      const assetName = sanitized.name;
      const assetPath = sanitized.savePath ?? targetPath;

      const commands: string[] = [
        `CreateAsset LevelSequence ${assetName} ${assetPath}`,
        `SetSequenceFrameRate ${assetName} ${params.frameRate ?? 30}`,
        `SetSequenceDuration ${assetName} ${params.duration ?? 5}`
      ];

      if (params.tracks) {
        for (const track of params.tracks) {
          commands.push(`AddSequenceTrack ${assetName} ${track.actorName} ${track.trackType}`);
          if (track.keyframes) {
            for (const keyframe of track.keyframes) {
              commands.push(
                `AddSequenceKey ${assetName} ${track.actorName} ${track.trackType} ${keyframe.time} ${JSON.stringify(keyframe.value)}`
              );
            }
          }
        }
      }

      await this.bridge.executeConsoleCommands(commands);
      return {
        success: true,
        message: `Level Sequence ${assetName} created`,
        path: `${assetPath}/${assetName}`
      };
    } catch (err) {
      return { success: false, error: `Failed to create level sequence: ${err}` };
    }
  }

  async playAnimation(params: {
    actorName: string;
    animationType: 'Montage' | 'Sequence' | 'BlendSpace';
    animationPath: string;
    playRate?: number;
    loop?: boolean;
    blendInTime?: number;
    blendOutTime?: number;
  }): Promise<PlayAnimationSuccess | PlayAnimationFailure> {
    try {
      const commands: string[] = [
        `PlayAnimation ${params.actorName} ${params.animationType} ${params.animationPath} ${params.playRate ?? 1.0} ${params.loop ?? false} ${params.blendInTime ?? 0.25} ${params.blendOutTime ?? 0.25}`
      ];

      await this.bridge.executeConsoleCommands(commands);

      return {
        success: true,
        message: `Animation ${params.animationType} triggered on ${params.actorName}`,
        actorName: params.actorName,
        animationType: params.animationType,
        assetPath: params.animationPath
      };
    } catch (err) {
      const error = `Failed to play animation: ${err}`;
      return { success: false, message: error, error: String(err) };
    }
  }

}
