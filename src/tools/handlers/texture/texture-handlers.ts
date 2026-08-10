import type { ITools } from '../../../types/tools/tool-interfaces.js';
import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import { ResponseFactory } from '../../../utils/responses/response-factory.js';
import { createUnknownActionResponse } from '../foundation/dispatch/handler-error-context.js';
import { handleTextureChannelAction } from './texture-channel-actions.js';
import { handleTextureCompatAction } from './texture-compat-actions.js';
import { handleTextureGenerationAction } from './texture-generation-actions.js';
import { createTextureContext } from './texture-handler-types.js';
import { handleTextureProcessingAction } from './texture-processing-actions.js';
import { handleTextureSettingsAction } from './texture-settings-actions.js';

const TEXTURE_ACTION_ALIASES: Record<string, string> = {
  create_texture: 'create_noise_texture',
  import_texture: 'import_texture',
  set_texture_compression: 'set_compression_settings',
  set_texture_filter: 'set_filter',
  set_texture_wrap: 'set_wrap',
  set_texture_size: 'resize_texture',
  create_render_target: 'create_render_target',
  create_cube_texture: 'create_cube_texture',
  create_volume_texture: 'create_volume_texture',
  create_texture_array: 'create_texture_array'
};

function normalizeTextureAction(action: string): string {
  return TEXTURE_ACTION_ALIASES[action] ?? action;
}

export async function handleTextureTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const normalizedAction = normalizeTextureAction(action);
  const context = createTextureContext(args, tools);

  try {
    const result =
      await handleTextureGenerationAction(normalizedAction, context) ??
      await handleTextureProcessingAction(normalizedAction, context) ??
      await handleTextureChannelAction(normalizedAction, context) ??
      await handleTextureSettingsAction(normalizedAction, context) ??
      await handleTextureCompatAction(normalizedAction, context);

    return result ?? createUnknownActionResponse(`Unknown texture action: ${action}`);
  } catch (error) {
    const err = error instanceof Error ? error : new Error(String(error));
    return ResponseFactory.error(`Texture operation failed: ${err.message}`, 'TEXTURE_ERROR');
  }
}
