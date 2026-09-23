import { cleanObject } from '../../../utils/serialization/safe-json.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import type { SystemArgs } from '../../../types/handlers/handler-types.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';

const SUPPORTED_SCREENSHOT_MODES = new Set(['editor_viewport', 'game_viewport', 'full_editor_window']);

function getScreenshotMode(args: SystemArgs): { mode?: string; error?: string } {
  if (typeof args.mode !== 'string' || args.mode.trim() === '') {
    return {};
  }

  const mode = args.mode.trim().toLowerCase();
  if (!SUPPORTED_SCREENSHOT_MODES.has(mode)) {
    return { error: `Unknown screenshot mode: ${args.mode}. Supported: editor_viewport, game_viewport, full_editor_window` };
  }

  return { mode };
}

function buildScreenshotPayload(
  filename: string | undefined,
  resolution: unknown,
  mode: string | undefined,
  returnBase64: boolean | undefined
): Record<string, unknown> {
  const payload: Record<string, unknown> = {
    action: 'screenshot',
    filename,
    resolution
  };

  if (mode !== undefined) payload.mode = mode;
  if (returnBase64 !== undefined) payload.returnBase64 = returnBase64;

  return payload;
}

function parseResolution(value: unknown): { width?: number; height?: number } {
  if (typeof value !== 'string') return {};
  const match = value.trim().match(/^(\d+)x(\d+)$/i);
  if (!match) return {};
  return { width: Number(match[1]), height: Number(match[2]) };
}

function getResolution(args: SystemArgs): { width: number; height: number } {
  const parsed = parseResolution(args.resolution);
  const argsRecord = args as Record<string, unknown>;
  return {
    width: Number.isFinite(Number(argsRecord.width)) ? Number(argsRecord.width) : (parsed.width ?? NaN),
    height: Number.isFinite(Number(argsRecord.height)) ? Number(argsRecord.height) : (parsed.height ?? NaN)
  };
}

export async function handleScreenshot(args: SystemArgs, tools: ITools): Promise<Record<string, unknown>> {
  const includeMetadata = args.includeMetadata === true;
  const filenameArg = typeof args.filename === 'string' ? args.filename : undefined;
  const modeResult = getScreenshotMode(args);

  if (modeResult.error) {
    return {
      success: false,
      type: 'INVALID_ARGUMENT',
      error: 'INVALID_ARGUMENT',
      message: modeResult.error,
      action: {
        tool: 'system_control',
        command: 'screenshot'
      }
    };
  }

  const mode = modeResult.mode;
  const returnBase64 = typeof args.returnBase64 === 'boolean'
    ? args.returnBase64
    : (mode === 'full_editor_window' || mode === 'game_viewport' ? true : undefined);
  const targetAction = mode === 'game_viewport' ? 'system_control' : 'control_editor';

  if (includeMetadata) {
    const baseName = filenameArg && filenameArg.trim().length > 0 ? filenameArg.trim() : `Screenshot_${Date.now()}`;
    const payload = {
      ...buildScreenshotPayload(baseName, args.resolution, mode, returnBase64),
      includeMetadata: true
    };

    try {
      const screenshotPayload = args.metadata === undefined ? payload : { ...payload, metadata: args.metadata };
      const screenshotResponse = await executeAutomationRequest(tools, targetAction, screenshotPayload);
      const cleanedResponse = typeof screenshotResponse === 'object' && screenshotResponse !== null ? screenshotResponse : {};
      return cleanObject({
        ...cleanedResponse,
        action: 'screenshot',
        filename: baseName,
        includeMetadata: true,
        metadata: args.metadata
      });
    } catch (error) {
      if (!(error instanceof Error)) {
        throw error;
      }
      await executeAutomationRequest(tools, targetAction, payload);
    }

    return {
      success: true,
      message: `Metadata screenshot captured: ${baseName}`,
      filename: baseName,
      includeMetadata: true,
      metadata: args.metadata,
      action: 'screenshot',
      handled: true
    };
  }

  const response = await executeAutomationRequest(
    tools,
    targetAction,
    buildScreenshotPayload(filenameArg, args.resolution, mode, returnBase64)
  ) as Record<string, unknown>;
  const cleanedResponse = typeof response === 'object' && response !== null ? response : {};
  return cleanObject({
    ...cleanedResponse,
    metadata: args.metadata,
    action: 'screenshot'
  });
}

export async function handleSetResolution(args: SystemArgs, tools: ITools): Promise<Record<string, unknown>> {
  const { width, height } = getResolution(args);
  if (!Number.isFinite(width) || !Number.isFinite(height) || width <= 0 || height <= 0) {
    return {
      success: false,
      error: 'VALIDATION_ERROR',
      message: 'Validation error: Invalid resolution: width and height must be positive numbers',
      action: 'set_resolution'
    };
  }

  const argsRecord = args as Record<string, unknown>;
  const windowed = argsRecord.windowed !== false;
  const suffix = windowed ? 'w' : 'f';
  await executeAutomationRequest(tools, 'console_command', { command: `r.SetRes ${width}x${height}${suffix}` });
  return {
    success: true,
    message: `Resolution set to ${width}x${height} (${windowed ? 'windowed' : 'fullscreen'})`,
    action: 'set_resolution'
  };
}

export async function handleSetFullscreen(args: SystemArgs, tools: ITools): Promise<Record<string, unknown>> {
  const { width, height } = getResolution(args);
  const argsRecord = args as Record<string, unknown>;
  const windowed = argsRecord.windowed === true || args.enabled === false;
  const fullscreen = args.enabled === true;
  const suffix = windowed ? 'w' : 'f';

  if (!Number.isFinite(width) || !Number.isFinite(height) || width <= 0 || height <= 0) {
    if (typeof argsRecord.windowed === 'boolean' || typeof args.enabled === 'boolean') {
      const modeValue = windowed ? 1 : (fullscreen ? 0 : 1);
      await executeAutomationRequest(tools, 'console_command', { command: `r.FullScreenMode ${modeValue}` });
      return {
        success: true,
        message: `Fullscreen mode toggled (${windowed ? 'windowed' : 'fullscreen'})`,
        action: 'set_fullscreen',
        handled: true
      };
    }

    return {
      success: false,
      error: 'VALIDATION_ERROR',
      message: 'Invalid resolution: provide width/height or resolution like "1920x1080"',
      action: 'set_fullscreen'
    };
  }

  await executeAutomationRequest(tools, 'console_command', { command: `r.SetRes ${width}x${height}${suffix}` });
  return {
    success: true,
    message: `Fullscreen mode set to ${width}x${height} (${windowed ? 'windowed' : 'fullscreen'})`,
    action: 'set_fullscreen'
  };
}

