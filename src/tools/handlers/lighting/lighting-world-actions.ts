import type { ITools } from '../../../types/tools/tool-interfaces.js';
import type { LightingArgs } from '../../../types/handlers/handler-types.js';
import { LONG_RUNNING_OP_TIMEOUT_MS } from '../../../constants.js';
import { TOOL_ACTIONS } from '../../../utils/commands/action-constants.js';
import { toBoolean, toLocationObj, toNumber, toString } from '../../../utils/validation/type-coercion.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import { normalizeLightName } from './lighting-name.js';

export async function buildLighting(tools: ITools, args: LightingArgs): Promise<Record<string, unknown>> {
  const payload = {
    quality: toString(args.quality) || 'High',
    buildOnlySelected: toBoolean(args.buildOnlySelected) || false,
    buildReflectionCaptures: toBoolean(args.buildReflectionCaptures) !== false,
    levelPath: toString(args.levelPath),
    timeoutMs: typeof args.timeoutMs === 'number'
      ? args.timeoutMs
      : LONG_RUNNING_OP_TIMEOUT_MS
  };

  return await executeAutomationRequest(
    tools,
    TOOL_ACTIONS.BAKE_LIGHTMAP,
    payload,
    'Automation bridge not available for lighting build'
  ) as Record<string, unknown>;
}

export async function createLightingEnabledLevel(tools: ITools, args: LightingArgs): Promise<Record<string, unknown>> {
  const levelName = toString(args.levelName) || 'LightingEnabledLevel';
  let path = toString(args.path);
  if (!path) path = `/Game/Maps/${levelName}`;

  const payload = {
    path,
    levelName,
    copyActors: toBoolean(args.copyActors) === true,
    useTemplate: toBoolean(args.useTemplate) === true
  };

  return await executeAutomationRequest(
    tools,
    TOOL_ACTIONS.CREATE_LIGHTING_ENABLED_LEVEL,
    payload,
    'Automation bridge not available for level creation'
  ) as Record<string, unknown>;
}

export async function createLightmassVolume(tools: ITools, args: LightingArgs): Promise<Record<string, unknown>> {
  const name = normalizeLightName(args.name);
  const location = toLocationObj(args.location) || { x: 0, y: 0, z: 0 };
  const size = toLocationObj(args.size) || { x: 1000, y: 1000, z: 1000 };

  return await executeAutomationRequest(
    tools,
    TOOL_ACTIONS.CREATE_LIGHTMASS_VOLUME,
    { name, location, size },
    'Automation bridge not available for lightmass volume creation'
  ) as Record<string, unknown>;
}

export async function setupVolumetricFog(tools: ITools, args: LightingArgs): Promise<Record<string, unknown>> {
  const enabled = args.enabled !== false;

  await executeAutomationRequest(tools, TOOL_ACTIONS.CONSOLE_COMMAND, { command: `r.VolumetricFog ${enabled ? 1 : 0}` });

  // Only what the native handler applies. density/scatteringIntensity/fogHeight
  // were forwarded for a long time and silently ignored on arrival
  // (HandleSetupVolumetricFog reads viewDistance and nothing else), so sending
  // them reported success and changed nothing.
  const payload = {
    enabled,
    viewDistance: toNumber(args.viewDistance)
  };

  return await executeAutomationRequest(
    tools,
    TOOL_ACTIONS.SETUP_VOLUMETRIC_FOG,
    payload,
    'Volumetric fog console setting applied (plugin required for fog actor adjustment)'
  ) as Record<string, unknown>;
}

export async function listLightTypes(tools: ITools): Promise<Record<string, unknown>> {
  return await executeAutomationRequest(
    tools,
    TOOL_ACTIONS.LIST_LIGHT_TYPES,
    {},
    'Automation bridge not available for listing light types'
  ) as Record<string, unknown>;
}
