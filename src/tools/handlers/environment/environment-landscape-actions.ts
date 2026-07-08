import { cleanObject } from '../../../utils/serialization/safe-json.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import { buildRegionFromTopLevel, type EnvironmentArgs, vec3ToArray, vec3ToObject } from './environment-handler-utils.js';

export async function handleEnvironmentLandscapeAction(
  action: string,
  argsRecord: Record<string, unknown>,
  argsTyped: EnvironmentArgs,
  tools: ITools
): Promise<Record<string, unknown> | undefined> {
  switch (action) {
    case 'create_landscape': {
      const componentCount = argsTyped.componentCount;
      const componentsX = typeof componentCount === 'object' ? componentCount.x : undefined;
      const componentsY = typeof componentCount === 'object' ? componentCount.y : undefined;
      // Default a plain actor name when omitted (mirrors the procedural handler): the C++ requires a non-empty,
      // path-char-free name and otherwise fails INVALID_ARGUMENT — an empty `name` was the common failure.
      const rawName = typeof argsTyped.name === 'string' ? argsTyped.name.trim() : '';
      const landscapeName = rawName || `Landscape_${Date.now()}`;
      return cleanObject(await executeAutomationRequest(tools, 'create_landscape', {
        name: landscapeName,
        location: vec3ToArray(argsTyped.location),
        sizeX: argsRecord.sizeX as number | undefined,
        sizeY: argsRecord.sizeY as number | undefined,
        quadsPerSection: (argsRecord.quadsPerSection as number | undefined) ?? argsTyped.sectionSize,
        sectionSize: argsTyped.sectionSize,
        sectionsPerComponent: argsTyped.sectionsPerComponent,
        componentCount: componentsX,
        componentsX,
        componentsY,
        materialPath: argsTyped.materialPath
      }) as Record<string, unknown>);
    }
    case 'modify_heightmap':
      return cleanObject(await executeAutomationRequest(tools, 'modify_heightmap', {
        landscapeName: argsTyped.landscapeName || argsTyped.name || '',
        landscapePath: argsTyped.landscapePath || '',
        operation: (argsRecord.operation as string) || 'set',
        heightData: argsTyped.heightData ?? [],
        minX: (argsRecord.minX as number) ?? 0,
        minY: (argsRecord.minY as number) ?? 0,
        maxX: (argsRecord.maxX as number) ?? 0,
        maxY: (argsRecord.maxY as number) ?? 0,
        region: (argsRecord.region as { minX?: number; minY?: number; maxX?: number; maxY?: number } | undefined) ?? buildRegionFromTopLevel(argsRecord),
        updateNormals: argsRecord.updateNormals as boolean | undefined,
        skipFlush: argsRecord.skipFlush as boolean | undefined
      }) as Record<string, unknown>);
    case 'sculpt':
    case 'sculpt_landscape': {
      // Default to 'Raise' tool if not specified
      const tool = (argsRecord.tool as string) || 'Raise';
      return cleanObject(await executeAutomationRequest(tools, 'sculpt_landscape', {
        landscapeName: argsTyped.landscapeName || argsTyped.name || '',
        landscapePath: argsTyped.landscapePath || '',
        tool,
        // C++ expects location as object {x, y, z}, not array
        location: vec3ToObject(argsTyped.location),
        radius: argsTyped.radius || 500,
        brushRadius: argsTyped.radius || 500,
        strength: (argsRecord.strength as number) || 0.5,
        falloff: argsRecord.falloff as number | undefined,
        brushFalloff: argsRecord.falloff as number | undefined,
        toolMode: tool,
        skipFlush: argsRecord.skipFlush as boolean | undefined
      }) as Record<string, unknown>);
    }

    default:
      return undefined;
  }
}
