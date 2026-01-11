/**
 * Unit tests for data-handlers.ts
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { handleDataTools } from './data-handlers.js';
import type { ITools } from '../../types/tool-interfaces.js';

// Mock common-handlers module
vi.mock('./common-handlers.js', () => ({
  executeAutomationRequest: vi.fn(),
}));

// Create mock tools object
const mockTools = {
  automationBridge: { isConnected: () => true },
} as unknown as ITools;

describe('handleDataTools', () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  // =========================================
  // SUCCESS PATH TESTS (3+)
  // =========================================

  it('should handle create_data_asset successfully', async () => {
    const { executeAutomationRequest } = await import('./common-handlers.js');
    vi.mocked(executeAutomationRequest).mockResolvedValue({
      success: true,
      assetPath: '/Game/Data/MyDataAsset',
    });

    const result = await handleDataTools(
      'create_data_asset',
      { name: 'MyDataAsset', path: '/Game/Data' },
      mockTools
    );

    expect(result).toEqual({ success: true, assetPath: '/Game/Data/MyDataAsset' });
    expect(executeAutomationRequest).toHaveBeenCalledWith(
      mockTools,
      'manage_data',
      expect.objectContaining({ action_type: 'create_data_asset' }),
      expect.any(String)
    );
  });

  it('should handle create_data_table successfully', async () => {
    const { executeAutomationRequest } = await import('./common-handlers.js');
    vi.mocked(executeAutomationRequest).mockResolvedValue({
      success: true,
      tablePath: '/Game/Data/MyTable',
    });

    const result = await handleDataTools(
      'create_data_table',
      { name: 'MyTable', rowStruct: 'FMyRowStruct' },
      mockTools
    );

    expect(result).toEqual({ success: true, tablePath: '/Game/Data/MyTable' });
    expect(executeAutomationRequest).toHaveBeenCalled();
  });

  it('should handle save_game_to_slot successfully', async () => {
    const { executeAutomationRequest } = await import('./common-handlers.js');
    vi.mocked(executeAutomationRequest).mockResolvedValue({
      success: true,
      slotName: 'SaveSlot1',
    });

    const result = await handleDataTools(
      'save_game_to_slot',
      { slotName: 'SaveSlot1', userIndex: 0 },
      mockTools
    );

    expect(result).toEqual({ success: true, slotName: 'SaveSlot1' });
  });

  it('should handle get_data_table_rows successfully', async () => {
    const { executeAutomationRequest } = await import('./common-handlers.js');
    vi.mocked(executeAutomationRequest).mockResolvedValue({
      success: true,
      rows: [{ name: 'Row1' }, { name: 'Row2' }],
    });

    const result = await handleDataTools(
      'get_data_table_rows',
      { tablePath: '/Game/Data/MyTable' },
      mockTools
    );

    expect(result).toEqual({
      success: true,
      rows: [{ name: 'Row1' }, { name: 'Row2' }],
    });
  });

  // =========================================
  // AUTOMATION FAILURE TESTS (2+)
  // =========================================

  it('should propagate automation failure for create_data_asset', async () => {
    const { executeAutomationRequest } = await import('./common-handlers.js');
    vi.mocked(executeAutomationRequest).mockRejectedValue(
      new Error('Automation bridge not available for create_data_asset')
    );

    await expect(handleDataTools('create_data_asset', { name: 'Test' }, mockTools))
      .rejects.toThrow('Automation bridge not available');
  });

  it('should propagate automation failure for save_game_to_slot', async () => {
    const { executeAutomationRequest } = await import('./common-handlers.js');
    vi.mocked(executeAutomationRequest).mockRejectedValue(
      new Error('Bridge connection timeout')
    );

    await expect(handleDataTools('save_game_to_slot', { slotName: 'Test' }, mockTools))
      .rejects.toThrow('Bridge connection timeout');
  });

  it('should propagate automation failure for create_data_table', async () => {
    const { executeAutomationRequest } = await import('./common-handlers.js');
    vi.mocked(executeAutomationRequest).mockRejectedValue(
      new Error('Network error')
    );

    await expect(handleDataTools('create_data_table', {}, mockTools))
      .rejects.toThrow('Network error');
  });

  // =========================================
  // INVALID ACTION TESTS (3+)
  // =========================================

  it('should return error for unknown action', async () => {
    const result = await handleDataTools('invalid_action_xyz', {}, mockTools);

    expect(result).toMatchObject({
      success: false,
      message: expect.stringMatching(/unknown/i),
    });
  });

  it('should return error for empty action', async () => {
    const result = await handleDataTools('', {}, mockTools);

    expect(result).toMatchObject({
      success: false,
      message: expect.stringMatching(/unknown/i),
    });
  });

  it('should return error for action with typo', async () => {
    const result = await handleDataTools('create_datatable', {}, mockTools); // typo: no underscore

    expect(result).toMatchObject({
      success: false,
      message: expect.stringMatching(/unknown/i),
    });
  });

  it('should include UNKNOWN_ACTION error code for unknown action', async () => {
    const result = await handleDataTools('not_a_real_action', {}, mockTools);

    expect(result).toMatchObject({
      success: false,
      error: 'UNKNOWN_ACTION',
    });
  });
});
