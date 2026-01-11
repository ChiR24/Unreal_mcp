/**
 * Animation Authoring Handlers for Phase 10
 *
 * Provides complete animation system authoring from keyframes to state machines.
 * Includes Animation Sequences, Montages, Blend Spaces, Animation Blueprints,
 * Control Rig, and Retargeting.
 */

import { ITools } from '../../types/tool-interfaces.js';
import type { HandlerArgs, HandlerResult } from '../../types/handler-types.js';
import { executeAutomationRequest } from './common-handlers.js';
import {
  normalizeArgs,
  extractString,
  extractOptionalString,
  extractOptionalNumber,
  extractOptionalBoolean,
} from './argument-helper.js';
import { ResponseFactory } from '../../utils/response-factory.js';

/** Helper to extract optional array from params */
function extractOptionalArray(params: Record<string, unknown>, key: string): unknown[] | undefined {
  const val = params[key];
  if (val === undefined || val === null) return undefined;
  if (Array.isArray(val)) return val;
  return undefined;
}

/** Helper to extract optional object from params */
function extractOptionalObject(params: Record<string, unknown>, key: string): Record<string, unknown> | undefined {
  const val = params[key];
  if (val === undefined || val === null) return undefined;
  if (typeof val === 'object' && !Array.isArray(val)) return val as HandlerResult;
  return undefined;
}

/** Animation authoring response */
interface AnimationAuthoringResponse {
  success?: boolean;
  message?: string;
  error?: string;
  errorCode?: string;
  result?: Record<string, unknown>;
  assetPath?: string;
  [key: string]: unknown;
}

/**
 * Handle animation authoring actions
 */
export async function handleAnimationAuthoringTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<HandlerResult> {
  try {
    switch (action) {
      // ===== 10.1 Animation Sequences =====
      case 'create_animation_sequence': {
        const params = normalizeArgs(args, [
          { key: 'name', required: true },
          { key: 'path', aliases: ['directory'], default: '/Game/Animations' },
          { key: 'skeletonPath', required: true },
          { key: 'numFrames', default: 30 },
          { key: 'frameRate', default: 30 },
          { key: 'save', default: true },
        ]);

        const name = extractString(params, 'name');
        const path = extractOptionalString(params, 'path') ?? '/Game/Animations';
        const skeletonPath = extractString(params, 'skeletonPath');
        const numFrames = extractOptionalNumber(params, 'numFrames') ?? 30;
        const frameRate = extractOptionalNumber(params, 'frameRate') ?? 30;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'create_animation_sequence',
          name,
          path,
          skeletonPath,
          numFrames,
          frameRate,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create animation sequence', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Animation sequence '${name}' created`);
      }

      case 'set_sequence_length': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'numFrames', required: true },
          { key: 'frameRate' },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const numFrames = extractOptionalNumber(params, 'numFrames') ?? 30;
        const frameRate = extractOptionalNumber(params, 'frameRate');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'set_sequence_length',
          assetPath,
          numFrames,
          frameRate,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set sequence length', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Sequence length updated');
      }

      case 'add_bone_track': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'boneName', required: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const boneName = extractString(params, 'boneName');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'add_bone_track',
          assetPath,
          boneName,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add bone track', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Bone track '${boneName}' added`);
      }

      case 'set_bone_key': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'boneName', required: true },
          { key: 'frame', required: true },
          { key: 'location' }, // {x, y, z}
          { key: 'rotation' }, // {pitch, yaw, roll} or {x, y, z, w}
          { key: 'scale' },    // {x, y, z}
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const boneName = extractString(params, 'boneName');
        const frame = extractOptionalNumber(params, 'frame') ?? 0;
        const location = extractOptionalObject(params, 'location');
        const rotation = extractOptionalObject(params, 'rotation');
        const scale = extractOptionalObject(params, 'scale');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'set_bone_key',
          assetPath,
          boneName,
          frame,
          location,
          rotation,
          scale,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set bone key', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Bone key set at frame ${frame}`);
      }

      case 'set_curve_key': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'curveName', required: true },
          { key: 'frame', required: true },
          { key: 'value', required: true },
          { key: 'createIfMissing', default: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const curveName = extractString(params, 'curveName');
        const frame = extractOptionalNumber(params, 'frame') ?? 0;
        const value = extractOptionalNumber(params, 'value') ?? 0;
        const createIfMissing = extractOptionalBoolean(params, 'createIfMissing') ?? true;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'set_curve_key',
          assetPath,
          curveName,
          frame,
          value,
          createIfMissing,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set curve key', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Curve key set at frame ${frame}`);
      }

      case 'add_notify': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'notifyClass', required: true },
          { key: 'frame', required: true },
          { key: 'trackIndex', default: 0 },
          { key: 'notifyName' },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const notifyClass = extractString(params, 'notifyClass');
        const frame = extractOptionalNumber(params, 'frame') ?? 0;
        const trackIndex = extractOptionalNumber(params, 'trackIndex') ?? 0;
        const notifyName = extractOptionalString(params, 'notifyName');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'add_notify',
          assetPath,
          notifyClass,
          frame,
          trackIndex,
          notifyName,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add notify', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Notify added');
      }

      case 'add_notify_state': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'notifyClass', required: true },
          { key: 'startFrame', required: true },
          { key: 'endFrame', required: true },
          { key: 'trackIndex', default: 0 },
          { key: 'notifyName' },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const notifyClass = extractString(params, 'notifyClass');
        const startFrame = extractOptionalNumber(params, 'startFrame') ?? 0;
        const endFrame = extractOptionalNumber(params, 'endFrame') ?? 10;
        const trackIndex = extractOptionalNumber(params, 'trackIndex') ?? 0;
        const notifyName = extractOptionalString(params, 'notifyName');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'add_notify_state',
          assetPath,
          notifyClass,
          startFrame,
          endFrame,
          trackIndex,
          notifyName,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add notify state', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Notify state added');
      }

      case 'add_sync_marker': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'markerName', required: true },
          { key: 'frame', required: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const markerName = extractString(params, 'markerName');
        const frame = extractOptionalNumber(params, 'frame') ?? 0;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'add_sync_marker',
          assetPath,
          markerName,
          frame,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add sync marker', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Sync marker '${markerName}' added`);
      }

      case 'set_root_motion_settings': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'enableRootMotion', default: true },
          { key: 'rootMotionRootLock', default: 'RefPose' },
          { key: 'forceRootLock', default: false },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const enableRootMotion = extractOptionalBoolean(params, 'enableRootMotion') ?? true;
        const rootMotionRootLock = extractOptionalString(params, 'rootMotionRootLock') ?? 'RefPose';
        const forceRootLock = extractOptionalBoolean(params, 'forceRootLock') ?? false;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'set_root_motion_settings',
          assetPath,
          enableRootMotion,
          rootMotionRootLock,
          forceRootLock,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set root motion settings', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Root motion settings updated');
      }

      case 'set_additive_settings': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'additiveAnimType', default: 'NoAdditive' }, // NoAdditive, LocalSpaceAdditive, MeshSpaceAdditive
          { key: 'basePoseType', default: 'RefPose' }, // RefPose, AnimationFrame, AnimationScaled
          { key: 'basePoseAnimation' },
          { key: 'basePoseFrame', default: 0 },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const additiveAnimType = extractOptionalString(params, 'additiveAnimType') ?? 'NoAdditive';
        const basePoseType = extractOptionalString(params, 'basePoseType') ?? 'RefPose';
        const basePoseAnimation = extractOptionalString(params, 'basePoseAnimation');
        const basePoseFrame = extractOptionalNumber(params, 'basePoseFrame') ?? 0;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'set_additive_settings',
          assetPath,
          additiveAnimType,
          basePoseType,
          basePoseAnimation,
          basePoseFrame,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set additive settings', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Additive settings updated');
      }

      // ===== 10.2 Animation Montages =====
      case 'create_montage': {
        const params = normalizeArgs(args, [
          { key: 'name', required: true },
          { key: 'path', aliases: ['directory'], default: '/Game/Animations' },
          { key: 'skeletonPath', required: true },
          { key: 'slotName', default: 'DefaultSlot' },
          { key: 'save', default: true },
        ]);

        const name = extractString(params, 'name');
        const path = extractOptionalString(params, 'path') ?? '/Game/Animations';
        const skeletonPath = extractString(params, 'skeletonPath');
        const slotName = extractOptionalString(params, 'slotName') ?? 'DefaultSlot';
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'create_montage',
          name,
          path,
          skeletonPath,
          slotName,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create montage', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Montage '${name}' created`);
      }

      case 'add_montage_section': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'sectionName', required: true },
          { key: 'startTime', required: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const sectionName = extractString(params, 'sectionName');
        const startTime = extractOptionalNumber(params, 'startTime') ?? 0;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'add_montage_section',
          assetPath,
          sectionName,
          startTime,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add montage section', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Section '${sectionName}' added`);
      }

      case 'add_montage_slot': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'animationPath', required: true },
          { key: 'slotName', default: 'DefaultSlot' },
          { key: 'startTime', default: 0 },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const animationPath = extractString(params, 'animationPath');
        const slotName = extractOptionalString(params, 'slotName') ?? 'DefaultSlot';
        const startTime = extractOptionalNumber(params, 'startTime') ?? 0;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'add_montage_slot',
          assetPath,
          animationPath,
          slotName,
          startTime,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add montage slot', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Animation added to montage slot');
      }

      case 'set_section_timing': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'sectionName', required: true },
          { key: 'startTime' },
          { key: 'length' },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const sectionName = extractString(params, 'sectionName');
        const startTime = extractOptionalNumber(params, 'startTime');
        const length = extractOptionalNumber(params, 'length');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'set_section_timing',
          assetPath,
          sectionName,
          startTime,
          length,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set section timing', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Section timing updated');
      }

      case 'add_montage_notify': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'notifyClass', required: true },
          { key: 'time', required: true },
          { key: 'trackIndex', default: 0 },
          { key: 'notifyName' },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const notifyClass = extractString(params, 'notifyClass');
        const time = extractOptionalNumber(params, 'time') ?? 0;
        const trackIndex = extractOptionalNumber(params, 'trackIndex') ?? 0;
        const notifyName = extractOptionalString(params, 'notifyName');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'add_montage_notify',
          assetPath,
          notifyClass,
          time,
          trackIndex,
          notifyName,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add montage notify', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Montage notify added');
      }

      case 'set_blend_in': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'blendTime', default: 0.25 },
          { key: 'blendOption', default: 'Linear' },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const blendTime = extractOptionalNumber(params, 'blendTime') ?? 0.25;
        const blendOption = extractOptionalString(params, 'blendOption') ?? 'Linear';
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'set_blend_in',
          assetPath,
          blendTime,
          blendOption,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set blend in', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Blend in settings updated');
      }

      case 'set_blend_out': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'blendTime', default: 0.25 },
          { key: 'blendOption', default: 'Linear' },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const blendTime = extractOptionalNumber(params, 'blendTime') ?? 0.25;
        const blendOption = extractOptionalString(params, 'blendOption') ?? 'Linear';
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'set_blend_out',
          assetPath,
          blendTime,
          blendOption,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set blend out', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Blend out settings updated');
      }

      case 'link_sections': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'fromSection', required: true },
          { key: 'toSection', required: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const fromSection = extractString(params, 'fromSection');
        const toSection = extractString(params, 'toSection');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'link_sections',
          assetPath,
          fromSection,
          toSection,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to link sections', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Linked '${fromSection}' to '${toSection}'`);
      }

      // ===== 10.3 Blend Spaces =====
      case 'create_blend_space_1d': {
        const params = normalizeArgs(args, [
          { key: 'name', required: true },
          { key: 'path', aliases: ['directory'], default: '/Game/Animations' },
          { key: 'skeletonPath', required: true },
          { key: 'axisName', default: 'Speed' },
          { key: 'axisMin', default: 0 },
          { key: 'axisMax', default: 600 },
          { key: 'save', default: true },
        ]);

        const name = extractString(params, 'name');
        const path = extractOptionalString(params, 'path') ?? '/Game/Animations';
        const skeletonPath = extractString(params, 'skeletonPath');
        const axisName = extractOptionalString(params, 'axisName') ?? 'Speed';
        const axisMin = extractOptionalNumber(params, 'axisMin') ?? 0;
        const axisMax = extractOptionalNumber(params, 'axisMax') ?? 600;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'create_blend_space_1d',
          name,
          path,
          skeletonPath,
          axisName,
          axisMin,
          axisMax,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create blend space 1D', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Blend Space 1D '${name}' created`);
      }

      case 'create_blend_space_2d': {
        const params = normalizeArgs(args, [
          { key: 'name', required: true },
          { key: 'path', aliases: ['directory'], default: '/Game/Animations' },
          { key: 'skeletonPath', required: true },
          { key: 'horizontalAxisName', default: 'Direction' },
          { key: 'horizontalMin', default: -180 },
          { key: 'horizontalMax', default: 180 },
          { key: 'verticalAxisName', default: 'Speed' },
          { key: 'verticalMin', default: 0 },
          { key: 'verticalMax', default: 600 },
          { key: 'save', default: true },
        ]);

        const name = extractString(params, 'name');
        const path = extractOptionalString(params, 'path') ?? '/Game/Animations';
        const skeletonPath = extractString(params, 'skeletonPath');
        const horizontalAxisName = extractOptionalString(params, 'horizontalAxisName') ?? 'Direction';
        const horizontalMin = extractOptionalNumber(params, 'horizontalMin') ?? -180;
        const horizontalMax = extractOptionalNumber(params, 'horizontalMax') ?? 180;
        const verticalAxisName = extractOptionalString(params, 'verticalAxisName') ?? 'Speed';
        const verticalMin = extractOptionalNumber(params, 'verticalMin') ?? 0;
        const verticalMax = extractOptionalNumber(params, 'verticalMax') ?? 600;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'create_blend_space_2d',
          name,
          path,
          skeletonPath,
          horizontalAxisName,
          horizontalMin,
          horizontalMax,
          verticalAxisName,
          verticalMin,
          verticalMax,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create blend space 2D', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Blend Space 2D '${name}' created`);
      }

      case 'add_blend_sample': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'animationPath', required: true },
          { key: 'sampleValue', required: true }, // For 1D: number, for 2D: {x, y}
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const animationPath = extractString(params, 'animationPath');
        const sampleValue = params['sampleValue'];
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'add_blend_sample',
          assetPath,
          animationPath,
          sampleValue,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add blend sample', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Blend sample added');
      }

      case 'set_axis_settings': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'axis', required: true }, // 'Horizontal', 'Vertical', or 'X' for 1D
          { key: 'axisName' },
          { key: 'minValue' },
          { key: 'maxValue' },
          { key: 'gridDivisions' },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const axis = extractString(params, 'axis');
        const axisName = extractOptionalString(params, 'axisName');
        const minValue = extractOptionalNumber(params, 'minValue');
        const maxValue = extractOptionalNumber(params, 'maxValue');
        const gridDivisions = extractOptionalNumber(params, 'gridDivisions');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'set_axis_settings',
          assetPath,
          axis,
          axisName,
          minValue,
          maxValue,
          gridDivisions,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set axis settings', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Axis settings updated');
      }

      case 'set_interpolation_settings': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'interpolationType', default: 'Lerp' }, // Lerp, Cubic
          { key: 'targetWeightInterpolationSpeed', default: 5.0 },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const interpolationType = extractOptionalString(params, 'interpolationType') ?? 'Lerp';
        const targetWeightInterpolationSpeed = extractOptionalNumber(params, 'targetWeightInterpolationSpeed') ?? 5.0;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'set_interpolation_settings',
          assetPath,
          interpolationType,
          targetWeightInterpolationSpeed,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set interpolation settings', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Interpolation settings updated');
      }

      case 'create_aim_offset': {
        const params = normalizeArgs(args, [
          { key: 'name', required: true },
          { key: 'path', aliases: ['directory'], default: '/Game/Animations' },
          { key: 'skeletonPath', required: true },
          { key: 'save', default: true },
        ]);

        const name = extractString(params, 'name');
        const path = extractOptionalString(params, 'path') ?? '/Game/Animations';
        const skeletonPath = extractString(params, 'skeletonPath');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'create_aim_offset',
          name,
          path,
          skeletonPath,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create aim offset', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Aim Offset '${name}' created`);
      }

      case 'add_aim_offset_sample': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'animationPath', required: true },
          { key: 'yaw', required: true },
          { key: 'pitch', required: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const animationPath = extractString(params, 'animationPath');
        const yaw = extractOptionalNumber(params, 'yaw') ?? 0;
        const pitch = extractOptionalNumber(params, 'pitch') ?? 0;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'add_aim_offset_sample',
          assetPath,
          animationPath,
          yaw,
          pitch,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add aim offset sample', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Aim offset sample added');
      }

      // ===== 10.4 Animation Blueprints =====
      case 'create_anim_blueprint': {
        const params = normalizeArgs(args, [
          { key: 'name', required: true },
          { key: 'path', aliases: ['directory'], default: '/Game/Blueprints' },
          { key: 'skeletonPath', required: true },
          { key: 'parentClass', default: 'AnimInstance' },
          { key: 'save', default: true },
        ]);

        const name = extractString(params, 'name');
        const path = extractOptionalString(params, 'path') ?? '/Game/Blueprints';
        const skeletonPath = extractString(params, 'skeletonPath');
        const parentClass = extractOptionalString(params, 'parentClass') ?? 'AnimInstance';
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'create_anim_blueprint',
          name,
          path,
          skeletonPath,
          parentClass,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create anim blueprint', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Animation Blueprint '${name}' created`);
      }

      case 'add_state_machine': {
        const params = normalizeArgs(args, [
          { key: 'blueprintPath', required: true },
          { key: 'stateMachineName', required: true },
          { key: 'save', default: true },
        ]);

        const blueprintPath = extractString(params, 'blueprintPath');
        const stateMachineName = extractString(params, 'stateMachineName');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'add_state_machine',
          blueprintPath,
          stateMachineName,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add state machine', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `State machine '${stateMachineName}' added`);
      }

      case 'add_state': {
        const params = normalizeArgs(args, [
          { key: 'blueprintPath', required: true },
          { key: 'stateMachineName', required: true },
          { key: 'stateName', required: true },
          { key: 'animationPath' },
          { key: 'isEntryState', default: false },
          { key: 'save', default: true },
        ]);

        const blueprintPath = extractString(params, 'blueprintPath');
        const stateMachineName = extractString(params, 'stateMachineName');
        const stateName = extractString(params, 'stateName');
        const animationPath = extractOptionalString(params, 'animationPath');
        const isEntryState = extractOptionalBoolean(params, 'isEntryState') ?? false;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'add_state',
          blueprintPath,
          stateMachineName,
          stateName,
          animationPath,
          isEntryState,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add state', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `State '${stateName}' added`);
      }

      case 'add_transition': {
        const params = normalizeArgs(args, [
          { key: 'blueprintPath', required: true },
          { key: 'stateMachineName', required: true },
          { key: 'fromState', required: true },
          { key: 'toState', required: true },
          { key: 'save', default: true },
        ]);

        const blueprintPath = extractString(params, 'blueprintPath');
        const stateMachineName = extractString(params, 'stateMachineName');
        const fromState = extractString(params, 'fromState');
        const toState = extractString(params, 'toState');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'add_transition',
          blueprintPath,
          stateMachineName,
          fromState,
          toState,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add transition', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Transition from '${fromState}' to '${toState}' added`);
      }

      case 'set_transition_rules': {
        const params = normalizeArgs(args, [
          { key: 'blueprintPath', required: true },
          { key: 'stateMachineName', required: true },
          { key: 'fromState', required: true },
          { key: 'toState', required: true },
          { key: 'blendTime', default: 0.2 },
          { key: 'blendLogicType', default: 'StandardBlend' },
          { key: 'automaticTriggerRule' }, // e.g., 'TimeRemaining'
          { key: 'automaticTriggerTime' },
          { key: 'save', default: true },
        ]);

        const blueprintPath = extractString(params, 'blueprintPath');
        const stateMachineName = extractString(params, 'stateMachineName');
        const fromState = extractString(params, 'fromState');
        const toState = extractString(params, 'toState');
        const blendTime = extractOptionalNumber(params, 'blendTime') ?? 0.2;
        const blendLogicType = extractOptionalString(params, 'blendLogicType') ?? 'StandardBlend';
        const automaticTriggerRule = extractOptionalString(params, 'automaticTriggerRule');
        const automaticTriggerTime = extractOptionalNumber(params, 'automaticTriggerTime');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'set_transition_rules',
          blueprintPath,
          stateMachineName,
          fromState,
          toState,
          blendTime,
          blendLogicType,
          automaticTriggerRule,
          automaticTriggerTime,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set transition rules', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Transition rules updated');
      }

      case 'add_blend_node': {
        const params = normalizeArgs(args, [
          { key: 'blueprintPath', required: true },
          { key: 'blendType', required: true }, // TwoWayBlend, BlendByBool, BlendPosesByBool, etc.
          { key: 'nodeName' },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
          { key: 'save', default: true },
        ]);

        const blueprintPath = extractString(params, 'blueprintPath');
        const blendType = extractString(params, 'blendType');
        const nodeName = extractOptionalString(params, 'nodeName');
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'add_blend_node',
          blueprintPath,
          blendType,
          nodeName,
          x,
          y,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add blend node', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Blend node added');
      }

      case 'add_cached_pose': {
        const params = normalizeArgs(args, [
          { key: 'blueprintPath', required: true },
          { key: 'cacheName', required: true },
          { key: 'save', default: true },
        ]);

        const blueprintPath = extractString(params, 'blueprintPath');
        const cacheName = extractString(params, 'cacheName');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'add_cached_pose',
          blueprintPath,
          cacheName,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add cached pose', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Cached pose '${cacheName}' added`);
      }

      case 'add_slot_node': {
        const params = normalizeArgs(args, [
          { key: 'blueprintPath', required: true },
          { key: 'slotName', required: true },
          { key: 'save', default: true },
        ]);

        const blueprintPath = extractString(params, 'blueprintPath');
        const slotName = extractString(params, 'slotName');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'add_slot_node',
          blueprintPath,
          slotName,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add slot node', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Slot node '${slotName}' added`);
      }

      case 'add_layered_blend_per_bone': {
        const params = normalizeArgs(args, [
          { key: 'blueprintPath', required: true },
          { key: 'layerSetup' }, // Array of {branchFilters: [{boneName, blendDepth}]}
          { key: 'save', default: true },
        ]);

        const blueprintPath = extractString(params, 'blueprintPath');
        const layerSetup = extractOptionalArray(params, 'layerSetup');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'add_layered_blend_per_bone',
          blueprintPath,
          layerSetup,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add layered blend per bone', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Layered blend per bone added');
      }

      case 'set_anim_graph_node_value': {
        const params = normalizeArgs(args, [
          { key: 'blueprintPath', required: true },
          { key: 'nodeName', required: true },
          { key: 'propertyName', required: true },
          { key: 'value', required: true },
          { key: 'save', default: true },
        ]);

        const blueprintPath = extractString(params, 'blueprintPath');
        const nodeName = extractString(params, 'nodeName');
        const propertyName = extractString(params, 'propertyName');
        const value = params['value'];
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'set_anim_graph_node_value',
          blueprintPath,
          nodeName,
          propertyName,
          value,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set anim graph node value', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Anim graph node value set');
      }

      // ===== 10.5 Control Rig =====
      case 'create_control_rig': {
        const params = normalizeArgs(args, [
          { key: 'name', required: true },
          { key: 'path', aliases: ['directory'], default: '/Game/ControlRigs' },
          { key: 'skeletalMeshPath', required: true },
          { key: 'save', default: true },
        ]);

        const name = extractString(params, 'name');
        const path = extractOptionalString(params, 'path') ?? '/Game/ControlRigs';
        const skeletalMeshPath = extractString(params, 'skeletalMeshPath');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'create_control_rig',
          name,
          path,
          skeletalMeshPath,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create control rig', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Control Rig '${name}' created`);
      }

      case 'add_control': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'controlName', required: true },
          { key: 'controlType', default: 'Transform' }, // Transform, Bool, Float, Integer, Vector2D
          { key: 'parentBone' },
          { key: 'parentControl' },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const controlName = extractString(params, 'controlName');
        const controlType = extractOptionalString(params, 'controlType') ?? 'Transform';
        const parentBone = extractOptionalString(params, 'parentBone');
        const parentControl = extractOptionalString(params, 'parentControl');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'add_control',
          assetPath,
          controlName,
          controlType,
          parentBone,
          parentControl,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add control', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Control '${controlName}' added`);
      }

      case 'add_rig_unit': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'unitType', required: true }, // FKIK, Aim, BasicIK, TwoBoneIK, FABRIK, etc.
          { key: 'unitName' },
          { key: 'settings' }, // Unit-specific settings
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const unitType = extractString(params, 'unitType');
        const unitName = extractOptionalString(params, 'unitName');
        const settings = extractOptionalObject(params, 'settings');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'add_rig_unit',
          assetPath,
          unitType,
          unitName,
          settings,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add rig unit', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Rig unit '${unitType}' added`);
      }

      case 'connect_rig_elements': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'sourceElement', required: true },
          { key: 'sourcePin', required: true },
          { key: 'targetElement', required: true },
          { key: 'targetPin', required: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const sourceElement = extractString(params, 'sourceElement');
        const sourcePin = extractString(params, 'sourcePin');
        const targetElement = extractString(params, 'targetElement');
        const targetPin = extractString(params, 'targetPin');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'connect_rig_elements',
          assetPath,
          sourceElement,
          sourcePin,
          targetElement,
          targetPin,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to connect rig elements', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Rig elements connected');
      }

      case 'create_pose_library': {
        const params = normalizeArgs(args, [
          { key: 'name', required: true },
          { key: 'path', aliases: ['directory'], default: '/Game/Animations' },
          { key: 'skeletonPath', required: true },
          { key: 'save', default: true },
        ]);

        const name = extractString(params, 'name');
        const path = extractOptionalString(params, 'path') ?? '/Game/Animations';
        const skeletonPath = extractString(params, 'skeletonPath');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'create_pose_library',
          name,
          path,
          skeletonPath,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create pose library', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Pose library '${name}' created`);
      }

      // ===== 10.6 Retargeting =====
      case 'create_ik_rig': {
        const params = normalizeArgs(args, [
          { key: 'name', required: true },
          { key: 'path', aliases: ['directory'], default: '/Game/Retargeting' },
          { key: 'skeletalMeshPath', required: true },
          { key: 'save', default: true },
        ]);

        const name = extractString(params, 'name');
        const path = extractOptionalString(params, 'path') ?? '/Game/Retargeting';
        const skeletalMeshPath = extractString(params, 'skeletalMeshPath');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'create_ik_rig',
          name,
          path,
          skeletalMeshPath,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create IK rig', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `IK Rig '${name}' created`);
      }

      case 'add_ik_chain': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'chainName', required: true },
          { key: 'startBone', required: true },
          { key: 'endBone', required: true },
          { key: 'goal' },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const chainName = extractString(params, 'chainName');
        const startBone = extractString(params, 'startBone');
        const endBone = extractString(params, 'endBone');
        const goal = extractOptionalString(params, 'goal');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'add_ik_chain',
          assetPath,
          chainName,
          startBone,
          endBone,
          goal,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add IK chain', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `IK chain '${chainName}' added`);
      }

      case 'create_ik_retargeter': {
        const params = normalizeArgs(args, [
          { key: 'name', required: true },
          { key: 'path', aliases: ['directory'], default: '/Game/Retargeting' },
          { key: 'sourceIKRigPath', required: true },
          { key: 'targetIKRigPath', required: true },
          { key: 'save', default: true },
        ]);

        const name = extractString(params, 'name');
        const path = extractOptionalString(params, 'path') ?? '/Game/Retargeting';
        const sourceIKRigPath = extractString(params, 'sourceIKRigPath');
        const targetIKRigPath = extractString(params, 'targetIKRigPath');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'create_ik_retargeter',
          name,
          path,
          sourceIKRigPath,
          targetIKRigPath,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create IK retargeter', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `IK Retargeter '${name}' created`);
      }

      case 'set_retarget_chain_mapping': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
          { key: 'sourceChain', required: true },
          { key: 'targetChain', required: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const sourceChain = extractString(params, 'sourceChain');
        const targetChain = extractString(params, 'targetChain');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'set_retarget_chain_mapping',
          assetPath,
          sourceChain,
          targetChain,
          save,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set retarget chain mapping', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Chain mapping '${sourceChain}' -> '${targetChain}' set`);
      }

      // ===== Utility =====
      case 'get_animation_info': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', required: true },
        ]);

        const assetPath = extractString(params, 'assetPath');

        const res = (await executeAutomationRequest(tools, 'manage_animation_authoring', {
          subAction: 'get_animation_info',
          assetPath,
        })) as AnimationAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to get animation info', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Animation info retrieved');
      }

      default:
        return ResponseFactory.error(`Unknown animation authoring action: ${action}`, 'UNKNOWN_ACTION');
    }
  } catch (error: unknown) {
    const err = error instanceof Error ? error : new Error(String(error));
    return ResponseFactory.error(`Animation authoring operation failed: ${err.message}`, 'ANIMATION_AUTHORING_ERROR');
  }
}
