/**
 * Texture Handlers for Phase 9
 *
 * Provides procedural texture creation, processing, and settings management.
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

/** Texture response */
interface TextureResponse {
  success?: boolean;
  message?: string;
  error?: string;
  errorCode?: string;
  result?: Record<string, unknown>;
  assetPath?: string;
  [key: string]: unknown;
}

/**
 * Handle texture generation and processing actions
 */
export async function handleTextureTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<HandlerResult> {
  try {
    switch (action) {
      // ===== 9.1 Procedural Generation =====
      case 'create_noise_texture': {
        const params = normalizeArgs(args, [
          { key: 'name', required: true },
          { key: 'path', aliases: ['directory'], default: '/Game/Textures' },
          { key: 'noiseType', default: 'Perlin' }, // Perlin, Simplex, Worley, Voronoi
          { key: 'width', default: 1024 },
          { key: 'height', default: 1024 },
          { key: 'scale', default: 1.0 },
          { key: 'octaves', default: 4 },
          { key: 'persistence', default: 0.5 },
          { key: 'lacunarity', default: 2.0 },
          { key: 'seed', default: 0 },
          { key: 'seamless', default: false },
          { key: 'hdr', default: false },
          { key: 'save', default: true },
        ]);

        const name = extractString(params, 'name');
        const path = extractOptionalString(params, 'path') ?? '/Game/Textures';
        const noiseType = extractOptionalString(params, 'noiseType') ?? 'Perlin';
        const width = extractOptionalNumber(params, 'width') ?? 1024;
        const height = extractOptionalNumber(params, 'height') ?? 1024;
        const scale = extractOptionalNumber(params, 'scale') ?? 1.0;
        const octaves = extractOptionalNumber(params, 'octaves') ?? 4;
        const persistence = extractOptionalNumber(params, 'persistence') ?? 0.5;
        const lacunarity = extractOptionalNumber(params, 'lacunarity') ?? 2.0;
        const seed = extractOptionalNumber(params, 'seed') ?? 0;
        const seamless = extractOptionalBoolean(params, 'seamless') ?? false;
        const hdr = extractOptionalBoolean(params, 'hdr') ?? false;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_texture', {
          subAction: 'create_noise_texture',
          name,
          path,
          noiseType,
          width,
          height,
          scale,
          octaves,
          persistence,
          lacunarity,
          seed,
          seamless,
          hdr,
          save,
        })) as TextureResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create noise texture', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Noise texture '${name}' created`);
      }

      case 'create_gradient_texture': {
        const params = normalizeArgs(args, [
          { key: 'name', required: true },
          { key: 'path', aliases: ['directory'], default: '/Game/Textures' },
          { key: 'gradientType', default: 'Linear' }, // Linear, Radial, Angular
          { key: 'width', default: 1024 },
          { key: 'height', default: 1024 },
          { key: 'startColor', default: { r: 0, g: 0, b: 0, a: 1 } },
          { key: 'endColor', default: { r: 1, g: 1, b: 1, a: 1 } },
          { key: 'angle', default: 0 }, // For linear gradient
          { key: 'centerX', default: 0.5 }, // For radial gradient
          { key: 'centerY', default: 0.5 },
          { key: 'radius', default: 0.5 }, // For radial gradient
          { key: 'colorStops' }, // Optional array of {position, color} for multi-color gradients
          { key: 'hdr', default: false },
          { key: 'save', default: true },
        ]);

        const name = extractString(params, 'name');
        const path = extractOptionalString(params, 'path') ?? '/Game/Textures';
        const gradientType = extractOptionalString(params, 'gradientType') ?? 'Linear';
        const width = extractOptionalNumber(params, 'width') ?? 1024;
        const height = extractOptionalNumber(params, 'height') ?? 1024;
        const startColor = extractOptionalObject(params, 'startColor') ?? { r: 0, g: 0, b: 0, a: 1 };
        const endColor = extractOptionalObject(params, 'endColor') ?? { r: 1, g: 1, b: 1, a: 1 };
        const angle = extractOptionalNumber(params, 'angle') ?? 0;
        const centerX = extractOptionalNumber(params, 'centerX') ?? 0.5;
        const centerY = extractOptionalNumber(params, 'centerY') ?? 0.5;
        const radius = extractOptionalNumber(params, 'radius') ?? 0.5;
        const colorStops = extractOptionalArray(params, 'colorStops');
        const hdr = extractOptionalBoolean(params, 'hdr') ?? false;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_texture', {
          subAction: 'create_gradient_texture',
          name,
          path,
          gradientType,
          width,
          height,
          startColor,
          endColor,
          angle,
          centerX,
          centerY,
          radius,
          colorStops,
          hdr,
          save,
        })) as TextureResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create gradient texture', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Gradient texture '${name}' created`);
      }

      case 'create_pattern_texture': {
        const params = normalizeArgs(args, [
          { key: 'name', required: true },
          { key: 'path', aliases: ['directory'], default: '/Game/Textures' },
          { key: 'patternType', default: 'Checker' }, // Checker, Grid, Brick, Tile, Dots, Stripes
          { key: 'width', default: 1024 },
          { key: 'height', default: 1024 },
          { key: 'primaryColor', default: { r: 1, g: 1, b: 1, a: 1 } },
          { key: 'secondaryColor', default: { r: 0, g: 0, b: 0, a: 1 } },
          { key: 'tilesX', default: 8 },
          { key: 'tilesY', default: 8 },
          { key: 'lineWidth', default: 0.02 }, // For grid/stripes
          { key: 'brickRatio', default: 2.0 }, // Width/height ratio for bricks
          { key: 'offset', default: 0.5 }, // Brick offset
          { key: 'save', default: true },
        ]);

        const name = extractString(params, 'name');
        const path = extractOptionalString(params, 'path') ?? '/Game/Textures';
        const patternType = extractOptionalString(params, 'patternType') ?? 'Checker';
        const width = extractOptionalNumber(params, 'width') ?? 1024;
        const height = extractOptionalNumber(params, 'height') ?? 1024;
        const primaryColor = extractOptionalObject(params, 'primaryColor') ?? { r: 1, g: 1, b: 1, a: 1 };
        const secondaryColor = extractOptionalObject(params, 'secondaryColor') ?? { r: 0, g: 0, b: 0, a: 1 };
        const tilesX = extractOptionalNumber(params, 'tilesX') ?? 8;
        const tilesY = extractOptionalNumber(params, 'tilesY') ?? 8;
        const lineWidth = extractOptionalNumber(params, 'lineWidth') ?? 0.02;
        const brickRatio = extractOptionalNumber(params, 'brickRatio') ?? 2.0;
        const offset = extractOptionalNumber(params, 'offset') ?? 0.5;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_texture', {
          subAction: 'create_pattern_texture',
          name,
          path,
          patternType,
          width,
          height,
          primaryColor,
          secondaryColor,
          tilesX,
          tilesY,
          lineWidth,
          brickRatio,
          offset,
          save,
        })) as TextureResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create pattern texture', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Pattern texture '${name}' created`);
      }

      case 'create_normal_from_height': {
        const params = normalizeArgs(args, [
          { key: 'sourceTexture', aliases: ['heightMapPath'], required: true },
          { key: 'name' }, // Optional - defaults to source name + _N
          { key: 'path', aliases: ['directory'] }, // Optional - defaults to source directory
          { key: 'strength', default: 1.0 },
          { key: 'algorithm', default: 'Sobel' }, // Sobel, Prewitt, Scharr
          { key: 'flipY', default: false }, // Flip green channel for DirectX/OpenGL compatibility
          { key: 'save', default: true },
        ]);

        const sourceTexture = extractString(params, 'sourceTexture');
        const name = extractOptionalString(params, 'name');
        const path = extractOptionalString(params, 'path');
        const strength = extractOptionalNumber(params, 'strength') ?? 1.0;
        const algorithm = extractOptionalString(params, 'algorithm') ?? 'Sobel';
        const flipY = extractOptionalBoolean(params, 'flipY') ?? false;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_texture', {
          subAction: 'create_normal_from_height',
          sourceTexture,
          name,
          path,
          strength,
          algorithm,
          flipY,
          save,
        })) as TextureResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create normal map from height', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Normal map created from height map');
      }

      case 'create_ao_from_mesh': {
        const params = normalizeArgs(args, [
          { key: 'meshPath', required: true },
          { key: 'name', required: true },
          { key: 'path', aliases: ['directory'], default: '/Game/Textures' },
          { key: 'width', default: 1024 },
          { key: 'height', default: 1024 },
          { key: 'samples', default: 64 },
          { key: 'rayDistance', default: 100.0 },
          { key: 'bias', default: 0.01 },
          { key: 'uvChannel', default: 0 },
          { key: 'save', default: true },
        ]);

        const meshPath = extractString(params, 'meshPath');
        const name = extractString(params, 'name');
        const path = extractOptionalString(params, 'path') ?? '/Game/Textures';
        const width = extractOptionalNumber(params, 'width') ?? 1024;
        const height = extractOptionalNumber(params, 'height') ?? 1024;
        const samples = extractOptionalNumber(params, 'samples') ?? 64;
        const rayDistance = extractOptionalNumber(params, 'rayDistance') ?? 100.0;
        const bias = extractOptionalNumber(params, 'bias') ?? 0.01;
        const uvChannel = extractOptionalNumber(params, 'uvChannel') ?? 0;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_texture', {
          subAction: 'create_ao_from_mesh',
          meshPath,
          name,
          path,
          width,
          height,
          samples,
          rayDistance,
          bias,
          uvChannel,
          save,
        })) as TextureResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to create AO from mesh', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `AO texture '${name}' created from mesh`);
      }

      // ===== 9.2 Texture Processing =====
      case 'resize_texture': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['texturePath'], required: true },
          { key: 'newWidth', required: true },
          { key: 'newHeight', required: true },
          { key: 'filterMethod', default: 'Bilinear' }, // Nearest, Bilinear, Bicubic, Lanczos
          { key: 'preserveAspect', default: false },
          { key: 'outputPath' }, // Optional - defaults to overwriting source
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const newWidth = extractOptionalNumber(params, 'newWidth') ?? 1024;
        const newHeight = extractOptionalNumber(params, 'newHeight') ?? 1024;
        const filterMethod = extractOptionalString(params, 'filterMethod') ?? 'Bilinear';
        const preserveAspect = extractOptionalBoolean(params, 'preserveAspect') ?? false;
        const outputPath = extractOptionalString(params, 'outputPath');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_texture', {
          subAction: 'resize_texture',
          assetPath,
          newWidth,
          newHeight,
          filterMethod,
          preserveAspect,
          outputPath,
          save,
        })) as TextureResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to resize texture', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Texture resized to ${newWidth}x${newHeight}`);
      }

      case 'adjust_levels': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['texturePath'], required: true },
          { key: 'inputBlackPoint', default: 0.0 },
          { key: 'inputWhitePoint', default: 1.0 },
          { key: 'gamma', default: 1.0 },
          { key: 'outputBlackPoint', default: 0.0 },
          { key: 'outputWhitePoint', default: 1.0 },
          { key: 'channel', default: 'All' }, // All, Red, Green, Blue, Alpha
          { key: 'outputPath' },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const inputBlackPoint = extractOptionalNumber(params, 'inputBlackPoint') ?? 0.0;
        const inputWhitePoint = extractOptionalNumber(params, 'inputWhitePoint') ?? 1.0;
        const gamma = extractOptionalNumber(params, 'gamma') ?? 1.0;
        const outputBlackPoint = extractOptionalNumber(params, 'outputBlackPoint') ?? 0.0;
        const outputWhitePoint = extractOptionalNumber(params, 'outputWhitePoint') ?? 1.0;
        const channel = extractOptionalString(params, 'channel') ?? 'All';
        const outputPath = extractOptionalString(params, 'outputPath');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_texture', {
          subAction: 'adjust_levels',
          assetPath,
          inputBlackPoint,
          inputWhitePoint,
          gamma,
          outputBlackPoint,
          outputWhitePoint,
          channel,
          outputPath,
          save,
        })) as TextureResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to adjust levels', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Texture levels adjusted');
      }

      case 'adjust_curves': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['texturePath'], required: true },
          { key: 'curvePoints', required: true }, // Array of {x, y} points defining the curve
          { key: 'channel', default: 'All' }, // All, Red, Green, Blue, Alpha
          { key: 'outputPath' },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const curvePoints = extractOptionalArray(params, 'curvePoints') ?? [];
        const channel = extractOptionalString(params, 'channel') ?? 'All';
        const outputPath = extractOptionalString(params, 'outputPath');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_texture', {
          subAction: 'adjust_curves',
          assetPath,
          curvePoints,
          channel,
          outputPath,
          save,
        })) as TextureResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to adjust curves', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Texture curves adjusted');
      }

      case 'blur': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['texturePath'], required: true },
          { key: 'radius', default: 2.0 },
          { key: 'blurType', default: 'Gaussian' }, // Gaussian, Box, Radial
          { key: 'outputPath' },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const radius = extractOptionalNumber(params, 'radius') ?? 2.0;
        const blurType = extractOptionalString(params, 'blurType') ?? 'Gaussian';
        const outputPath = extractOptionalString(params, 'outputPath');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_texture', {
          subAction: 'blur',
          assetPath,
          radius,
          blurType,
          outputPath,
          save,
        })) as TextureResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to blur texture', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Texture blurred');
      }

      case 'sharpen': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['texturePath'], required: true },
          { key: 'strength', default: 1.0 },
          { key: 'radius', default: 1.0 },
          { key: 'sharpenType', default: 'UnsharpMask' }, // UnsharpMask, Laplacian
          { key: 'outputPath' },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const strength = extractOptionalNumber(params, 'strength') ?? 1.0;
        const radius = extractOptionalNumber(params, 'radius') ?? 1.0;
        const sharpenType = extractOptionalString(params, 'sharpenType') ?? 'UnsharpMask';
        const outputPath = extractOptionalString(params, 'outputPath');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_texture', {
          subAction: 'sharpen',
          assetPath,
          strength,
          radius,
          sharpenType,
          outputPath,
          save,
        })) as TextureResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to sharpen texture', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Texture sharpened');
      }

      case 'invert': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['texturePath'], required: true },
          { key: 'invertAlpha', default: false },
          { key: 'channel', default: 'All' }, // All, Red, Green, Blue, Alpha
          { key: 'outputPath' },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const invertAlpha = extractOptionalBoolean(params, 'invertAlpha') ?? false;
        const channel = extractOptionalString(params, 'channel') ?? 'All';
        const outputPath = extractOptionalString(params, 'outputPath');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_texture', {
          subAction: 'invert',
          assetPath,
          invertAlpha,
          channel,
          outputPath,
          save,
        })) as TextureResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to invert texture', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Texture inverted');
      }

      case 'desaturate': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['texturePath'], required: true },
          { key: 'amount', default: 1.0 }, // 0.0 = no change, 1.0 = full desaturation
          { key: 'method', default: 'Luminance' }, // Luminance, Average, Lightness
          { key: 'outputPath' },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const amount = extractOptionalNumber(params, 'amount') ?? 1.0;
        const method = extractOptionalString(params, 'method') ?? 'Luminance';
        const outputPath = extractOptionalString(params, 'outputPath');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_texture', {
          subAction: 'desaturate',
          assetPath,
          amount,
          method,
          outputPath,
          save,
        })) as TextureResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to desaturate texture', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Texture desaturated');
      }

      case 'channel_pack': {
        const params = normalizeArgs(args, [
          { key: 'name', required: true },
          { key: 'path', aliases: ['directory'], default: '/Game/Textures' },
          { key: 'redChannel' }, // Asset path or null for black
          { key: 'greenChannel' },
          { key: 'blueChannel' },
          { key: 'alphaChannel' },
          { key: 'redSourceChannel', default: 'Red' }, // Which channel to extract from source
          { key: 'greenSourceChannel', default: 'Green' },
          { key: 'blueSourceChannel', default: 'Blue' },
          { key: 'alphaSourceChannel', default: 'Alpha' },
          { key: 'width' }, // Optional - auto-detect from first valid input
          { key: 'height' },
          { key: 'save', default: true },
        ]);

        const name = extractString(params, 'name');
        const path = extractOptionalString(params, 'path') ?? '/Game/Textures';
        const redChannel = extractOptionalString(params, 'redChannel');
        const greenChannel = extractOptionalString(params, 'greenChannel');
        const blueChannel = extractOptionalString(params, 'blueChannel');
        const alphaChannel = extractOptionalString(params, 'alphaChannel');
        const redSourceChannel = extractOptionalString(params, 'redSourceChannel') ?? 'Red';
        const greenSourceChannel = extractOptionalString(params, 'greenSourceChannel') ?? 'Green';
        const blueSourceChannel = extractOptionalString(params, 'blueSourceChannel') ?? 'Blue';
        const alphaSourceChannel = extractOptionalString(params, 'alphaSourceChannel') ?? 'Alpha';
        const width = extractOptionalNumber(params, 'width');
        const height = extractOptionalNumber(params, 'height');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_texture', {
          subAction: 'channel_pack',
          name,
          path,
          redChannel,
          greenChannel,
          blueChannel,
          alphaChannel,
          redSourceChannel,
          greenSourceChannel,
          blueSourceChannel,
          alphaSourceChannel,
          width,
          height,
          save,
        })) as TextureResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to pack channels', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Packed texture '${name}' created`);
      }

      case 'channel_extract': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['texturePath'], required: true },
          { key: 'channel', required: true }, // Red, Green, Blue, Alpha
          { key: 'name' }, // Optional - defaults to source_R/G/B/A
          { key: 'path', aliases: ['directory'] },
          { key: 'outputAsGrayscale', default: true },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const channel = extractString(params, 'channel');
        const name = extractOptionalString(params, 'name');
        const path = extractOptionalString(params, 'path');
        const outputAsGrayscale = extractOptionalBoolean(params, 'outputAsGrayscale') ?? true;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_texture', {
          subAction: 'channel_extract',
          assetPath,
          channel,
          name,
          path,
          outputAsGrayscale,
          save,
        })) as TextureResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to extract channel', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Channel ${channel} extracted`);
      }

      case 'combine_textures': {
        const params = normalizeArgs(args, [
          { key: 'name', required: true },
          { key: 'path', aliases: ['directory'], default: '/Game/Textures' },
          { key: 'baseTexture', required: true },
          { key: 'blendTexture', required: true },
          { key: 'blendMode', default: 'Multiply' }, // Multiply, Add, Subtract, Screen, Overlay, SoftLight, HardLight, Difference, Normal
          { key: 'opacity', default: 1.0 },
          { key: 'maskTexture' }, // Optional mask for blending
          { key: 'save', default: true },
        ]);

        const name = extractString(params, 'name');
        const path = extractOptionalString(params, 'path') ?? '/Game/Textures';
        const baseTexture = extractString(params, 'baseTexture');
        const blendTexture = extractString(params, 'blendTexture');
        const blendMode = extractOptionalString(params, 'blendMode') ?? 'Multiply';
        const opacity = extractOptionalNumber(params, 'opacity') ?? 1.0;
        const maskTexture = extractOptionalString(params, 'maskTexture');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_texture', {
          subAction: 'combine_textures',
          name,
          path,
          baseTexture,
          blendTexture,
          blendMode,
          opacity,
          maskTexture,
          save,
        })) as TextureResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to combine textures', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Combined texture '${name}' created`);
      }

      // ===== 9.3 Texture Settings =====
      case 'set_compression_settings': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['texturePath'], required: true },
          { key: 'compressionSettings', required: true }, // TC_Default, TC_Normalmap, TC_Masks, TC_Grayscale, TC_Displacementmap, TC_VectorDisplacementmap, TC_HDR, TC_EditorIcon, TC_Alpha, TC_DistanceFieldFont, TC_HDR_Compressed, TC_BC7
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const compressionSettings = extractString(params, 'compressionSettings');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_texture', {
          subAction: 'set_compression_settings',
          assetPath,
          compressionSettings,
          save,
        })) as TextureResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set compression settings', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Compression set to ${compressionSettings}`);
      }

      case 'set_texture_group': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['texturePath'], required: true },
          { key: 'textureGroup', required: true }, // TEXTUREGROUP_World, TEXTUREGROUP_WorldNormalMap, TEXTUREGROUP_WorldSpecular, TEXTUREGROUP_Character, TEXTUREGROUP_CharacterNormalMap, TEXTUREGROUP_CharacterSpecular, TEXTUREGROUP_Weapon, TEXTUREGROUP_WeaponNormalMap, TEXTUREGROUP_WeaponSpecular, TEXTUREGROUP_Vehicle, TEXTUREGROUP_VehicleNormalMap, TEXTUREGROUP_VehicleSpecular, TEXTUREGROUP_Cinematic, TEXTUREGROUP_Effects, TEXTUREGROUP_EffectsNotFiltered, TEXTUREGROUP_Skybox, TEXTUREGROUP_UI, TEXTUREGROUP_Lightmap, TEXTUREGROUP_RenderTarget, TEXTUREGROUP_MobileFlattened, TEXTUREGROUP_ProcBuilding_Face, TEXTUREGROUP_ProcBuilding_LightMap, TEXTUREGROUP_Shadowmap, TEXTUREGROUP_ColorLookupTable, TEXTUREGROUP_Terrain_Heightmap, TEXTUREGROUP_Terrain_Weightmap, TEXTUREGROUP_Bokeh, TEXTUREGROUP_IESLightProfile, TEXTUREGROUP_Pixels2D, TEXTUREGROUP_HierarchicalLOD, TEXTUREGROUP_Impostor, TEXTUREGROUP_ImpostorNormalDepth, TEXTUREGROUP_8BitData, TEXTUREGROUP_16BitData, TEXTUREGROUP_Project01-15
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const textureGroup = extractString(params, 'textureGroup');
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_texture', {
          subAction: 'set_texture_group',
          assetPath,
          textureGroup,
          save,
        })) as TextureResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set texture group', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Texture group set to ${textureGroup}`);
      }

      case 'set_lod_bias': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['texturePath'], required: true },
          { key: 'lodBias', required: true }, // Integer, typically -2 to 4
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const lodBias = extractOptionalNumber(params, 'lodBias') ?? 0;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_texture', {
          subAction: 'set_lod_bias',
          assetPath,
          lodBias,
          save,
        })) as TextureResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set LOD bias', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `LOD bias set to ${lodBias}`);
      }

      case 'configure_virtual_texture': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['texturePath'], required: true },
          { key: 'virtualTextureStreaming', required: true }, // true/false
          { key: 'tileSize', default: 128 }, // 32, 64, 128, 256, 512, 1024
          { key: 'tileBorderSize', default: 4 },
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const virtualTextureStreaming = extractOptionalBoolean(params, 'virtualTextureStreaming') ?? false;
        const tileSize = extractOptionalNumber(params, 'tileSize') ?? 128;
        const tileBorderSize = extractOptionalNumber(params, 'tileBorderSize') ?? 4;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_texture', {
          subAction: 'configure_virtual_texture',
          assetPath,
          virtualTextureStreaming,
          tileSize,
          tileBorderSize,
          save,
        })) as TextureResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to configure virtual texture', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? `Virtual texture streaming ${virtualTextureStreaming ? 'enabled' : 'disabled'}`);
      }

      case 'set_streaming_priority': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['texturePath'], required: true },
          { key: 'neverStream', default: false },
          { key: 'streamingPriority', default: 0 }, // -1 to 1, lower = higher priority
          { key: 'save', default: true },
        ]);

        const assetPath = extractString(params, 'assetPath');
        const neverStream = extractOptionalBoolean(params, 'neverStream') ?? false;
        const streamingPriority = extractOptionalNumber(params, 'streamingPriority') ?? 0;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        const res = (await executeAutomationRequest(tools, 'manage_texture', {
          subAction: 'set_streaming_priority',
          assetPath,
          neverStream,
          streamingPriority,
          save,
        })) as TextureResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to set streaming priority', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Streaming priority configured');
      }

      // ===== Utility Actions =====
      case 'get_texture_info': {
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['texturePath'], required: true },
        ]);

        const assetPath = extractString(params, 'assetPath');

        const res = (await executeAutomationRequest(tools, 'manage_texture', {
          subAction: 'get_texture_info',
          assetPath,
        })) as TextureResponse;

        if (res.success === false) {
          return ResponseFactory.error(res.error ?? 'Failed to get texture info', res.errorCode);
        }
        return ResponseFactory.success(res, res.message ?? 'Texture info retrieved');
      }

      default:
        return ResponseFactory.error(`Unknown texture action: ${action}`, 'UNKNOWN_ACTION');
    }
  } catch (error: unknown) {
    const err = error instanceof Error ? error : new Error(String(error));
    return ResponseFactory.error(`Texture operation failed: ${err.message}`, 'TEXTURE_ERROR');
  }
}

// Additional utility action not in original roadmap but useful
// get_texture_info: Returns width, height, format, compression, mip count, etc.
