/**
 * Unit tests for lighting-handlers
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { handleLightingTools } from './lighting-handlers.js';
import type { ITools } from '../../types/tool-interfaces.js';
import type { LightingArgs } from '../../types/handler-types.js';

function createMockTools(): ITools {
  return {
    lightingTools: {
      createPointLight: vi.fn().mockResolvedValue({ success: true }),
      createDirectionalLight: vi.fn().mockResolvedValue({ success: true }),
      createSpotLight: vi.fn().mockResolvedValue({ success: true }),
      createRectLight: vi.fn().mockResolvedValue({ success: true }),
      createSkyLight: vi.fn().mockResolvedValue({ success: true }),
      createDynamicLight: vi.fn().mockResolvedValue({ success: true }),
      ensureSingleSkyLight: vi.fn().mockResolvedValue({ success: true }),
      createLightmassVolume: vi.fn().mockResolvedValue({ success: true }),
      setupVolumetricFog: vi.fn().mockResolvedValue({ success: true }),
      setupGlobalIllumination: vi.fn().mockResolvedValue({ success: true }),
      configureShadows: vi.fn().mockResolvedValue({ success: true }),
      setExposure: vi.fn().mockResolvedValue({ success: true }),
      setAmbientOcclusion: vi.fn().mockResolvedValue({ success: true }),
      buildLighting: vi.fn().mockResolvedValue({ success: true }),
      createLightingEnabledLevel: vi.fn().mockResolvedValue({ success: true }),
      listLightTypes: vi.fn().mockResolvedValue({ success: true, types: [] }),
    },
    automationBridge: {
      isConnected: vi.fn().mockReturnValue(true),
      sendAutomationRequest: vi.fn().mockResolvedValue({ success: true }),
    },
  } as unknown as ITools;
}

describe('handleLightingTools', () => {
  let mockTools: ITools;

  beforeEach(() => {
    mockTools = createMockTools();
  });

  describe('spawn_light / create_light', () => {
    it('creates point light by default', async () => {
      const result = await handleLightingTools('spawn_light', { location: { x: 0, y: 0, z: 100 } } as LightingArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(mockTools.lightingTools.createPointLight).toHaveBeenCalled();
    });

    it('creates directional light when specified', async () => {
      const result = await handleLightingTools('create_light', { lightType: 'directional' } as LightingArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(mockTools.lightingTools.createDirectionalLight).toHaveBeenCalled();
    });

    it('creates spot light when specified', async () => {
      const result = await handleLightingTools('create_light', { lightType: 'spot' } as LightingArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(mockTools.lightingTools.createSpotLight).toHaveBeenCalled();
    });

    it('creates rect light when specified', async () => {
      const result = await handleLightingTools('create_light', { lightType: 'rect' } as LightingArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(mockTools.lightingTools.createRectLight).toHaveBeenCalled();
    });

    it('returns error for invalid light type', async () => {
      const result = await handleLightingTools('create_light', { lightType: 'invalid_type' } as LightingArgs, mockTools) as { success: boolean; error?: string };
      
      expect(result.success).toBe(false);
      expect(result.error).toBe('INVALID_LIGHT_TYPE');
    });

    it('normalizes location from object format', async () => {
      await handleLightingTools('spawn_light', { location: { x: 100, y: 200, z: 300 } } as unknown as LightingArgs, mockTools);
      
      expect(mockTools.lightingTools.createPointLight).toHaveBeenCalledWith(
        expect.objectContaining({
          location: [100, 200, 300]
        })
      );
    });
  });

  describe('create_sky_light', () => {
    it('creates sky light successfully', async () => {
      const result = await handleLightingTools('create_sky_light', { name: 'MySkyLight' } as LightingArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(mockTools.lightingTools.createSkyLight).toHaveBeenCalled();
    });

    it('handles spawn_sky_light alias', async () => {
      const result = await handleLightingTools('spawn_sky_light', {} as LightingArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
    });
  });

  describe('create_dynamic_light', () => {
    it('creates dynamic light successfully', async () => {
      const result = await handleLightingTools('create_dynamic_light', { name: 'DynLight' } as LightingArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(mockTools.lightingTools.createDynamicLight).toHaveBeenCalled();
    });
  });

  describe('ensure_single_sky_light', () => {
    it('ensures single sky light', async () => {
      const result = await handleLightingTools('ensure_single_sky_light', { name: 'MainSkyLight' } as LightingArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(mockTools.lightingTools.ensureSingleSkyLight).toHaveBeenCalled();
    });
  });

  describe('create_lightmass_volume', () => {
    it('creates lightmass volume', async () => {
      const result = await handleLightingTools('create_lightmass_volume', { name: 'LMV', location: { x: 0, y: 0, z: 0 } } as LightingArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(mockTools.lightingTools.createLightmassVolume).toHaveBeenCalled();
    });
  });

  describe('setup_volumetric_fog', () => {
    it('sets up volumetric fog with defaults', async () => {
      const result = await handleLightingTools('setup_volumetric_fog', {} as LightingArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(mockTools.lightingTools.setupVolumetricFog).toHaveBeenCalledWith(
        expect.objectContaining({ enabled: true })
      );
    });

    it('disables fog when enabled is false', async () => {
      await handleLightingTools('setup_volumetric_fog', { enabled: false } as LightingArgs, mockTools);
      
      expect(mockTools.lightingTools.setupVolumetricFog).toHaveBeenCalledWith(
        expect.objectContaining({ enabled: false })
      );
    });
  });

  describe('setup_global_illumination', () => {
    it('sets up GI successfully', async () => {
      const result = await handleLightingTools('setup_global_illumination', { method: 'lumen' } as LightingArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(mockTools.lightingTools.setupGlobalIllumination).toHaveBeenCalled();
    });

    it('returns error for invalid GI method', async () => {
      const result = await handleLightingTools('setup_global_illumination', { method: 'invalid_method' } as LightingArgs, mockTools) as { success: boolean; error?: string };
      
      expect(result.success).toBe(false);
      expect(result.error).toBe('INVALID_GI_METHOD');
    });

    it('accepts valid GI methods', async () => {
      for (const method of ['lumen', 'screenspace', 'none', 'raytraced', 'ssgi']) {
        const result = await handleLightingTools('setup_global_illumination', { method } as LightingArgs, mockTools);
        expect(result).toEqual(expect.objectContaining({ success: true }));
      }
    });
  });

  describe('configure_shadows', () => {
    it('configures shadows successfully', async () => {
      const result = await handleLightingTools('configure_shadows', { shadowQuality: 'Epic' } as LightingArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(mockTools.lightingTools.configureShadows).toHaveBeenCalled();
    });
  });

  describe('set_exposure', () => {
    it('sets exposure successfully', async () => {
      const result = await handleLightingTools('set_exposure', { method: 'Manual', compensationValue: 1.5 } as LightingArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(mockTools.lightingTools.setExposure).toHaveBeenCalled();
    });
  });

  describe('set_ambient_occlusion', () => {
    it('enables AO by default', async () => {
      const result = await handleLightingTools('set_ambient_occlusion', {} as LightingArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(mockTools.lightingTools.setAmbientOcclusion).toHaveBeenCalledWith(
        expect.objectContaining({ enabled: true })
      );
    });
  });

  describe('build_lighting', () => {
    it('builds lighting successfully', async () => {
      const result = await handleLightingTools('build_lighting', { quality: 'Production' } as LightingArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(mockTools.lightingTools.buildLighting).toHaveBeenCalled();
    });
  });

  describe('create_lighting_enabled_level', () => {
    it('creates lighting enabled level', async () => {
      const result = await handleLightingTools('create_lighting_enabled_level', { levelName: 'TestLevel' } as LightingArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(mockTools.lightingTools.createLightingEnabledLevel).toHaveBeenCalled();
    });
  });

  describe('list_light_types', () => {
    it('lists light types', async () => {
      const result = await handleLightingTools('list_light_types', {} as LightingArgs, mockTools);
      
      expect(result).toEqual(expect.objectContaining({ success: true }));
      expect(mockTools.lightingTools.listLightTypes).toHaveBeenCalled();
    });
  });

  describe('unknown action', () => {
    it('throws for unknown action', async () => {
      await expect(
        handleLightingTools('unknown_action', {} as LightingArgs, mockTools)
      ).rejects.toThrow('Unknown lighting action: unknown_action');
    });
  });

  describe('automation failure', () => {
    it('propagates errors from lightingTools', async () => {
      vi.mocked(mockTools.lightingTools.createPointLight).mockRejectedValue(new Error('Lighting error'));
      
      await expect(
        handleLightingTools('spawn_light', {} as LightingArgs, mockTools)
      ).rejects.toThrow('Lighting error');
    });
  });
});
