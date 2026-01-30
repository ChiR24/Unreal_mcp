/**
 * Unit tests for environment-handlers
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { handleEnvironmentTools } from './environment-handlers.js';
import type { ITools } from '../../types/tool-interfaces.js';
import type { EnvironmentArgs } from '../../types/handler-types.js';

// Mock common-handlers
vi.mock('./common-handlers.js', () => ({
  executeAutomationRequest: vi.fn().mockResolvedValue({ success: true }),
}));

import { executeAutomationRequest } from './common-handlers.js';

function createMockTools(): ITools {
  return {
    landscapeTools: {
      createLandscape: vi.fn().mockResolvedValue({ success: true }),
      modifyHeightmap: vi.fn().mockResolvedValue({ success: true }),
      sculptLandscape: vi.fn().mockResolvedValue({ success: true }),
      createProceduralTerrain: vi.fn().mockResolvedValue({ success: true }),
      createLandscapeGrassType: vi.fn().mockResolvedValue({ success: true }),
      setLandscapeMaterial: vi.fn().mockResolvedValue({ success: true }),
    },
    foliageTools: {
      addFoliageType: vi.fn().mockResolvedValue({ success: true }),
      addFoliage: vi.fn().mockResolvedValue({ success: true }),
      addFoliageInstances: vi.fn().mockResolvedValue({ success: true }),
      paintFoliage: vi.fn().mockResolvedValue({ success: true }),
      createProceduralFoliage: vi.fn().mockResolvedValue({ success: true }),
    },
    lightingTools: {
      buildLighting: vi.fn().mockResolvedValue({ success: true }),
    },
    environmentTools: {
      exportSnapshot: vi.fn().mockResolvedValue({ success: true }),
      importSnapshot: vi.fn().mockResolvedValue({ success: true }),
      cleanup: vi.fn().mockResolvedValue({ success: true }),
    },
    automationBridge: {
      isConnected: vi.fn().mockReturnValue(true),
      sendAutomationRequest: vi.fn().mockResolvedValue({ success: true }),
    },
  } as unknown as ITools;
}

describe('handleEnvironmentTools', () => {
  let mockTools: ITools;

  beforeEach(() => {
    mockTools = createMockTools();
    vi.clearAllMocks();
  });

  describe('water actions', () => {
    it('routes create_water_body_ocean to manage_water', async () => {
      const result = await handleEnvironmentTools('create_water_body_ocean', { name: 'TestOcean' } as EnvironmentArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(executeAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_water',
        expect.objectContaining({ action: 'create_water_body_ocean' }),
        expect.any(String),
        expect.any(Object)
      );
    });

    it('routes create_water_body_lake to manage_water', async () => {
      const result = await handleEnvironmentTools('create_water_body_lake', { name: 'TestLake' } as EnvironmentArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(executeAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_water',
        expect.objectContaining({ action: 'create_water_body_lake' }),
        expect.any(String),
        expect.any(Object)
      );
    });

    it('routes configure_water_waves to manage_water', async () => {
      const result = await handleEnvironmentTools('configure_water_waves', { actorPath: '/Game/Water/Ocean' } as EnvironmentArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(executeAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_water',
        expect.objectContaining({ action: 'configure_water_waves' }),
        expect.any(String),
        expect.any(Object)
      );
    });
  });

  describe('weather actions', () => {
    it('routes configure_wind to manage_weather', async () => {
      const result = await handleEnvironmentTools('configure_wind', { speed: 10 } as EnvironmentArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(executeAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_weather',
        expect.objectContaining({ action: 'configure_wind' }),
        expect.any(String),
        expect.any(Object)
      );
    });

    it('routes create_weather_system to manage_weather', async () => {
      const result = await handleEnvironmentTools('create_weather_system', { name: 'TestWeather' } as EnvironmentArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(executeAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_weather',
        expect.objectContaining({ action: 'create_weather_system' }),
        expect.any(String),
        expect.any(Object)
      );
    });
  });

  describe('landscape actions', () => {
    it('creates landscape successfully', async () => {
      const result = await handleEnvironmentTools('create_landscape', { 
        name: 'TestLandscape', 
        location: { x: 0, y: 0, z: 0 } 
      } as EnvironmentArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(mockTools.landscapeTools.createLandscape).toHaveBeenCalled();
    });

    it('modifies heightmap successfully', async () => {
      const result = await handleEnvironmentTools('modify_heightmap', { 
        landscapeName: 'TestLandscape',
        heightData: [1, 2, 3],
        minX: 0,
        minY: 0,
        maxX: 10,
        maxY: 10
      } as unknown as EnvironmentArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(mockTools.landscapeTools.modifyHeightmap).toHaveBeenCalled();
    });

    it('sculpts landscape with default Raise tool', async () => {
      await handleEnvironmentTools('sculpt_landscape', { 
        landscapeName: 'TestLandscape',
        location: { x: 100, y: 100, z: 0 },
        radius: 500
      } as EnvironmentArgs, mockTools);
      
      expect(mockTools.landscapeTools.sculptLandscape).toHaveBeenCalledWith(
        expect.objectContaining({ tool: 'Raise' })
      );
    });

    it('sets landscape material', async () => {
      const result = await handleEnvironmentTools('set_landscape_material', { 
        landscapeName: 'TestLandscape',
        materialPath: '/Game/Materials/M_Landscape'
      } as EnvironmentArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(mockTools.landscapeTools.setLandscapeMaterial).toHaveBeenCalled();
    });
  });

  describe('foliage actions', () => {
    it('adds foliage type when meshPath is provided', async () => {
      const result = await handleEnvironmentTools('add_foliage', { 
        meshPath: '/Game/Meshes/SM_Tree',
        density: 0.5
      } as EnvironmentArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(mockTools.foliageTools.addFoliageType).toHaveBeenCalled();
    });

    it('adds foliage instances when foliageType and locations provided', async () => {
      const result = await handleEnvironmentTools('add_foliage', { 
        foliageType: '/Game/FoliageTypes/FT_Tree',
        locations: [{ x: 0, y: 0, z: 0 }]
      } as EnvironmentArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(mockTools.foliageTools.addFoliage).toHaveBeenCalled();
    });

    it('returns error when foliageType is missing and no meshPath', async () => {
      const result = await handleEnvironmentTools('add_foliage', {} as EnvironmentArgs, mockTools) as { success: boolean; error?: string };
      
      expect(result.success).toBe(false);
      expect(result.error).toBe('INVALID_ARGUMENT');
    });

    it('generates locations from location+radius when no explicit locations', async () => {
      const result = await handleEnvironmentTools('add_foliage', { 
        foliageType: '/Game/FoliageTypes/FT_Bush',
        location: { x: 100, y: 100, z: 0 },
        radius: 200,
        density: 5
      } as EnvironmentArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(mockTools.foliageTools.addFoliage).toHaveBeenCalledWith(
        expect.objectContaining({
          foliageType: '/Game/FoliageTypes/FT_Bush',
          locations: expect.any(Array)
        })
      );
    });

    it('paints foliage successfully', async () => {
      const result = await handleEnvironmentTools('paint_foliage', { 
        foliageType: '/Game/FoliageTypes/FT_Grass',
        location: { x: 0, y: 0, z: 0 },
        radius: 100
      } as EnvironmentArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(mockTools.foliageTools.paintFoliage).toHaveBeenCalled();
    });
  });

  describe('environment utility actions', () => {
    it('exports snapshot', async () => {
      const result = await handleEnvironmentTools('export_snapshot', { 
        path: '/Game/Snapshots',
        filename: 'test_snapshot'
      } as unknown as EnvironmentArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(mockTools.environmentTools.exportSnapshot).toHaveBeenCalled();
    });

    it('imports snapshot', async () => {
      const result = await handleEnvironmentTools('import_snapshot', { 
        path: '/Game/Snapshots',
        filename: 'test_snapshot'
      } as unknown as EnvironmentArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(mockTools.environmentTools.importSnapshot).toHaveBeenCalled();
    });

    it('deletes actors by name', async () => {
      const result = await handleEnvironmentTools('delete', { 
        name: 'ActorToDelete'
      } as EnvironmentArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(mockTools.environmentTools.cleanup).toHaveBeenCalledWith(
        expect.objectContaining({ names: expect.arrayContaining(['ActorToDelete']) })
      );
    });

    it('bakes lightmap', async () => {
      const result = await handleEnvironmentTools('bake_lightmap', { 
        quality: 'Production'
      } as unknown as EnvironmentArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(mockTools.lightingTools.buildLighting).toHaveBeenCalled();
    });
  });

  describe('path normalization', () => {
    it('normalizes /Content/ to /Game/ in water paths', async () => {
      await handleEnvironmentTools('configure_water_body', { 
        actorPath: '/Content/Water/TestBody',
        waterMaterial: '/Content/Materials/M_Water'
      } as unknown as EnvironmentArgs, mockTools);
      
      expect(executeAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_water',
        expect.objectContaining({ 
          actorPath: '/Game/Water/TestBody',
          waterMaterial: '/Game/Materials/M_Water'
        }),
        expect.any(String),
        expect.any(Object)
      );
    });

    it('adds /Game/ prefix to relative paths', async () => {
      await handleEnvironmentTools('set_river_depth', { 
        actorPath: 'Rivers/MyRiver',
        splineKey: 0.5
      } as unknown as EnvironmentArgs, mockTools);
      
      expect(executeAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_water',
        expect.objectContaining({ 
          actorPath: '/Game/Rivers/MyRiver'
        }),
        expect.any(String),
        expect.any(Object)
      );
    });
  });

  describe('default action handling', () => {
    it('routes unknown actions to build_environment', async () => {
      const result = await handleEnvironmentTools('custom_environment_action', { 
        someParam: 'value'
      } as unknown as EnvironmentArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(executeAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'build_environment',
        expect.any(Object),
        expect.stringContaining('environment building')
      );
    });
  });

  describe('automation failure', () => {
    it('propagates errors from landscapeTools', async () => {
      vi.mocked(mockTools.landscapeTools.createLandscape).mockRejectedValue(new Error('Landscape creation failed'));
      
      await expect(
        handleEnvironmentTools('create_landscape', { name: 'Test' } as EnvironmentArgs, mockTools)
      ).rejects.toThrow('Landscape creation failed');
    });

    it('propagates errors from foliageTools', async () => {
      vi.mocked(mockTools.foliageTools.addFoliageType).mockRejectedValue(new Error('Foliage error'));
      
      await expect(
        handleEnvironmentTools('add_foliage', { meshPath: '/Game/SM_Tree' } as EnvironmentArgs, mockTools)
      ).rejects.toThrow('Foliage error');
    });
  });
});
