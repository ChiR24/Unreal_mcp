import { beforeEach, describe, expect, it, vi } from 'vitest';

const { executeAutomationRequestMock, exportEnvironmentSnapshotMock, importEnvironmentSnapshotMock } = vi.hoisted(() => ({
  executeAutomationRequestMock: vi.fn(async () => ({ success: true, result: {} })),
  exportEnvironmentSnapshotMock: vi.fn(async () => ({ success: true })),
  importEnvironmentSnapshotMock: vi.fn(async () => ({ success: true }))
}));

vi.mock('./common-handlers.js', async () => {
  const actual = await vi.importActual<typeof import('./common-handlers.js')>('./common-handlers.js');
  return {
    ...actual,
    executeAutomationRequest: executeAutomationRequestMock
  };
});

vi.mock('../../utils/environment-snapshot.js', () => ({
  exportEnvironmentSnapshot: exportEnvironmentSnapshotMock,
  importEnvironmentSnapshot: importEnvironmentSnapshotMock
}));

import { handleEnvironmentTools } from './environment-handlers.js';
import { consolidatedToolDefinitions } from '../consolidated-tool-definitions.js';

const PHASE_28_ENVIRONMENT_ACTIONS = [
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
    exportEnvironmentSnapshotMock.mockClear();
    importEnvironmentSnapshotMock.mockClear();
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

  it('exposes every Phase 28 roadmap action on the build_environment schema', () => {
    expect(getBuildEnvironmentActionEnum()).toEqual(expect.arrayContaining([...PHASE_28_ENVIRONMENT_ACTIONS]));
  });

  it('routes the Phase 28 create_foliage_type alias to foliage type creation', async () => {
    await handleEnvironmentTools('create_foliage_type', {
      action: 'create_foliage_type',
      name: 'Phase28FoliageType',
      foliageTypePath: 'Content/Foliage/Phase28FoliageType',
      meshPath: 'Engine/BasicShapes/Cone',
      path: 'Content/Foliage',
      density: 12
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'add_foliage_type',
      expect.objectContaining({
        name: 'Phase28FoliageType',
        foliageTypePath: '/Game/Foliage/Phase28FoliageType',
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

  it('preserves foliageTypePath for targeted remove_foliage_instances', async () => {
    await handleEnvironmentTools('remove_foliage_instances', {
      action: 'remove_foliage_instances',
      foliageTypePath: 'Game/Foliage/Phase28FoliageType'
    }, {} as never);

    expect(executeAutomationRequestMock).toHaveBeenCalledWith(
      {},
      'build_environment',
      expect.objectContaining({
        action: 'remove_foliage_instances',
        foliageTypePath: '/Game/Foliage/Phase28FoliageType'
      }),
      'Automation bridge not available for environment building operations'
    );
  });

  it('exposes removeAll for explicit all-foliage removal', () => {
    expect(getBuildEnvironmentProperties()).toHaveProperty('removeAll');
  });

  it.each([
    ['import_heightmap', 'landscapeActorPath', 'Content/MCPTest/Landscape.Landscape', '/Game/MCPTest/Landscape.Landscape'],
    ['export_heightmap', 'landscapeActorPath', 'Content/MCPTest/Landscape.Landscape', '/Game/MCPTest/Landscape.Landscape'],
    ['configure_landscape_material', 'landscapeActorPath', 'Content/MCPTest/Landscape.Landscape', '/Game/MCPTest/Landscape.Landscape'],
    ['configure_landscape_splines', 'landscapeActorPath', 'Content/MCPTest/Landscape.Landscape', '/Game/MCPTest/Landscape.Landscape'],
    ['configure_sky_light', 'cubemapPath', 'Content/HDRI/T_SkyCubemap', '/Game/HDRI/T_SkyCubemap']
  ])('normalizes Phase 28 alias path field %s.%s before dispatch', async (action, fieldName, rawPath, normalizedPath) => {
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
  ])('normalizes Phase 28 asset path field %s.%s before dispatch', async (action, fieldName, rawPath, normalizedPath) => {
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

  it('preserves filesystem snapshot paths', async () => {
    await handleEnvironmentTools('export_snapshot', {
      action: 'export_snapshot',
      path: './tmp/unreal-mcp/build-environment',
      filename: 'snapshot.json'
    }, {} as never);

    expect(exportEnvironmentSnapshotMock).toHaveBeenCalledWith({
      path: './tmp/unreal-mcp/build-environment',
      filename: 'snapshot.json'
    });
  });
});
