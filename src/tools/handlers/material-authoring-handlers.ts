/**
 * Material Authoring Handlers for Phase 8
 *
 * Provides comprehensive material creation and shader authoring capabilities.
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

/** Helper to extract optional object from params */
function extractOptionalObject(params: Record<string, unknown>, key: string): Record<string, unknown> | undefined {
  const val = params[key];
  if (val === undefined || val === null) return undefined;
  if (typeof val === 'object' && !Array.isArray(val)) return val as HandlerResult;
  return undefined;
}
import { ResponseFactory } from '../../utils/response-factory.js';

/** Material authoring response */
interface MaterialAuthoringResponse {
  success?: boolean;
  message?: string;
  error?: string;
  errorCode?: string;
  result?: Record<string, unknown>;
  assetPath?: string;
  nodeId?: string;
  [key: string]: unknown;
}

/**
 * Handle material authoring actions
 */
export async function handleMaterialAuthoringTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<HandlerResult> {
  try {
    switch (action) {
      // ===== 8.1 Material Creation =====
      case 'create_material': {
        const params = normalizeArgs(args, [
          { key: 'name', required: true },
          { key: 'path', aliases: ['directory'], default: '/Game/Materials' },
          { key: 'materialDomain', aliases: ['domain'], default: 'Surface' },
          { key: 'blendMode', default: 'Opaque' },
          { key: 'shadingModel', default: 'DefaultLit' },
          { key: 'twoSided', default: false },
          { key: 'save', default: true },
        ]);

        const name = extractString(params, 'name');
        const path = extractOptionalString(params, 'path') ?? '/Game/Materials';
        const materialDomain = extractOptionalString(params, 'materialDomain') ?? 'Surface';
        const blendMode = extractOptionalString(params, 'blendMode') ?? 'Opaque';
        const shadingModel = extractOptionalString(params, 'shadingModel') ?? 'DefaultLit';
        const twoSided = extractOptionalBoolean(params, 'twoSided') ?? false;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'create_material',
          name,
          path,
          materialDomain,
          blendMode,
          shadingModel,
          twoSided,
          save,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create material', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Material '${name}' created`);
      }

      case 'set_blend_mode': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'blendMode', required: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const blendMode = extractString(params, 'blendMode');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'set_blend_mode',
          assetPath,
          blendMode,
          save,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set blend mode', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Blend mode set to ${blendMode}`);
      }

      case 'set_shading_model': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'shadingModel', required: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const shadingModel = extractString(params, 'shadingModel');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'set_shading_model',
          assetPath,
          shadingModel,
          save,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set shading model', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Shading model set to ${shadingModel}`);
      }

      case 'set_material_domain': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'domain', aliases: ['materialDomain'], required: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const domain = extractString(params, 'domain');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'set_material_domain',
          assetPath,
          materialDomain: domain,
          save,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set material domain', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Material domain set to ${domain}`);
      }

      // ===== 8.2 Material Expressions =====
      case 'add_texture_sample': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'texturePath', required: true },
          { key: 'parameterName', aliases: ['name'] },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
          { key: 'samplerType', default: 'Color' },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const texturePath = extractString(params, 'texturePath');
        const parameterName = extractOptionalString(params, 'parameterName');
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;
        const samplerType = extractOptionalString(params, 'samplerType') ?? 'Color';

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'add_texture_sample',
          assetPath,
          texturePath,
          parameterName,
          x,
          y,
          samplerType,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add texture sample', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Texture sample added');
      }

      case 'add_texture_coordinate': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'coordinateIndex', default: 0 },
          { key: 'uTiling', default: 1.0 },
          { key: 'vTiling', default: 1.0 },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const coordinateIndex = extractOptionalNumber(params, 'coordinateIndex') ?? 0;
        const uTiling = extractOptionalNumber(params, 'uTiling') ?? 1.0;
        const vTiling = extractOptionalNumber(params, 'vTiling') ?? 1.0;
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'add_texture_coordinate',
          assetPath,
          coordinateIndex,
          uTiling,
          vTiling,
          x,
          y,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add texture coordinate', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Texture coordinate added');
      }

      case 'add_scalar_parameter': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'parameterName', aliases: ['name'], required: true },
          { key: 'defaultValue', default: 0.0 },
          { key: 'group', default: 'None' },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const parameterName = extractString(params, 'parameterName');
        const defaultValue = extractOptionalNumber(params, 'defaultValue') ?? 0.0;
        const group = extractOptionalString(params, 'group') ?? 'None';
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'add_scalar_parameter',
          assetPath,
          parameterName,
          defaultValue,
          group,
          x,
          y,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add scalar parameter', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Scalar parameter '${parameterName}' added`);
      }

      case 'add_vector_parameter': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'parameterName', aliases: ['name'], required: true },
          { key: 'defaultValue', aliases: ['color'] },
          { key: 'group', default: 'None' },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const parameterName = extractString(params, 'parameterName');
        const defaultValue = extractOptionalObject(params, 'defaultValue') ?? { r: 1, g: 1, b: 1, a: 1 };
        const group = extractOptionalString(params, 'group') ?? 'None';
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'add_vector_parameter',
          assetPath,
          parameterName,
          defaultValue,
          group,
          x,
          y,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add vector parameter', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Vector parameter '${parameterName}' added`);
      }

      case 'add_static_switch_parameter': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'parameterName', aliases: ['name'], required: true },
          { key: 'defaultValue', default: false },
          { key: 'group', default: 'None' },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const parameterName = extractString(params, 'parameterName');
        const defaultValue = extractOptionalBoolean(params, 'defaultValue') ?? false;
        const group = extractOptionalString(params, 'group') ?? 'None';
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'add_static_switch_parameter',
          assetPath,
          parameterName,
          defaultValue,
          group,
          x,
          y,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add static switch', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Static switch '${parameterName}' added`);
      }

      case 'add_math_node': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'operation', required: true },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
          { key: 'constA', aliases: ['valueA'] },
          { key: 'constB', aliases: ['valueB'] },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const operation = extractString(params, 'operation');
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;
        const constA = extractOptionalNumber(params, 'constA');
        const constB = extractOptionalNumber(params, 'constB');

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'add_math_node',
          assetPath,
          operation,
          x,
          y,
          constA,
          constB,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add math node', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Math node '${operation}' added`);
      }

      case 'add_world_position':
      case 'add_vertex_normal':
      case 'add_pixel_depth':
      case 'add_fresnel':
      case 'add_reflection_vector':
      case 'add_panner':
      case 'add_rotator':
      case 'add_noise':
      case 'add_voronoi': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: action,
          assetPath,
          x,
          y,
          ...args, // Pass through any additional params
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? `Failed to add ${action}`, res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `${action} node added`);
      }

      case 'add_if':
      case 'add_switch': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: action,
          assetPath,
          x,
          y,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? `Failed to add ${action}`, res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `${action} node added`);
      }

      case 'add_custom_expression': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'code', aliases: ['hlsl'], required: true },
          { key: 'outputType', default: 'Float1' },
          { key: 'description' },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const code = extractString(params, 'code');
        const outputType = extractOptionalString(params, 'outputType') ?? 'Float1';
        const description = extractOptionalString(params, 'description');
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'add_custom_expression',
          assetPath,
          code,
          outputType,
          description,
          x,
          y,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add custom expression', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Custom HLSL expression added');
      }

      case 'connect_nodes': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'sourceNodeId', aliases: ['fromNode'], required: true },
          { key: 'sourcePin', aliases: ['fromPin'], default: '' },
          { key: 'targetNodeId', aliases: ['toNode'], required: true },
          { key: 'targetPin', aliases: ['toPin', 'inputName'], required: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const sourceNodeId = extractString(params, 'sourceNodeId');
        const sourcePin = extractOptionalString(params, 'sourcePin') ?? '';
        const targetNodeId = extractString(params, 'targetNodeId');
        const targetPin = extractString(params, 'targetPin');

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'connect_nodes',
          assetPath,
          sourceNodeId,
          sourcePin,
          targetNodeId,
          inputName: targetPin,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to connect nodes', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Nodes connected');
      }

      case 'disconnect_nodes': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'nodeId', required: true },
          { key: 'pinName' },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const nodeId = extractString(params, 'nodeId');
        const pinName = extractOptionalString(params, 'pinName');

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'disconnect_nodes',
          assetPath,
          nodeId,
          pinName,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to disconnect nodes', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Nodes disconnected');
      }

      // ===== 8.3 Material Functions & Layers =====
      case 'create_material_function': {
        const params = normalizeArgs(args, [
          { key: 'name', required: true },
          { key: 'path', aliases: ['directory'], default: '/Game/Materials/Functions' },
          { key: 'description' },
          { key: 'exposeToLibrary', default: true },
          { key: 'save', default: true },
        ]);

        const name = extractString(params, 'name');
        const path = extractOptionalString(params, 'path') ?? '/Game/Materials/Functions';
        const description = extractOptionalString(params, 'description');
        const exposeToLibrary = extractOptionalBoolean(params, 'exposeToLibrary') ?? true;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'create_material_function',
          name,
          path,
          description,
          exposeToLibrary,
          save,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create material function', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Material function '${name}' created`);
      }

      case 'add_function_input':
      case 'add_function_output': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['functionPath'], required: true },
          { key: 'inputName', aliases: ['name', 'outputName'], required: true },
          { key: 'inputType', aliases: ['type', 'outputType'], default: 'Float3' },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const inputName = extractString(params, 'inputName');
        const inputType = extractOptionalString(params, 'inputType') ?? 'Float3';
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: action,
          assetPath,
          inputName,
          inputType,
          x,
          y,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? `Failed to add function ${action === 'add_function_input' ? 'input' : 'output'}`, res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Function ${action === 'add_function_input' ? 'input' : 'output'} '${inputName}' added`);
      }

      case 'use_material_function': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'functionPath', required: true },
          { key: 'x', default: 0 },
          { key: 'y', default: 0 },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const functionPath = extractString(params, 'functionPath');
        const x = extractOptionalNumber(params, 'x') ?? 0;
        const y = extractOptionalNumber(params, 'y') ?? 0;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'use_material_function',
          assetPath,
          functionPath,
          x,
          y,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to use material function', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Material function added');
      }

      // ===== 8.4 Material Instances =====
      case 'create_material_instance': {
        const params = normalizeArgs(args, [
          { key: 'name', required: true },
          { key: 'path', aliases: ['directory'], default: '/Game/Materials' },
          { key: 'parentMaterial', aliases: ['parent'], required: true },
          { key: 'save', default: true },
        ]);

        const name = extractString(params, 'name');
        const path = extractOptionalString(params, 'path') ?? '/Game/Materials';
        const parentMaterial = extractString(params, 'parentMaterial');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'create_material_instance',
          name,
          path,
          parentMaterial,
          save,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create material instance', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Material instance '${name}' created`);
      }

      case 'set_scalar_parameter_value': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['instancePath'], required: true },
          { key: 'parameterName', required: true },
          { key: 'value', required: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const parameterName = extractString(params, 'parameterName');
        const value = extractOptionalNumber(params, 'value') ?? 0;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'set_scalar_parameter_value',
          assetPath,
          parameterName,
          value,
          save,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set scalar parameter', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Scalar parameter '${parameterName}' set to ${value}`);
      }

      case 'set_vector_parameter_value': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['instancePath'], required: true },
          { key: 'parameterName', required: true },
          { key: 'value', aliases: ['color'], required: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const parameterName = extractString(params, 'parameterName');
        const value = extractOptionalObject(params, 'value') ?? { r: 1, g: 1, b: 1, a: 1 };
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'set_vector_parameter_value',
          assetPath,
          parameterName,
          value,
          save,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set vector parameter', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Vector parameter '${parameterName}' set`);
      }

      case 'set_texture_parameter_value': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['instancePath'], required: true },
          { key: 'parameterName', required: true },
          { key: 'texturePath', aliases: ['value'], required: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const parameterName = extractString(params, 'parameterName');
        const texturePath = extractString(params, 'texturePath');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'set_texture_parameter_value',
          assetPath,
          parameterName,
          texturePath,
          save,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set texture parameter', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Texture parameter '${parameterName}' set`);
      }

      // ===== 8.5 Specialized Materials =====
      case 'create_landscape_material':
      case 'create_decal_material':
      case 'create_post_process_material': {
        const params = normalizeArgs(args, [
          { key: 'name', required: true },
          { key: 'path', aliases: ['directory'], default: '/Game/Materials' },
          { key: 'save', default: true },
        ]);

        const name = extractString(params, 'name');
        const path = extractOptionalString(params, 'path') ?? '/Game/Materials';
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: action,
          name,
          path,
          save,
          ...args, // Pass through extra params like layers for landscape
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? `Failed to ${action}`, res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `${action.replace(/_/g, ' ')} created`);
      }

      case 'add_landscape_layer': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'layerName', required: true },
          { key: 'blendType', default: 'LB_WeightBlend' },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const layerName = extractString(params, 'layerName');
        const blendType = extractOptionalString(params, 'blendType') ?? 'LB_WeightBlend';

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'add_landscape_layer',
          assetPath,
          layerName,
          blendType,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to add landscape layer', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Landscape layer '${layerName}' added`);
      }

      case 'configure_layer_blend': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'layers', required: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const layers = params.layers;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'configure_layer_blend',
          assetPath,
          layers,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to configure layer blend', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Layer blend configured');
      }

      case 'compile_material': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'compile_material',
          assetPath,
          save,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to compile material', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Material compiled');
      }

      case 'create_substrate_material': {
        const params = normalizeArgs(args, [
          { key: 'name', required: true },
          { key: 'path', aliases: ['directory'], default: '/Game/Materials' },
          { key: 'blendMode', default: 'Opaque' },
          { key: 'save', default: true },
        ]);

        const name = extractString(params, 'name');
        const path = extractOptionalString(params, 'path') ?? '/Game/Materials';
        const blendMode = extractOptionalString(params, 'blendMode') ?? 'Opaque';
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'create_substrate_material',
          name,
          path,
          blendMode,
          save,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create Substrate material', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Substrate material '${name}' created`);
      }

      case 'set_substrate_properties': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
          { key: 'slabType', default: 'Simple' },
          { key: 'thickness', default: 0.01 },
          { key: 'fuzzAmount', default: 0.0 },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const slabType = extractOptionalString(params, 'slabType') ?? 'Simple';
        const thickness = extractOptionalNumber(params, 'thickness') ?? 0.01;
        const fuzzAmount = extractOptionalNumber(params, 'fuzzAmount') ?? 0.0;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'set_substrate_properties',
          assetPath,
          slabType,
          thickness,
          fuzzAmount,
          save,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set Substrate properties', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Substrate properties configured');
      }

      case 'configure_sss_profile': {
        const params = normalizeArgs(args, [
          { key: 'name', required: true },
          { key: 'path', aliases: ['directory'], default: '/Game/Materials/SSS' },
          { key: 'color', default: { r: 1, g: 0.8, b: 0.8, a: 1 } },
          { key: 'scatterRadius', default: 10.0 },
          { key: 'save', default: true },
        ]);

        const name = extractString(params, 'name');
        const path = extractOptionalString(params, 'path') ?? '/Game/Materials/SSS';
        const color = extractOptionalObject(params, 'color') ?? { r: 1, g: 0.8, b: 0.8, a: 1 };
        const scatterRadius = extractOptionalNumber(params, 'scatterRadius') ?? 10.0;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'configure_sss_profile',
          name,
          path,
          color,
          scatterRadius,
          save,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to configure SSS profile', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `SSS profile '${name}' configured`);
      }

      case 'configure_exposure': {
        const params = normalizeArgs(args, [
          { key: 'postProcessVolumeName', required: true },
          { key: 'minBrightness', default: 0.03 },
          { key: 'maxBrightness', default: 2.0 },
          { key: 'speedUp', default: 3.0 },
          { key: 'speedDown', default: 1.0 },
        ]);

        const postProcessVolumeName = extractString(params, 'postProcessVolumeName');
        const minBrightness = extractOptionalNumber(params, 'minBrightness') ?? 0.03;
        const maxBrightness = extractOptionalNumber(params, 'maxBrightness') ?? 2.0;
        const speedUp = extractOptionalNumber(params, 'speedUp') ?? 3.0;
        const speedDown = extractOptionalNumber(params, 'speedDown') ?? 1.0;

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'configure_exposure',
          postProcessVolumeName,
          minBrightness,
          maxBrightness,
          speedUp,
          speedDown,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to configure exposure', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Exposure configured');
      }

      case 'get_material_info': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['materialPath'], required: true },
        ]);

        const assetPath = extractString(params, 'assetPath');

        const res = (await executeAutomationRequest(tools, 'manage_material_authoring', {
          subAction: 'get_material_info',
          assetPath,
        })) as MaterialAuthoringResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to get material info', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Material info retrieved');
      }

      default:
        return ResponseFactory.error(
          `Unknown material authoring action: ${action}. Available actions: create_material, set_blend_mode, set_shading_model, add_texture_sample, add_scalar_parameter, add_vector_parameter, add_math_node, connect_nodes, create_material_instance, set_scalar_parameter_value, set_vector_parameter_value, set_texture_parameter_value, compile_material, get_material_info`,
          'UNKNOWN_ACTION'
        );
    }
  } catch (error: unknown) {
    const err = error instanceof Error ? error : new Error(String(error));
    return ResponseFactory.error(`Material authoring error: ${err.message}`, 'MATERIAL_ERROR');
  }
}
