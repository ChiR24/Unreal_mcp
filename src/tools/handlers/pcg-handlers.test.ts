/**
 * Unit tests for pcg-handlers.ts
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { handlePCGTools } from './pcg-handlers.js';
import type { ITools } from '../../types/tool-interfaces.js';

vi.mock('./common-handlers.js', () => ({
  executeAutomationRequest: vi.fn(),
}));

import { executeAutomationRequest } from './common-handlers.js';
const mockedExecuteAutomationRequest = vi.mocked(executeAutomationRequest);

function createMockTools(): ITools {
  return {
    automationBridge: { isConnected: () => true, sendAutomationRequest: vi.fn() },
    actorTools: {},
    assetTools: {},
    blueprintTools: {},
    levelTools: {},
    editorTools: {},
  } as unknown as ITools;
}

describe('handlePCGTools', () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  describe('graph management actions', () => {
    it('handles create_pcg_graph action', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({ success: true, graphPath: '/Game/PCG/TestGraph' });
      const result = await handlePCGTools('create_pcg_graph', { graphPath: '/Game/PCG/TestGraph' }, mockTools);
      expect(result).toHaveProperty('success', true);
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_pcg',
        expect.objectContaining({ subAction: 'create_pcg_graph' }),
        expect.any(String),
        expect.any(Object)
      );
    });

    it('handles add_pcg_node action', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({ success: true, nodeId: 'Node_001' });
      const result = await handlePCGTools('add_pcg_node', { graphPath: '/Game/PCG/Graph', nodeType: 'SurfaceSampler' }, mockTools);
      expect(result).toHaveProperty('success', true);
    });

    it('handles connect_pcg_pins action', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({ success: true });
      const result = await handlePCGTools('connect_pcg_pins', { graphPath: '/Game/PCG/Graph', sourceNode: 'A', targetNode: 'B' }, mockTools);
      expect(result).toHaveProperty('success', true);
    });
  });

  describe('input node actions', () => {
    it('handles add_landscape_data_node action', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({ success: true });
      const result = await handlePCGTools('add_landscape_data_node', { graphPath: '/Game/PCG/Graph' }, mockTools);
      expect(result).toHaveProperty('success', true);
    });

    it('handles add_spline_data_node action', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({ success: true });
      const result = await handlePCGTools('add_spline_data_node', { graphPath: '/Game/PCG/Graph', splinePath: '/Game/Spline' }, mockTools);
      expect(result).toHaveProperty('success', true);
    });
  });

  describe('sampler actions', () => {
    it('handles add_surface_sampler action', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({ success: true, nodeId: 'Sampler_001' });
      const result = await handlePCGTools('add_surface_sampler', { graphPath: '/Game/PCG/Graph' }, mockTools);
      expect(result).toHaveProperty('success', true);
    });
  });

  describe('spawner actions', () => {
    it('handles add_static_mesh_spawner action', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({ success: true });
      const result = await handlePCGTools('add_static_mesh_spawner', { graphPath: '/Game/PCG/Graph', meshPath: '/Game/Meshes/Rock' }, mockTools);
      expect(result).toHaveProperty('success', true);
    });
  });

  describe('execution actions', () => {
    it('handles execute_pcg_graph action', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({ success: true, executionTime: 0.5 });
      const result = await handlePCGTools('execute_pcg_graph', { graphPath: '/Game/PCG/Graph' }, mockTools);
      expect(result).toHaveProperty('success', true);
    });
  });

  describe('path normalization', () => {
    it('normalizes /Content/ paths to /Game/', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({ success: true });
      await handlePCGTools('create_pcg_graph', { graphPath: '/Content/PCG/TestGraph' }, mockTools);
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_pcg',
        expect.objectContaining({ graphPath: '/Game/PCG/TestGraph' }),
        expect.any(String),
        expect.any(Object)
      );
    });

    it('adds /Game/ prefix to relative paths', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockResolvedValue({ success: true });
      await handlePCGTools('create_pcg_graph', { graphPath: 'PCG/TestGraph' }, mockTools);
      expect(mockedExecuteAutomationRequest).toHaveBeenCalledWith(
        mockTools,
        'manage_pcg',
        expect.objectContaining({ graphPath: '/Game/PCG/TestGraph' }),
        expect.any(String),
        expect.any(Object)
      );
    });
  });

  describe('automation failures', () => {
    it('rejects when bridge fails', async () => {
      const mockTools = createMockTools();
      mockedExecuteAutomationRequest.mockRejectedValue(new Error('Bridge unavailable'));
      await expect(handlePCGTools('create_pcg_graph', { graphPath: '/Game/PCG/Graph' }, mockTools))
        .rejects.toThrow('Bridge unavailable');
    });
  });

  describe('unknown action', () => {
    it('returns error for unknown action', async () => {
      const mockTools = createMockTools();
      const result = await handlePCGTools('nonexistent_action', {}, mockTools);
      expect(result).toHaveProperty('success', false);
      expect(result).toHaveProperty('error', 'UNKNOWN_ACTION');
    });
  });
});
