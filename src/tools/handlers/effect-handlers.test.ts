/**
 * Unit tests for effect-handlers.ts
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { handleEffectTools } from './effect-handlers.js';
import type { ITools } from '../../types/tool-interfaces.js';

vi.mock('./common-handlers.js', () => ({
  executeAutomationRequest: vi.fn(),
  requireNonEmptyString: vi.fn((value, _p, msg) => {
    if (typeof value !== 'string' || !value.trim()) throw new Error(msg);
  }),
}));

import { executeAutomationRequest, requireNonEmptyString } from './common-handlers.js';
const mockedExecuteAutomationRequest = vi.mocked(executeAutomationRequest);
const mockedRequireNonEmptyString = vi.mocked(requireNonEmptyString);

function createMockTools(): ITools {
  return {
    automationBridge: {
      isConnected: vi.fn().mockReturnValue(true),
      sendAutomationRequest: vi.fn().mockResolvedValue({ success: true }),
    },
    niagaraTools: {
      createSystem: vi.fn().mockResolvedValue({ success: true, systemPath: '/Game/Effects/NS_Test' }),
      createEmitter: vi.fn().mockResolvedValue({ success: true, emitterPath: '/Game/Effects/NE_Test' }),
    },
    actorTools: {},
    assetTools: {},
    blueprintTools: {},
    levelTools: {},
    editorTools: {},
  } as unknown as ITools;
}

describe('handleEffectTools', () => {
  let mockTools: ITools;

  beforeEach(() => {
    vi.clearAllMocks();
    mockTools = createMockTools();
    mockedExecuteAutomationRequest.mockResolvedValue({ success: true });
  });

  describe('niagara system creation', () => {
    it('creates niagara system via niagaraTools', async () => {
      const result = await handleEffectTools('create_niagara_system', { name: 'NS_Test' }, mockTools);
      
      expect(result).toHaveProperty('success', true);
      expect(mockTools.niagaraTools.createSystem).toHaveBeenCalledWith(
        expect.objectContaining({ name: 'NS_Test' })
      );
    });

    it('passes savePath and template to createSystem', async () => {
      await handleEffectTools('create_niagara_system', {
        name: 'NS_Test',
        savePath: '/Game/Effects',
        template: 'Fountain'
      }, mockTools);
      
      expect(mockTools.niagaraTools.createSystem).toHaveBeenCalledWith({
        name: 'NS_Test',
        savePath: '/Game/Effects',
        template: 'Fountain'
      });
    });
  });

  describe('niagara emitter creation', () => {
    it('creates niagara emitter via niagaraTools', async () => {
      const result = await handleEffectTools('create_niagara_emitter', {
        name: 'NE_Test',
        systemPath: '/Game/Effects/NS_Test'
      }, mockTools);
      
      expect(result).toHaveProperty('success', true);
      expect(mockTools.niagaraTools.createEmitter).toHaveBeenCalled();
    });
  });

  describe('debug shapes', () => {
    it('handles debug_shape action', async () => {
      const result = await handleEffectTools('debug_shape', {
        shapeType: 'Sphere',
        location: { x: 0, y: 0, z: 100 }
      }, mockTools);
      
      expect(result).toHaveProperty('success', true);
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'create_effect',
        expect.objectContaining({ action: 'debug_shape', subAction: 'debug_shape' })
      );
    });

    it('maps shape to shapeType', async () => {
      await handleEffectTools('debug_shape', {
        shape: 'Box',
        location: { x: 0, y: 0, z: 0 }
      }, mockTools);
      
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'create_effect',
        expect.objectContaining({ shapeType: 'Box' })
      );
    });

    it('throws when shapeType is missing', async () => {
      await expect(
        handleEffectTools('debug_shape', { location: { x: 0, y: 0, z: 0 } }, mockTools)
      ).rejects.toThrow('Missing required parameter: shapeType');
    });

    it('clears debug shapes', async () => {
      const result = await handleEffectTools('clear_debug_shapes', {}, mockTools);
      
      expect(result).toHaveProperty('success', true);
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'clear_debug_shapes',
        expect.any(Object)
      );
    });

    it('lists debug shapes', async () => {
      const result = await handleEffectTools('list_debug_shapes', {}, mockTools);
      
      expect(result).toHaveProperty('success', true);
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'list_debug_shapes',
        expect.any(Object)
      );
    });
  });

  describe('advanced niagara actions', () => {
    it('handles create_niagara_module', async () => {
      const result = await handleEffectTools('create_niagara_module', {
        moduleName: 'TestModule'
      }, mockTools);
      
      expect(result).toHaveProperty('success', true);
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_niagara_advanced',
        expect.objectContaining({ subAction: 'create_niagara_module' }),
        expect.any(String)
      );
    });

    it('handles setup_niagara_fluids', async () => {
      const result = await handleEffectTools('setup_niagara_fluids', {
        systemPath: '/Game/Effects/NS_Water'
      }, mockTools);
      
      expect(result).toHaveProperty('success', true);
    });

    it('maps create_fluid_simulation to setup_niagara_fluids', async () => {
      await handleEffectTools('create_fluid_simulation', {}, mockTools);
      
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_niagara_advanced',
        expect.objectContaining({ subAction: 'setup_niagara_fluids' }),
        expect.any(String)
      );
    });
  });

  describe('effect creation actions', () => {
    it('handles create_volumetric_fog', async () => {
      const result = await handleEffectTools('create_volumetric_fog', {
        density: 0.5
      }, mockTools);
      
      expect(result).toHaveProperty('success', true);
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'create_effect',
        expect.objectContaining({ action: 'create_volumetric_fog' })
      );
    });

    it('handles create_particle_trail', async () => {
      const result = await handleEffectTools('create_particle_trail', {
        name: 'Trail_Test'
      }, mockTools);
      
      expect(result).toHaveProperty('success', true);
    });

    it('handles cleanup action', async () => {
      const result = await handleEffectTools('cleanup', {}, mockTools);
      
      expect(result).toHaveProperty('success', true);
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'create_effect',
        expect.objectContaining({ action: 'cleanup', subAction: 'cleanup' })
      );
    });
  });

  describe('simulation control', () => {
    it('handles activate action', async () => {
      const result = await handleEffectTools('activate', {
        systemName: 'NS_Test'
      }, mockTools);
      
      expect(result).toHaveProperty('success', true);
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'create_effect',
        expect.objectContaining({ action: 'activate_niagara', reset: true })
      );
    });

    it('handles deactivate action', async () => {
      const result = await handleEffectTools('deactivate', {
        systemName: 'NS_Test'
      }, mockTools);
      
      expect(result).toHaveProperty('success', true);
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'create_effect',
        expect.objectContaining({ action: 'deactivate_niagara' })
      );
    });

    it('handles advance_simulation action', async () => {
      const result = await handleEffectTools('advance_simulation', {
        systemName: 'NS_Test',
        deltaTime: 0.016
      }, mockTools);
      
      expect(result).toHaveProperty('success', true);
    });

    it('uses actorName as systemName fallback', async () => {
      await handleEffectTools('activate', {
        actorName: 'MyNiagaraActor'
      }, mockTools);
      
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'create_effect',
        expect.objectContaining({ systemName: 'MyNiagaraActor' })
      );
    });

    it('throws when systemName is missing for activate', async () => {
      await expect(
        handleEffectTools('activate', {}, mockTools)
      ).rejects.toThrow('Missing required parameter: systemName');
    });
  });

  describe('set_niagara_parameter', () => {
    it('sets niagara parameter', async () => {
      const result = await handleEffectTools('set_niagara_parameter', {
        systemName: 'NS_Test',
        parameterName: 'SpawnRate',
        parameterType: 'Float',
        parameterValue: 100
      }, mockTools);
      
      expect(result).toHaveProperty('success', true);
    });

    it('maps type to parameterType', async () => {
      await handleEffectTools('set_niagara_parameter', {
        systemName: 'NS_Test',
        parameterName: 'SpawnRate',
        type: 'float',
        parameterValue: 100
      }, mockTools);
      
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'create_effect',
        expect.objectContaining({ parameterType: 'Float' })
      );
    });

    it('throws when parameterName is missing', async () => {
      await expect(
        handleEffectTools('set_niagara_parameter', {
          systemName: 'NS_Test',
          parameterType: 'Float'
        }, mockTools)
      ).rejects.toThrow('Missing required parameter: parameterName');
    });
  });

  describe('fallback handling', () => {
    it('handles unknown actions via create_effect', async () => {
      const result = await handleEffectTools('some_other_effect', {
        customArg: 'value'
      }, mockTools);
      
      expect(result).toHaveProperty('success', true);
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'create_effect',
        expect.any(Object),
        expect.any(String)
      );
    });
  });

  describe('error handling', () => {
    it('handles SYSTEM_NOT_FOUND for niagara action', async () => {
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: false,
        error: 'SYSTEM_NOT_FOUND',
        message: 'Niagara system not found'
      });
      
      const result = await handleEffectTools('niagara', {
        systemPath: '/Game/Effects/NS_Missing'
      }, mockTools) as { success: boolean; error?: string; handled?: boolean };
      
      expect(result.success).toBe(false);
      expect(result.error).toBe('SYSTEM_NOT_FOUND');
      expect(result.handled).toBe(true);
    });

    it('suggests corrected path for spawn_niagara', async () => {
      mockedExecuteAutomationRequest.mockResolvedValue({
        success: false,
        error: 'SYSTEM_NOT_FOUND'
      });
      
      const result = await handleEffectTools('spawn_niagara', {
        systemPath: '/Game/Effects/NS_Test'
      }, mockTools) as { success: boolean; message?: string };
      
      expect(result.success).toBe(false);
      expect(result.message).toContain('Did you mean');
    });

    it('propagates bridge errors', async () => {
      mockedExecuteAutomationRequest.mockRejectedValue(new Error('Bridge unavailable'));
      
      await expect(
        handleEffectTools('create_volumetric_fog', {}, mockTools)
      ).rejects.toThrow('Bridge unavailable');
    });

    it('handles niagaraTools errors', async () => {
      vi.mocked(mockTools.niagaraTools.createSystem).mockRejectedValue(new Error('Creation failed'));
      
      await expect(
        handleEffectTools('create_niagara_system', { name: 'NS_Test' }, mockTools)
      ).rejects.toThrow('Creation failed');
    });
  });
});
