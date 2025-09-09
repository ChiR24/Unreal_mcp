import { UnrealBridge } from '../unreal-bridge.js';

export class AnimationTools {
  constructor(private bridge: UnrealBridge) {}

  /**
   * Create Animation Blueprint
   */
  async createAnimationBlueprint(params: {
    name: string;
    skeletonPath: string;
    savePath?: string;
  }) {
    try {
      // Animation blueprints require specific console commands and asset creation
      const commands = [
        `CreateAsset AnimBlueprint ${params.name} ${params.savePath || '/Game/Animations'}`,
        `SetAnimBlueprintSkeleton ${params.name} ${params.skeletonPath}`
      ];
      
      for (const cmd of commands) {
        await this.executeCommand(cmd);
      }
      
      return { 
        success: true, 
        message: `Animation Blueprint ${params.name} created`,
        path: `${params.savePath || '/Game/Animations'}/${params.name}`
      };
    } catch (err) {
      return { success: false, error: `Failed to create AnimBlueprint: ${err}` };
    }
  }

  /**
   * Add State Machine to Animation Blueprint
   */
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
  }) {
    try {
      // State machines are complex - we'll use console commands for basic setup
      const commands = [
        `AddAnimStateMachine ${params.blueprintPath} ${params.machineName}`
      ];
      
      // Add states
      for (const state of params.states) {
        commands.push(
          `AddAnimState ${params.blueprintPath} ${params.machineName} ${state.name} ${state.animation || ''}`
        );
        if (state.isEntry) {
          commands.push(`SetAnimStateEntry ${params.blueprintPath} ${params.machineName} ${state.name}`);
        }
      }
      
      // Add transitions
      if (params.transitions) {
        for (const transition of params.transitions) {
          commands.push(
            `AddAnimTransition ${params.blueprintPath} ${params.machineName} ${transition.sourceState} ${transition.targetState}`
          );
        }
      }
      
      for (const cmd of commands) {
        await this.executeCommand(cmd);
      }
      
      return { 
        success: true, 
        message: `State machine ${params.machineName} added to ${params.blueprintPath}` 
      };
    } catch (err) {
      return { success: false, error: `Failed to add state machine: ${err}` };
    }
  }

  /**
   * Create Animation Montage
   */
  async createMontage(params: {
    name: string;
    animationSequence: string;
    savePath?: string;
    sections?: Array<{
      name: string;
      startTime: number;
      endTime: number;
    }>;
    notifies?: Array<{
      name: string;
      time: number;
    }>;
  }) {
    try {
      const path = params.savePath || '/Game/Animations/Montages';
      const commands = [
        `CreateAsset AnimMontage ${params.name} ${path}`,
        `SetMontageAnimation ${params.name} ${params.animationSequence}`
      ];
      
      // Add sections
      if (params.sections) {
        for (const section of params.sections) {
          commands.push(
            `AddMontageSection ${params.name} ${section.name} ${section.startTime} ${section.endTime}`
          );
        }
      }
      
      // Add notifies
      if (params.notifies) {
        for (const notify of params.notifies) {
          commands.push(
            `AddMontageNotify ${params.name} ${notify.name} ${notify.time}`
          );
        }
      }
      
      for (const cmd of commands) {
        await this.executeCommand(cmd);
      }
      
      return { 
        success: true, 
        message: `Animation Montage ${params.name} created`,
        path: `${path}/${params.name}`
      };
    } catch (err) {
      return { success: false, error: `Failed to create montage: ${err}` };
    }
  }

  /**
   * Create Blend Space
   */
  async createBlendSpace(params: {
    name: string;
    skeletonPath: string;
    savePath?: string;
    dimensions: 1 | 2;
    horizontalAxis?: {
      name: string;
      minValue: number;
      maxValue: number;
    };
    verticalAxis?: {
      name: string;
      minValue: number;
      maxValue: number;
    };
    samples?: Array<{
      animation: string;
      x: number;
      y?: number;
    }>;
  }) {
    try {
      const path = params.savePath || '/Game/Animations/BlendSpaces';
      const blendSpaceType = params.dimensions === 1 ? 'BlendSpace1D' : 'BlendSpace';
      
      const commands = [
        `CreateAsset ${blendSpaceType} ${params.name} ${path}`,
        `SetBlendSpaceSkeleton ${params.name} ${params.skeletonPath}`
      ];
      
      // Configure axes
      if (params.horizontalAxis) {
        commands.push(
          `SetBlendSpaceAxis ${params.name} Horizontal ${params.horizontalAxis.name} ${params.horizontalAxis.minValue} ${params.horizontalAxis.maxValue}`
        );
      }
      
      if (params.dimensions === 2 && params.verticalAxis) {
        commands.push(
          `SetBlendSpaceAxis ${params.name} Vertical ${params.verticalAxis.name} ${params.verticalAxis.minValue} ${params.verticalAxis.maxValue}`
        );
      }
      
      // Add sample animations
      if (params.samples) {
        for (const sample of params.samples) {
          const coords = params.dimensions === 1 ? `${sample.x}` : `${sample.x} ${sample.y || 0}`;
          commands.push(
            `AddBlendSpaceSample ${params.name} ${sample.animation} ${coords}`
          );
        }
      }
      
      for (const cmd of commands) {
        await this.executeCommand(cmd);
      }
      
      return { 
        success: true, 
        message: `Blend Space ${params.name} created`,
        path: `${path}/${params.name}`
      };
    } catch (err) {
      return { success: false, error: `Failed to create blend space: ${err}` };
    }
  }

  /**
   * Setup Control Rig
   */
  async setupControlRig(params: {
    name: string;
    skeletonPath: string;
    savePath?: string;
    controls?: Array<{
      name: string;
      type: 'Transform' | 'Float' | 'Bool' | 'Vector';
      bone?: string;
      defaultValue?: any;
    }>;
  }) {
    try {
      const path = params.savePath || '/Game/Animations/ControlRigs';
      
      const commands = [
        `CreateAsset ControlRig ${params.name} ${path}`,
        `SetControlRigSkeleton ${params.name} ${params.skeletonPath}`
      ];
      
      // Add controls
      if (params.controls) {
        for (const control of params.controls) {
          commands.push(
            `AddControlRigControl ${params.name} ${control.name} ${control.type} ${control.bone || ''}`
          );
          if (control.defaultValue !== undefined) {
            commands.push(
              `SetControlRigDefault ${params.name} ${control.name} ${JSON.stringify(control.defaultValue)}`
            );
          }
        }
      }
      
      for (const cmd of commands) {
        await this.executeCommand(cmd);
      }
      
      return { 
        success: true, 
        message: `Control Rig ${params.name} created`,
        path: `${path}/${params.name}`
      };
    } catch (err) {
      return { success: false, error: `Failed to setup control rig: ${err}` };
    }
  }

  /**
   * Create Level Sequence (for cinematics)
   */
  async createLevelSequence(params: {
    name: string;
    savePath?: string;
    frameRate?: number;
    duration?: number;
    tracks?: Array<{
      actorName: string;
      trackType: 'Transform' | 'Animation' | 'Camera' | 'Event';
      keyframes?: Array<{
        time: number;
        value: any;
      }>;
    }>;
  }) {
    try {
      const path = params.savePath || '/Game/Cinematics';
      
      const commands = [
        `CreateAsset LevelSequence ${params.name} ${path}`,
        `SetSequenceFrameRate ${params.name} ${params.frameRate || 30}`,
        `SetSequenceDuration ${params.name} ${params.duration || 5}`
      ];
      
      // Add tracks
      if (params.tracks) {
        for (const track of params.tracks) {
          commands.push(
            `AddSequenceTrack ${params.name} ${track.actorName} ${track.trackType}`
          );
          
          // Add keyframes
          if (track.keyframes) {
            for (const keyframe of track.keyframes) {
              commands.push(
                `AddSequenceKey ${params.name} ${track.actorName} ${track.trackType} ${keyframe.time} ${JSON.stringify(keyframe.value)}`
              );
            }
          }
        }
      }
      
      for (const cmd of commands) {
        await this.executeCommand(cmd);
      }
      
      return { 
        success: true, 
        message: `Level Sequence ${params.name} created`,
        path: `${path}/${params.name}`
      };
    } catch (err) {
      return { success: false, error: `Failed to create level sequence: ${err}` };
    }
  }

  /**
   * Play Animation on Actor
   */
  async playAnimation(params: {
    actorName: string;
    animationType: 'Montage' | 'Sequence' | 'BlendSpace';
    animationPath: string;
    playRate?: number;
    loop?: boolean;
    blendInTime?: number;
    blendOutTime?: number;
  }) {
    try {
      let command = '';
      
      switch (params.animationType) {
        case 'Montage':
          command = `PlayMontage ${params.actorName} ${params.animationPath} ${params.playRate || 1} ${params.loop ? 'true' : 'false'}`;
          break;
        case 'Sequence':
          command = `PlayAnimSequence ${params.actorName} ${params.animationPath} ${params.loop ? 'true' : 'false'}`;
          break;
        case 'BlendSpace':
          command = `SetBlendSpaceInput ${params.actorName} ${params.animationPath}`;
          break;
      }
      
      await this.executeCommand(command);
      
      return { 
        success: true, 
        message: `Playing ${params.animationType} on ${params.actorName}` 
      };
    } catch (err) {
      return { success: false, error: `Failed to play animation: ${err}` };
    }
  }

  /**
   * Helper function to execute console commands
   */
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
}
