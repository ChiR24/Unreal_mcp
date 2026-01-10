import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';
import { cleanObject } from '../utils/safe-json.js';
import { validateAssetParams } from '../utils/validation.js';
import { wasmIntegration as _wasmIntegration } from '../wasm/index.js';

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

  private automationBridge?: AutomationBridge;

  constructor(private bridge: UnrealBridge, automationBridge?: AutomationBridge) {
    this.automationBridge = automationBridge;
  }

  setAutomationBridge(automationBridge?: AutomationBridge) {
    this.automationBridge = automationBridge;
  }

  private trackArtifact(key: string, info: { path?: string; type: string; metadata?: Record<string, unknown> }) {
    this.managedArtifacts.set(key, { ...info, createdAt: Date.now() });
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

      // Prefer native plugin support; surface real errors when creation fails.
      if (this.automationBridge && typeof this.automationBridge.sendAutomationRequest === 'function') {
        try {
          const resp = await this.automationBridge.sendAutomationRequest('create_animation_blueprint', {
            name: assetName,
            skeletonPath: params.skeletonPath,
            savePath: assetPath
          }, { timeoutMs: 60000 });

          const result = resp?.result ?? resp;
          const resultObj = result && typeof result === 'object' ? result as Record<string, unknown> : undefined;
          const warnings = Array.isArray(resultObj?.warnings) ? (resultObj.warnings as string[]) : undefined;
          const details = Array.isArray(resultObj?.details) ? (resultObj.details as string[]) : undefined;
          const isSuccess = resp && resp.success !== false && !!resultObj;

          if (isSuccess && resultObj) {
            const blueprintPath = typeof resultObj.blueprintPath === 'string' ? resultObj.blueprintPath : fullPath;
            this.trackArtifact(assetName, { path: blueprintPath, type: 'AnimationBlueprint' });
            return {
              success: true,
              message: resp.message || `Animation Blueprint created at ${blueprintPath}`,
              path: blueprintPath,
              skeleton: params.skeletonPath,
              warnings,
              details
            };
          }

          const message = typeof resp?.message === 'string'
            ? resp.message
            : (typeof resp?.error === 'string' ? resp.error : 'Animation Blueprint creation failed');
          const error = typeof resp?.error === 'string' ? resp.error : message;

          return {
            success: false,
            message,
            error,
            path: fullPath,
            skeleton: params.skeletonPath,
            warnings,
            details
          };
        } catch (err: unknown) {
          const error = String(err);
          return {
            success: false,
            message: `Failed to create Animation Blueprint: ${error}`,
            error,
            path: fullPath,
            skeleton: params.skeletonPath
          };
        }
      }

      return {
        success: false,
        message: 'Automation bridge not connected for Animation Blueprint creation',
        error: 'AUTOMATION_BRIDGE_UNAVAILABLE',
        path: fullPath,
        skeleton: params.skeletonPath
      };
    } catch (err: unknown) {
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
    } catch (err: unknown) {
      return { success: false, error: `Failed to add state machine: ${err}` };
    }
  }

  async createStateMachine(params: {
    machineName?: string;
    states?: unknown[];  // Array of state objects, normalized internally
    transitions?: unknown[];  // Array of transition objects, normalized internally
    blueprintPath?: string;
  }): Promise<
    | {
      success: true;
      message: string;
      machineName: string;
      blueprintPath?: string;
      states?: Array<{ name: string; animation?: string; isEntry?: boolean; isExit?: boolean }>;
      transitions?: Array<{ sourceState: string; targetState: string; condition?: string }>;
    }
    | { success: false; message: string; error: string }
  > {
    try {
      const rawName = typeof params.machineName === 'string' ? params.machineName.trim() : '';
      const machineName = rawName || 'StateMachine';

      const normalizedStates: Array<{ name: string; animation?: string; isEntry?: boolean; isExit?: boolean }> =
        Array.isArray(params.states)
          ? params.states
            .map((s) => {
              if (typeof s === 'string') {
                const name = s.trim();
                return name ? { name } : undefined;
              }
              if (s && typeof s === 'object') {
                const stateObj = s as Record<string, unknown>;
                if (typeof stateObj.name === 'string') {
                  const name = stateObj.name.trim();
                  if (!name) return undefined;
                  return s as { name: string; animation?: string; isEntry?: boolean; isExit?: boolean };
                }
              }
              return undefined;
            })
            .filter((s): s is { name: string; animation?: string; isEntry?: boolean; isExit?: boolean } => !!s)
          : [];

      const normalizedTransitionsRaw = Array.isArray(params.transitions)
        ? params.transitions
          .map((t) => {
            if (!t || typeof t !== 'object') return undefined;
            const tObj = t as Record<string, unknown>;
            const src = (String(tObj.sourceState ?? '') || '').trim();
            const dst = (String(tObj.targetState ?? '') || '').trim();
            if (!src || !dst) return undefined;
            return { sourceState: src, targetState: dst, condition: tObj.condition as string | undefined };
          })
          .filter((t) => !!t)
        : [];

      const normalizedTransitions = normalizedTransitionsRaw as Array<{
        sourceState: string;
        targetState: string;
        condition?: string;
      }>;

      const blueprintPath = typeof params.blueprintPath === 'string' && params.blueprintPath.trim().length > 0
        ? params.blueprintPath.trim()
        : undefined;

      // Call C++ bridge to create state machine in the Animation Blueprint
      if (this.automationBridge && typeof this.automationBridge.sendAutomationRequest === 'function') {
        try {
          const payload = cleanObject({
            subAction: 'add_state_machine',
            blueprintPath,
            stateMachineName: machineName
          });

          const resp = await this.automationBridge.sendAutomationRequest('manage_animation_authoring', payload, { timeoutMs: 60000 });
          const result = resp?.result ?? resp;
          const resultObj = result && typeof result === 'object' ? result as Record<string, unknown> : undefined;
          const isSuccess = resp && resp.success !== false && !!resultObj;

          if (isSuccess && resultObj) {
            const key = `StateMachine:${machineName}`;
            this.trackArtifact(key, {
              path: blueprintPath,
              type: 'AnimationStateMachine',
              metadata: {
                machineName,
                states: normalizedStates,
                transitions: normalizedTransitions
              }
            });

            // Add states if provided
            for (const state of normalizedStates) {
              await this.automationBridge.sendAutomationRequest('manage_animation_authoring', cleanObject({
                subAction: 'add_state',
                blueprintPath,
                stateMachineName: machineName,
                stateName: state.name
              }), { timeoutMs: 30000 });
            }

            // Add transitions if provided
            for (const transition of normalizedTransitions) {
              await this.automationBridge.sendAutomationRequest('manage_animation_authoring', cleanObject({
                subAction: 'add_transition',
                blueprintPath,
                stateMachineName: machineName,
                fromState: transition.sourceState,
                toState: transition.targetState,
                crossfadeDuration: 0.2
              }), { timeoutMs: 30000 });
            }

            return {
              success: true,
              message: resp.message || `State machine '${machineName}' created in blueprint`,
              machineName,
              blueprintPath,
              states: normalizedStates.length ? normalizedStates : undefined,
              transitions: normalizedTransitions.length ? normalizedTransitions : undefined
            };
          }

          const message = typeof resp?.message === 'string'
            ? resp.message
            : (typeof resp?.error === 'string' ? resp.error : 'State machine creation failed');
          const error = typeof resp?.error === 'string' ? resp.error : message;

          return { success: false, message, error };
        } catch (err: unknown) {
          const error = String(err);
          return {
            success: false,
            message: `Failed to create state machine: ${error}`,
            error
          };
        }
      }

      return {
        success: false,
        message: 'Automation bridge not connected for createStateMachine',
        error: 'AUTOMATION_BRIDGE_UNAVAILABLE'
      };
    } catch (err: unknown) {
      const error = String(err);
      return {
        success: false,
        message: `Failed to create state machine: ${error}`,
        error
      };
    }
  }

  async createBlendSpace(params: {
    name?: string;
    savePath?: string;
    path?: string;  // alias for savePath
    dimensions?: 1 | 2;
    skeletonPath?: string;
    horizontalAxis?: { name?: string; minValue?: number; maxValue?: number };
    verticalAxis?: { name?: string; minValue?: number; maxValue?: number };
    samples?: Array<{ animation: string; x: number; y?: number }>;
  }): Promise<
    | { success: true; message: string; path: string; skeletonPath?: string; warnings?: string[]; details?: unknown }
    | { success: false; error: string }
  > {
    try {
      const targetPath = params.savePath ?? params.path ?? '/Game/Animations';
      const validation = validateAssetParams({ name: params.name ?? 'BlendSpace', savePath: targetPath });
      if (!validation.valid) {
        return { success: false, error: validation.error ?? 'Invalid asset parameters' };
      }

      const sanitized = validation.sanitized;
      const assetName = sanitized.name;
      const assetPath = sanitized.savePath ?? targetPath;
      const dimensions = params.dimensions === 2 ? 2 : 1;

      if (this.automationBridge && typeof this.automationBridge.sendAutomationRequest === 'function') {
        try {
          const payload: Record<string, unknown> = {
            action: 'create_blend_space',
            name: assetName,
            savePath: assetPath,
            skeletonPath: params.skeletonPath,
            dimensions,
            samples: params.samples
          };

          if (params.horizontalAxis) {
            payload.minX = params.horizontalAxis.minValue;
            payload.maxX = params.horizontalAxis.maxValue;
            // gridX is not in params.horizontalAxis, maybe default or add to interface?
            // The C++ code defaults GridX to 3.0.
          }

          if (params.verticalAxis) {
            payload.minY = params.verticalAxis.minValue;
            payload.maxY = params.verticalAxis.maxValue;
          }

          const resp = await this.automationBridge.sendAutomationRequest('animation_physics', cleanObject(payload));
          const result = resp?.result ?? resp;
          const resultObj = result && typeof result === 'object' ? result as Record<string, unknown> : undefined;
          const isSuccess = resp && resp.success !== false && !!resultObj;

          if (isSuccess && resultObj) {
            const path = typeof resultObj.blendSpacePath === 'string'
              ? (resultObj.blendSpacePath as string)
              : `${assetPath}/${assetName}`;
          const warnings = Array.isArray(resultObj.warnings) ? (resultObj.warnings as string[]) : undefined;
            const details = resultObj.details;
            return {
              success: true,
              message: resp.message || `Blend Space ${assetName} created`,
              path,
              skeletonPath: params.skeletonPath,
              details,
              warnings
            };
          }

          const message = typeof resp?.message === 'string'
            ? resp.message
            : (typeof resp?.error === 'string' ? resp.error : 'Blend space creation failed');
          const error = typeof resp?.error === 'string' ? resp.error : message;

          return { success: false, error };
        } catch (err: unknown) {
          const error = String(err);
          return { success: false, error: `Failed to create blend space: ${error}` };
        }
      }

      return { success: false, error: 'Automation bridge not connected for createBlendSpace' };
    } catch (err: unknown) {
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
  }): Promise<
    | { success: true; message: string; path: string; warnings?: string[]; details?: unknown }
    | { success: false; error: string }
  > {
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

      if (this.automationBridge && typeof this.automationBridge.sendAutomationRequest === 'function') {
        try {
          const resp = await this.automationBridge.sendAutomationRequest('animation_physics', cleanObject({
            action: 'setup_ik',
            name: assetName,
            savePath: assetPath,
            skeletonPath: params.skeletonPath,
            controls: params.controls
          }), { timeoutMs: 60000 });
          const result = resp?.result ?? resp;
          const resultObj = result && typeof result === 'object' ? result as Record<string, unknown> : undefined;
          const isSuccess = resp && resp.success !== false && !!resultObj;

          if (isSuccess && resultObj) {
            const controlRigPath = typeof resultObj.controlRigPath === 'string'
              ? (resultObj.controlRigPath as string)
              : fullPath;
            const warnings = Array.isArray(resultObj.warnings) ? (resultObj.warnings as string[]) : undefined;
            const details = resultObj.details;
            this.trackArtifact(assetName, { path: controlRigPath, type: 'ControlRig' });
            return {
              success: true,
              message: resp.message || `Control Rig ${assetName} created`,
              path: controlRigPath,
              warnings,
              details
            };
          }

          const message = typeof resp?.message === 'string'
            ? resp.message
            : (typeof resp?.error === 'string' ? resp.error : 'Control Rig setup failed');
          const error = typeof resp?.error === 'string' ? resp.error : message;

          return { success: false, error };
        } catch (err: unknown) {
          const error = String(err);
          return { success: false, error: `Failed to setup control rig: ${error}` };
        }
      }

      return { success: false, error: 'Automation bridge not connected for setupControlRig' };
    } catch (err: unknown) {
      return { success: false, error: `Failed to setup control rig: ${err}` };
    }
  }

  async setupIK(params: {
    actorName?: string;
    ikBones?: unknown[];
    enableFootPlacement?: boolean;
  }): Promise<
    | {
      success: true;
      message: string;
      actorName: string;
      ikBones?: string[];
      enableFootPlacement?: boolean;
    }
    | { success: false; message: string; error: string }
  > {
    try {
      const actorName = (params.actorName || 'Character').trim();
      const ikBones = Array.isArray(params.ikBones)
        ? params.ikBones.map((b) => String(b)).filter((b) => b.trim().length > 0)
        : [];

      if (this.automationBridge && typeof this.automationBridge.sendAutomationRequest === 'function') {
        const resp = await this.automationBridge.sendAutomationRequest('animation_physics', {
          action: 'setup_ik',
          actorName,
          ikBones,
          enableFootPlacement: params.enableFootPlacement === true
        }, { timeoutMs: 60000 });

        if (resp && resp.success !== false) {
          const key = `IK:${actorName}`;
          this.trackArtifact(key, {
            type: 'IKSetup',
            metadata: {
              actorName,
              ikBones,
              enableFootPlacement: params.enableFootPlacement === true
            }
          });
          return {
            success: true,
            message: resp.message || `IK setup completed for actor "${actorName}"`,
            actorName,
            ikBones: ikBones.length ? ikBones : undefined,
            enableFootPlacement: params.enableFootPlacement === true ? true : undefined
          };
        }
        return { success: false, message: resp?.message || 'IK setup failed', error: resp?.error || 'BRIDGE_ERROR' };
      }

      return { success: false, message: 'Automation bridge not connected', error: 'AUTOMATION_BRIDGE_UNAVAILABLE' };
    } catch (err: unknown) {
      const error = String(err);
      return {
        success: false,
        message: `Failed to setup IK: ${error}`,
        error
      };
    }
  }

  async createProceduralAnim(params: {
    systemName?: string;
    baseAnimation?: string;
    modifiers?: unknown[];  // Array of modifier configs
    savePath?: string;
  }): Promise<
    | {
      success: true;
      message: string;
      path: string;
      systemName: string;
    }
    | { success: false; message: string; error: string }
  > {
    try {
      const baseName = (params.systemName || '').trim()
        || (params.baseAnimation ? params.baseAnimation.split('/').pop() || '' : '');
      const systemName = baseName || 'ProceduralSystem';
      const basePath = (params.savePath || '/Game/Animations').replace(/\/+$/, '');
      const expectedPath = `${basePath || '/Game/Animations'}/${systemName}`;

      // Route to C++ bridge for actual procedural animation creation
      if (this.automationBridge && typeof this.automationBridge.sendAutomationRequest === 'function') {
        try {
          const resp = await this.automationBridge.sendAutomationRequest('animation_physics', cleanObject({
            action: 'create_procedural_anim',
            systemName,
            baseAnimation: params.baseAnimation,
            modifiers: Array.isArray(params.modifiers) ? params.modifiers : [],
            savePath: basePath
          }), { timeoutMs: 60000 });

          const result = resp?.result ?? resp;
          const resultObj = result && typeof result === 'object' ? (result as Record<string, unknown>) : undefined;
          const isSuccess = resp && resp.success !== false && !!resultObj;

          if (isSuccess && resultObj) {
            const path = typeof resultObj.path === 'string' ? (resultObj.path as string) : expectedPath;
            this.trackArtifact(systemName, {
              path,
              type: 'ProceduralAnimation',
              metadata: {
                baseAnimation: params.baseAnimation,
                modifiers: Array.isArray(params.modifiers) ? params.modifiers : []
              }
            });

            return {
              success: true,
              message: resp.message || `Procedural animation system '${systemName}' created at ${path}`,
              path,
              systemName
            };
          }

          const message = typeof resp?.message === 'string'
            ? resp.message
            : (typeof resp?.error === 'string' ? resp.error : 'Procedural animation creation failed');
          const error = typeof resp?.error === 'string' ? resp.error : message;

          return { success: false, message, error };
        } catch (err: unknown) {
          const error = String(err);
          return {
            success: false,
            message: `Failed to create procedural animation system: ${error}`,
            error
          };
        }
      }

      return {
        success: false,
        message: 'Automation bridge not connected for createProceduralAnim',
        error: 'AUTOMATION_BRIDGE_UNAVAILABLE'
      };
    } catch (err: unknown) {
      const error = String(err);
      return {
        success: false,
        message: `Failed to create procedural animation system: ${error}`,
        error
      };
    }
  }

  async createBlendTree(params: {
    treeName?: string;
    blendType?: string;
    basePose?: string;
    additiveAnimations?: unknown[];  // Array of animation configs
    savePath?: string;
  }): Promise<
    | {
      success: true;
      message: string;
      path: string;
      treeName: string;
    }
    | { success: false; message: string; error: string }
  > {
    try {
      const rawName = (params.treeName || '').trim();
      const treeName = rawName || 'BlendTree';
      const basePath = (params.savePath || '/Game/Animations').replace(/\/+$/, '');
      const expectedPath = `${basePath || '/Game/Animations'}/${treeName}`;

      // Route to C++ bridge for actual blend tree creation
      if (this.automationBridge && typeof this.automationBridge.sendAutomationRequest === 'function') {
        try {
          const resp = await this.automationBridge.sendAutomationRequest('animation_physics', cleanObject({
            action: 'create_blend_tree',
            treeName,
            blendType: params.blendType,
            basePose: params.basePose,
            additiveAnimations: Array.isArray(params.additiveAnimations) ? params.additiveAnimations : [],
            savePath: basePath
          }), { timeoutMs: 60000 });

          const result = resp?.result ?? resp;
          const resultObj = result && typeof result === 'object' ? (result as Record<string, unknown>) : undefined;
          const isSuccess = resp && resp.success !== false && !!resultObj;

          if (isSuccess && resultObj) {
            const path = typeof resultObj.path === 'string' ? (resultObj.path as string) : expectedPath;
            this.trackArtifact(treeName, {
              path,
              type: 'BlendTree',
              metadata: {
                blendType: params.blendType,
                basePose: params.basePose,
                additiveAnimations: Array.isArray(params.additiveAnimations) ? params.additiveAnimations : []
              }
            });

            return {
              success: true,
              message: resp.message || `Blend tree '${treeName}' created at ${path}`,
              path,
              treeName
            };
          }

          const message = typeof resp?.message === 'string'
            ? resp.message
            : (typeof resp?.error === 'string' ? resp.error : 'Blend tree creation failed');
          const error = typeof resp?.error === 'string' ? resp.error : message;

          return { success: false, message, error };
        } catch (err: unknown) {
          const error = String(err);
          return {
            success: false,
            message: `Failed to create blend tree: ${error}`,
            error
          };
        }
      }

      return {
        success: false,
        message: 'Automation bridge not connected for createBlendTree',
        error: 'AUTOMATION_BRIDGE_UNAVAILABLE'
      };
    } catch (err: unknown) {
      const error = String(err);
      return {
        success: false,
        message: `Failed to create blend tree: ${error}`,
        error
      };
    }
  }

  async cleanup(artifacts?: unknown[]): Promise<
    | {
      success: boolean;
      message: string;
      removed?: string[];
      missing?: string[];
    }
    | { success: false; message: string; error: string }
  > {
    try {
      const pathsToDelete: string[] = [];

      if (Array.isArray(artifacts) && artifacts.length > 0) {
        pathsToDelete.push(...artifacts.map((a) => String(a).trim()).filter((a) => a.length > 0));
      } else {
        // If no specific artifacts provided, clear all managed ones
        for (const [key, val] of this.managedArtifacts.entries()) {
          if (val.path) pathsToDelete.push(val.path);
          else pathsToDelete.push(key);
        }
      }

      if (pathsToDelete.length === 0) {
        return {
          success: true,
          message: 'No artifacts to cleanup.'
        };
      }

      let bridgeMessage = '';
      if (this.automationBridge) {
        try {
          const response = await this.automationBridge.sendAutomationRequest('animation_physics', {
            action: 'cleanup',
            artifacts: pathsToDelete
          });

          if (!response.success) {
            bridgeMessage = ` (Engine cleanup failed: ${response.message})`;
          } else {
            bridgeMessage = ' (Engine assets deleted)';
          }
        } catch (e: unknown) {
          bridgeMessage = ` (Engine connection failed: ${e})`;
        }
      } else {
        bridgeMessage = ' (No automation bridge available)';
      }

      const removed: string[] = [];

      // Clear local registry
      const toRemoveKeys: string[] = [];
      for (const [key, val] of this.managedArtifacts.entries()) {
        if (pathsToDelete.includes(key) || (val.path && pathsToDelete.includes(val.path))) {
          toRemoveKeys.push(key);
        }
      }

      for (const key of toRemoveKeys) {
        this.managedArtifacts.delete(key);
        removed.push(key);
      }

      // Add any explicit paths that were not in managed artifacts but requested
      for (const path of pathsToDelete) {
        if (!removed.includes(path)) {
          // We don't have it locally, but we tried to delete it from engine
          removed.push(path);
        }
      }

      return {
        success: true,
        message: `Cleanup attempt processed for ${pathsToDelete.length} artifacts${bridgeMessage}`,
        removed
      };
    } catch (err: unknown) {
      const error = String(err);
      return {
        success: false,
        message: `Failed to cleanup animation artifacts: ${error}`,
        error
      };
    }
  }

  async createAnimationAsset(params: {
    name?: string;
    path?: string;
    savePath?: string;
    skeletonPath?: string;
    assetType?: string;
  }): Promise<
    | {
      success: true;
      message: string;
      path: string;
      assetType?: string;
      existingAsset?: boolean;
    }
    | { success: false; message: string; error: string }
  > {
    try {
      const targetPath = (params.path || params.savePath) ?? '/Game/Animations';
      const validation = validateAssetParams({ name: params.name ?? 'AnimAsset', savePath: targetPath });
      if (!validation.valid) {
        const message = validation.error ?? 'Invalid asset parameters';
        return { success: false, message, error: message };
      }

      const sanitized = validation.sanitized;
      const assetName = sanitized.name;
      const assetPath = sanitized.savePath ?? targetPath;
      const fullPath = `${assetPath}/${assetName}`;

      const normalizedType = (params.assetType || 'sequence').toLowerCase();

      if (this.automationBridge && typeof this.automationBridge.sendAutomationRequest === 'function') {
        try {
          const payload: Record<string, unknown> = {
            action: 'create_animation_asset',
            name: assetName,
            savePath: assetPath,
            skeletonPath: params.skeletonPath,
            assetType: normalizedType
          };

          const resp = await this.automationBridge.sendAutomationRequest('animation_physics', cleanObject(payload), { timeoutMs: 60000 });
          const result = resp?.result ?? resp;
          const resultObj = result && typeof result === 'object' ? (result as Record<string, unknown>) : undefined;
          const isSuccess = resp && resp.success !== false && !!resultObj;

          if (isSuccess && resultObj) {
            const assetPathResult = typeof resultObj.assetPath === 'string' ? (resultObj.assetPath as string) : fullPath;
            const assetTypeResult = typeof resultObj.assetType === 'string' ? (resultObj.assetType as string) : undefined;
            const existingAsset = typeof resultObj.existingAsset === 'boolean' ? Boolean(resultObj.existingAsset) : false;

            this.trackArtifact(assetName, { path: assetPathResult, type: 'AnimationAsset' });

            return {
              success: true,
              message: resp.message || `Animation asset created at ${assetPathResult}`,
              path: assetPathResult,
              assetType: assetTypeResult,
              existingAsset
            };
          }

          const message = typeof resp?.message === 'string'
            ? resp.message
            : (typeof resp?.error === 'string' ? resp.error : 'Animation asset creation failed');
          const error = typeof resp?.error === 'string' ? resp.error : message;

          return { success: false, message, error };
        } catch (err: unknown) {
          const error = String(err);
          return {
            success: false,
            message: `Failed to create animation asset: ${error}`,
            error
          };
        }
      }

      return {
        success: false,
        message: 'Automation bridge not connected for createAnimationAsset',
        error: 'AUTOMATION_BRIDGE_UNAVAILABLE'
      };
    } catch (err: unknown) {
      const error = String(err);
      return {
        success: false,
        message: `Failed to create animation asset: ${error}`,
        error
      };
    }
  }

  async addNotify(params: {
    animationPath?: string;
    assetPath?: string;
    notifyName?: string;
    time?: number;
  }): Promise<
    | {
      success: true;
      message: string;
      assetPath: string;
      notifyName: string;
      time: number;
    }
    | { success: false; message: string; error: string }
  > {
    try {
      const rawPath = (params.animationPath || params.assetPath || '').trim();
      if (!rawPath) {
        const error = 'animationPath or assetPath is required for addNotify';
        return { success: false, message: error, error };
      }

      const notifyName = (params.notifyName || 'Notify').trim();
      const time = typeof params.time === 'number' && params.time >= 0 ? params.time : 0;

      if (this.automationBridge && typeof this.automationBridge.sendAutomationRequest === 'function') {
        try {
          const resp = await this.automationBridge.sendAutomationRequest(
            'animation_physics',
            cleanObject({
              action: 'add_notify',
              assetPath: rawPath,
              notifyName,
              time
            }),
            { timeoutMs: 60000 }
          );

          const result = resp?.result ?? resp;
          const resultObj = result && typeof result === 'object' ? (result as Record<string, unknown>) : undefined;

          if (resp && resp.success !== false && resultObj) {
            return {
              success: true,
              message: resp.message || `Notify '${notifyName}' added to ${rawPath} at time ${time}`,
              assetPath: typeof resultObj.assetPath === 'string' ? (resultObj.assetPath as string) : rawPath,
              notifyName,
              time
            };
          }
        } catch (err: unknown) {
          const error = String(err);
          return {
            success: false,
            message: `Failed to add notify: ${error}`,
            error
          };
        }
      }

      return {
        success: false,
        message: 'Automation bridge not connected for addNotify',
        error: 'AUTOMATION_BRIDGE_UNAVAILABLE'
      };
    } catch (err: unknown) {
      const error = String(err);
      return {
        success: false,
        message: `Failed to add notify: ${error}`,
        error
      };
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
    } catch (err: unknown) {
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
    } catch (err: unknown) {
      const error = `Failed to play animation: ${err}`;
      return { success: false, message: error, error: String(err) };
    }
  }

}
