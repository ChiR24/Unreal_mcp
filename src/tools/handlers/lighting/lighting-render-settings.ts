import { cleanObject } from '../../../utils/serialization/safe-json.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import type { LightingArgs } from '../../../types/handlers/handler-types.js';
import { TOOL_ACTIONS } from '../../../utils/commands/action-constants.js';
import { toBoolean, toNumber, toString } from '../../../utils/validation/type-coercion.js';
import { executeAutomationRequest, executeBatchConsoleCommands } from '../foundation/dispatch/common-handlers.js';

function isBridgeConnectionFailure(result: unknown): result is { success: false; error: string } {
  return (
    typeof result === 'object' &&
    result !== null &&
    'success' in result &&
    'error' in result &&
    result.success === false &&
    typeof result.error === 'string' &&
    (result.error.includes('not available') || result.error.includes('Connection'))
  );
}

export async function setupGlobalIllumination(tools: ITools, args: LightingArgs): Promise<Record<string, unknown>> {
  if (!args.method) {
    return {
      success: false,
      isError: true,
      error: 'MISSING_REQUIRED_PARAM',
      message: "'method' parameter is required for setup_global_illumination. Must be one of: LumenGI, ScreenSpace, None, RayTraced, Lightmass"
    };
  }

  let normalizedMethod: string | undefined;
  const methodLower = String(args.method).toLowerCase();

  if (methodLower === 'lumen' || methodLower === 'lumengi') {
    normalizedMethod = 'LumenGI';
  } else if (methodLower === 'screenspace' || methodLower === 'ssgi') {
    normalizedMethod = 'ScreenSpace';
  } else if (methodLower === 'none') {
    normalizedMethod = 'None';
  } else if (methodLower === 'raytraced') {
    normalizedMethod = 'RayTraced';
  } else if (methodLower === 'lightmass') {
    normalizedMethod = 'Lightmass';
  } else {
    return {
      success: false,
      isError: true,
      error: 'INVALID_GI_METHOD',
      message: `Invalid GI method: '${args.method}'. Must be one of: LumenGI, ScreenSpace, None, RayTraced, Lightmass`
    };
  }

  const payload = {
    method: normalizedMethod,
    quality: toString(args.quality),
    indirectLightingIntensity: toNumber(args.indirectLightingIntensity),
    bounces: toNumber(args.bounces)
  };

  const result = await executeAutomationRequest(tools, TOOL_ACTIONS.SETUP_GLOBAL_ILLUMINATION, payload);
  if (isBridgeConnectionFailure(result)) {
    const commands: string[] = [];

    switch (normalizedMethod) {
      case 'Lightmass': commands.push('r.DynamicGlobalIlluminationMethod 0'); break;
      case 'LumenGI': commands.push('r.DynamicGlobalIlluminationMethod 1'); break;
      case 'ScreenSpace': commands.push('r.DynamicGlobalIlluminationMethod 2'); break;
      case 'None': commands.push('r.DynamicGlobalIlluminationMethod 3'); break;
    }

    if (args.quality) {
      const qualityMap: Record<string, number> = { Low: 0, Medium: 1, High: 2, Epic: 3 };
      commands.push(`r.Lumen.Quality ${qualityMap[args.quality] ?? 1}`);
    }
    if (args.indirectLightingIntensity !== undefined) {
      commands.push(`r.IndirectLightingIntensity ${args.indirectLightingIntensity}`);
    }
    if (args.bounces !== undefined) {
      commands.push(`r.Lumen.MaxReflectionBounces ${args.bounces}`);
    }
    if (commands.length > 0) {
      await executeBatchConsoleCommands(tools, commands);
    }

    return { success: true, message: 'Global illumination configured (console)' };
  }

  return cleanObject(result) as Record<string, unknown>;
}

export async function configureShadows(tools: ITools, args: LightingArgs): Promise<Record<string, unknown>> {
  const payload = {
    // The capability record declares `actorName` and `settings` for this action and
    // the native handler reads both (settings first, then top level), but this
    // payload was built from a fixed list that included neither, so the documented
    // call shape arrived empty and failed "No shadow settings supplied".
    actorName: toString(args.actorName),
    settings: args.settings,
    shadowQuality: toString(args.shadowQuality),
    cascadedShadows: toBoolean(args.cascadedShadows),
    shadowDistance: toNumber(args.shadowDistance),
    contactShadows: toBoolean(args.contactShadows),
    rayTracedShadows: toBoolean(args.rayTracedShadows),
    // NOT derived from rayTracedShadows: the native handler applies
    // r.Shadow.Virtual.Enable for this flag and answers rayTracedShadows with
    // "different feature ... NOT applied here", so aliasing them turned a
    // ray-tracing request into a silent virtual-shadow-map switch.
    virtualShadowMaps: toBoolean(args.virtualShadowMaps)
  };

  const result = await executeAutomationRequest(tools, TOOL_ACTIONS.CONFIGURE_SHADOWS, payload);
  if (isBridgeConnectionFailure(result)) {
    const commands: string[] = [];

    if (args.shadowQuality) {
      const qualityMap: Record<string, number> = { Low: 0, Medium: 1, High: 2, Epic: 3 };
      commands.push(`r.ShadowQuality ${qualityMap[args.shadowQuality] ?? 1}`);
    }
    if (args.cascadedShadows !== undefined) {
      commands.push(`r.Shadow.CSM.MaxCascades ${args.cascadedShadows ? 4 : 1}`);
    }
    if (args.shadowDistance !== undefined) {
      commands.push(`r.Shadow.DistanceScale ${args.shadowDistance}`);
    }
    if (args.contactShadows !== undefined) {
      commands.push(`r.ContactShadows ${args.contactShadows ? 1 : 0}`);
    }
    if (args.rayTracedShadows !== undefined) {
      commands.push(`r.RayTracing.Shadows ${args.rayTracedShadows ? 1 : 0}`);
    }
    if (args.virtualShadowMaps !== undefined) {
      commands.push(`r.Shadow.Virtual.Enable ${args.virtualShadowMaps ? 1 : 0}`);
    }
    if (commands.length > 0) {
      await executeBatchConsoleCommands(tools, commands);
    }

    return { success: true, message: 'Shadow settings configured (console)' };
  }

  return cleanObject(result) as Record<string, unknown>;
}

export async function setExposure(tools: ITools, args: LightingArgs): Promise<Record<string, unknown>> {
  const payload = {
    // Native resolves the PostProcessVolume from actorName/targetActor/actorPath
    // (McpPostProcessVolumeResolution.cpp). The record declared actorName but
    // this payload dropped it, so targeting one volume silently hit whichever
    // volume the resolver picked first.
    actorName: toString(args.actorName),
    method: toString(args.method),
    compensationValue: toNumber(args.compensationValue),
    minBrightness: toNumber(args.minBrightness),
    maxBrightness: toNumber(args.maxBrightness)
  };

  const result = await executeAutomationRequest(tools, TOOL_ACTIONS.SET_EXPOSURE, payload);
  if (isBridgeConnectionFailure(result)) {
    const commands: string[] = [`r.EyeAdaptation.ExposureMethod ${args.method === 'Manual' ? 0 : 1}`];

    if (args.compensationValue !== undefined) {
      commands.push(`r.EyeAdaptation.ExposureCompensation ${args.compensationValue}`);
    }
    if (args.minBrightness !== undefined) {
      commands.push(`r.EyeAdaptation.MinBrightness ${args.minBrightness}`);
    }
    if (args.maxBrightness !== undefined) {
      commands.push(`r.EyeAdaptation.MaxBrightness ${args.maxBrightness}`);
    }
    if (commands.length > 0) {
      await executeBatchConsoleCommands(tools, commands);
    }

    return { success: true, message: 'Exposure settings updated (console)' };
  }

  return cleanObject(result) as Record<string, unknown>;
}

export async function setAmbientOcclusion(tools: ITools, args: LightingArgs): Promise<Record<string, unknown>> {
  const enabled = args.enabled !== false;
  const payload = {
    // Same volume-resolution field set_exposure needs; see the note there.
    actorName: toString(args.actorName),
    enabled,
    intensity: toNumber(args.intensity),
    radius: toNumber(args.radius),
    quality: toString(args.quality)
  };

  const result = await executeAutomationRequest(tools, TOOL_ACTIONS.SET_AMBIENT_OCCLUSION, payload);
  if (isBridgeConnectionFailure(result)) {
    const commands: string[] = [`r.AmbientOcclusion.Enabled ${enabled ? 1 : 0}`];

    if (args.intensity !== undefined) {
      commands.push(`r.AmbientOcclusion.Intensity ${args.intensity}`);
    }
    if (args.radius !== undefined) {
      commands.push(`r.AmbientOcclusion.Radius ${args.radius}`);
    }
    if (args.quality) {
      const qualityMap: Record<string, number> = { Low: 0, Medium: 1, High: 2 };
      commands.push(`r.AmbientOcclusion.Quality ${qualityMap[args.quality] ?? 1}`);
    }
    if (commands.length > 0) {
      await executeBatchConsoleCommands(tools, commands);
    }

    return { success: true, message: 'Ambient occlusion configured (console)' };
  }

  return cleanObject(result) as Record<string, unknown>;
}
