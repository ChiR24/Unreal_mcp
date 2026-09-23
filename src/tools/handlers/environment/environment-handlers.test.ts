import { beforeEach, describe, expect, it, vi } from 'vitest';

const { executeAutomationRequestMock } = vi.hoisted(() => ({
  executeAutomationRequestMock: vi.fn(async () => ({ success: true, result: {} }))
}));

vi.mock('../foundation/dispatch/common-handlers.js', async () => {
  const actual = await vi.importActual<typeof import('../foundation/dispatch/common-handlers.js')>('../foundation/dispatch/common-handlers.js');
  return {
    ...actual,
    executeAutomationRequest: executeAutomationRequestMock
  };
});

import { handleEnvironmentTools } from './environment-handlers.js';
import { consolidatedToolDefinitions } from '../../catalog/consolidated-tool-definitions.js';
import { ALL_CAPABILITY_RECORDS } from '../../catalog/capabilities/records/aggregate.js';

const ENVIRONMENT_ACTIONS = [
  'create_landscape', 'import_heightmap', 'export_heightmap', 'sculpt_landscape',
  'paint_landscape_layer', 'create_landscape_layer_info', 'configure_landscape_material',
  'create_landscape_grass_type', 'configure_landscape_splines', 'configure_landscape_lod',
  'create_landscape_streaming_proxy', 'create_foliage_type', 'configure_foliage_mesh',
  'configure_foliage_placement', 'configure_foliage_lod', 'configure_foliage_collision',
  'configure_foliage_culling', 'paint_foliage_instances', 'remove_foliage_instances',
  'configure_sky_atmosphere', 'configure_sky_light', 'configure_directional_light_atmosphere',
  'configure_exponential_height_fog', 'configure_volumetric_cloud', 'create_sky_sphere',
  'create_weather_system', 'configure_rain_particles', 'configure_snow_particles',
  'configure_wind', 'configure_lightning', 'create_time_of_day_system', 'configure_sun_position',
  'configure_light_color_curve', 'configure_sky_color_curve', 'create_water_body_ocean',
  'create_water_body_lake', 'create_water_body_river', 'create_water_body_custom',
  'configure_water_waves', 'configure_water_material', 'configure_water_collision',
  'create_buoyancy_component'
] as const;

function getBuildEnvironmentActionEnum(): readonly string[] {
  const tool = consolidatedToolDefinitions.find(def => def.name === 'build_environment');
  const inputSchema = tool?.inputSchema as { properties?: { action?: { enum?: string[] } } } | undefined;
  return inputSchema?.properties?.action?.enum ?? [];
}

function getBuildEnvironmentProperties(): Record<string, unknown> {
  const tool = consolidatedToolDefinitions.find(def => def.name === 'build_environment');
  const inputSchema = tool?.inputSchema as { properties?: Record<string, unknown> } | undefined;
  return inputSchema?.properties ?? {};
}

describe('handleEnvironmentTools path normalization', () => {
  beforeEach(() => {
    executeAutomationRequestMock.mockClear();
  });

  it('normalizes landscape material path aliases before dispatch', async () => {
    await handleEnvironmentTools('create_landscape', {
      action: 'create_landscape',
      name: 'TestLandscape',
      materialPath: 'Content/MCPTest/Materials/M_Landscape'
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'create_landscape',
      expect.objectContaining({
        materialPath: '/Game/MCPTest/Materials/M_Landscape'
      })
    );
  });

  it('normalizes foliage asset path aliases before dispatch', async () => {
    await handleEnvironmentTools('add_foliage', {
      action: 'add_foliage',
      name: 'TestFoliage',
      meshPath: 'Engine/BasicShapes/Sphere'
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'add_foliage_type',
      expect.objectContaining({
        meshPath: '/Engine/BasicShapes/Sphere'
      })
    );
  });

  it('normalizes existing foliage type aliases before dispatch', async () => {
    await handleEnvironmentTools('paint_foliage', {
      action: 'paint_foliage',
      foliageType: 'Game/Foliage/TestFoliage',
      locations: [{ x: 0, y: 0, z: 100 }]
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'paint_foliage',
      expect.objectContaining({
        foliageType: '/Game/Foliage/TestFoliage'
      })
    );
  });

  it('forwards bare foliage locations with their scale and yaw ranges', async () => {
    await handleEnvironmentTools('add_foliage_instances', {
      action: 'add_foliage_instances',
      foliageType: '/Game/Foliage/TestFoliage',
      locations: [{ x: 1, y: 2, z: 3 }],
      minScale: 0.8,
      maxScale: 1.4,
      randomYaw: true
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'add_foliage_instances',
      expect.objectContaining({
        locations: [{ x: 1, y: 2, z: 3 }],
        minScale: 0.8,
        maxScale: 1.4,
        randomYaw: true
      })
    );
  });

  it('forwards a paint area, count, surface snap and scale range under the names the plugin reads', async () => {
    const area = { min: { x: 0, y: -150, z: 0 }, max: { x: 500, y: -100, z: 0 } };
    await handleEnvironmentTools('paint_foliage', {
      action: 'paint_foliage',
      foliageType: '/Game/Foliage/TestFoliage',
      area, count: 12, radius: 300, density: 0.5, minScale: 1, maxScale: 2, randomYaw: true, snapToSurface: false
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'paint_foliage',
      expect.objectContaining({ area, count: 12, radius: 300, density: 0.5, minScale: 1, maxScale: 2, randomYaw: true, snapToSurface: false })
    );
  });

  it('normalizes procedural foliage nested mesh paths before dispatch', async () => {
    await handleEnvironmentTools('create_procedural_foliage', {
      action: 'create_procedural_foliage',
      volumeName: 'TestProceduralFoliage',
      foliageTypes: [{ meshPath: 'Content/Foliage/SM_Bush', density: 1 }]
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'create_procedural_foliage',
      expect.objectContaining({
        foliageTypes: [expect.objectContaining({ meshPath: '/Game/Foliage/SM_Bush' })],
        types: [expect.objectContaining({ meshPath: '/Game/Foliage/SM_Bush' })]
      })
    );
  });

  it('exposes every environment action on the build_environment schema', () => {
    // A folded family advertises one primary action; the names it folded stay
    // callable as its legacy pairs, so they are part of the exposed surface.
    const folded = ALL_CAPABILITY_RECORDS
      .filter((record) => String(record.routing.parentTool) === 'build_environment')
      .flatMap((record) => record.legacyIds.map((legacy) => String(legacy.action)));
    expect([...getBuildEnvironmentActionEnum(), ...folded]).toEqual(expect.arrayContaining([...ENVIRONMENT_ACTIONS]));
  });

  it('routes the create_foliage_type alias to foliage type creation', async () => {
    await handleEnvironmentTools('create_foliage_type', {
      action: 'create_foliage_type',
      name: 'EnvironmentFoliageType',
      foliageTypePath: 'Content/Foliage/EnvironmentFoliageType',
      meshPath: 'Engine/BasicShapes/Cone',
      path: 'Content/Foliage',
      density: 12
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'add_foliage_type',
      expect.objectContaining({
        name: 'EnvironmentFoliageType',
        foliageTypePath: '/Game/Foliage/EnvironmentFoliageType',
        meshPath: '/Engine/BasicShapes/Cone',
        path: '/Game/Foliage',
        density: 12
      })
    );
  });

  it('exposes water steepness and sky cubemap path on the build_environment schema', () => {
    expect(getBuildEnvironmentProperties()).toHaveProperty('cubemapPath');
    expect(getBuildEnvironmentProperties()).toHaveProperty('steepness');
  });

  it('exposes explicit snapshot light selectors', () => {
    expect(getBuildEnvironmentProperties()).toHaveProperty(
      'directionalLightActorPath',
    );
    expect(getBuildEnvironmentProperties()).toHaveProperty(
      'skyLightActorPath',
    );
  });

  it('preserves foliageTypePath for targeted remove_foliage_instances', async () => {
    await handleEnvironmentTools('remove_foliage_instances', {
      action: 'remove_foliage_instances',
      foliageTypePath: 'Game/Foliage/EnvironmentFoliageType'
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'build_environment',
      expect.objectContaining({
        action: 'remove_foliage_instances',
        foliageTypePath: '/Game/Foliage/EnvironmentFoliageType'
      }),
      'Automation bridge not available for environment building operations'
    );
  });

  it('exposes removeAll for explicit all-foliage removal', () => {
    expect(getBuildEnvironmentProperties()).toHaveProperty('removeAll');
  });

  it('forwards exact actor paths for safe environment deletion', async () => {
    await handleEnvironmentTools('delete', {
      action: 'delete',
      actorPath: '/Game/TestMap.TestMap:PersistentLevel.TargetActor',
      targetActor: 'TargetActor'
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'build_environment',
      expect.objectContaining({
        action: 'delete',
        actorPath: '/Game/TestMap.TestMap:PersistentLevel.TargetActor',
        targetActor: 'TargetActor'
      })
    );
  });

  it.each([
    ['import_heightmap', 'landscapeActorPath', 'Content/MCPTest/Landscape.Landscape', '/Game/MCPTest/Landscape.Landscape'],
    ['export_heightmap', 'landscapeActorPath', 'Content/MCPTest/Landscape.Landscape', '/Game/MCPTest/Landscape.Landscape'],
    ['configure_landscape_material', 'landscapeActorPath', 'Content/MCPTest/Landscape.Landscape', '/Game/MCPTest/Landscape.Landscape'],
    ['configure_landscape_splines', 'landscapeActorPath', 'Content/MCPTest/Landscape.Landscape', '/Game/MCPTest/Landscape.Landscape'],
    ['configure_sky_light', 'cubemapPath', 'Content/HDRI/T_SkyCubemap', '/Game/HDRI/T_SkyCubemap']
  ])('normalizes environment alias path field %s.%s before dispatch', async (action, fieldName, rawPath, normalizedPath) => {
    await handleEnvironmentTools(action, {
      action,
      [fieldName]: rawPath
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'build_environment',
      expect.objectContaining({
        action,
        [fieldName]: normalizedPath
      }),
      'Automation bridge not available for environment building operations'
    );
  });

  it.each([
    ['create_weather_system', 'particleSystemPath', 'Content/Weather/P_Weather', '/Game/Weather/P_Weather'],
    ['configure_rain_particles', 'particleSystemPath', 'Content/Weather/P_Rain', '/Game/Weather/P_Rain'],
    ['configure_snow_particles', 'particleSystemPath', 'Content/Weather/P_Snow', '/Game/Weather/P_Snow'],
    ['configure_lightning', 'particleSystemPath', 'Content/Weather/P_Lightning', '/Game/Weather/P_Lightning'],
    ['configure_light_color_curve', 'curvePath', 'Content/Environment/Curves/C_Light', '/Game/Environment/Curves/C_Light'],
    ['configure_sky_color_curve', 'curvePath', 'Content/Environment/Curves/C_Sky', '/Game/Environment/Curves/C_Sky']
  ])('normalizes environment asset path field %s.%s before dispatch', async (action, fieldName, rawPath, normalizedPath) => {
    await handleEnvironmentTools(action, {
      action,
      [fieldName]: rawPath
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'build_environment',
      expect.objectContaining({
        action,
        [fieldName]: normalizedPath
      }),
      'Automation bridge not available for environment building operations'
    );
  });

  it.each([
    'create_water_body_ocean',
    'create_water_body_lake',
    'create_water_body_river',
    'create_water_body_custom'
  ])('normalizes water body create material paths before dispatch for %s', async action => {
    await handleEnvironmentTools(action, {
      action,
      materialPath: 'Engine/BasicShapes/BasicShapeMaterial'
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'build_environment',
      expect.objectContaining({
        action,
        materialPath: '/Engine/BasicShapes/BasicShapeMaterial'
      }),
      'Automation bridge not available for environment building operations'
    );
  });

  it('normalizes generate_lods asset path arrays before dispatch', async () => {
    await handleEnvironmentTools('generate_lods', {
      action: 'generate_lods',
      assetPaths: ['Engine/BasicShapes/Sphere', 'Content/MCPTest/SM_Rock'],
      numLODs: 2
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'build_environment',
      expect.objectContaining({
        action: 'generate_lods',
        assetPaths: ['/Engine/BasicShapes/Sphere', '/Game/MCPTest/SM_Rock']
      }),
      'Automation bridge not available for environment building operations'
    );
  });

  it.each(['export_snapshot', 'import_snapshot'])(
    'routes %s through Unreal with filesystem snapshot paths preserved',
    async action => {
      await handleEnvironmentTools(action, {
        action,
        path: './tmp/unreal-mcp/build-environment',
        filename: 'snapshot.json'
      }, {} as never);

      expect(executeAutomationRequestMock).toHaveBeenCalledWith(
        {},
        'build_environment',
        {
          action,
          path: './tmp/unreal-mcp/build-environment',
          filename: 'snapshot.json'
        },
        'Automation bridge not available for environment snapshot operations'
      );
    }
  );

  it('forwards every supported single-actor deletion selector', async () => {
    await handleEnvironmentTools('delete', {
      action: 'delete',
      actorPath: '/Game/Test.Test:PersistentLevel.PathActor',
      actorName: 'NamedActor',
      targetActor: 'TargetActor'
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'build_environment',
      expect.objectContaining({
        action: 'delete',
        actorPath: '/Game/Test.Test:PersistentLevel.PathActor',
        actorName: 'NamedActor',
        targetActor: 'TargetActor'
      })
    );
  });
});
