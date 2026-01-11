/**
 * Unit tests for modding-handlers.ts
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { handleModdingTools } from './modding-handlers.js';
import type { ITools } from '../../types/tool-interfaces.js';

// Mock common-handlers module
vi.mock('./common-handlers.js', () => ({
  executeAutomationRequest: vi.fn(),
}));

// Create mock tools object
const mockTools = {
  automationBridge: { isConnected: () => true },
} as unknown as ITools;

describe('handleModdingTools', () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  // =========================================
  // SUCCESS PATH TESTS (3+)
  // =========================================

  it('should handle configure_mod_loading_paths successfully', async () => {
    const { executeAutomationRequest } = await import('./common-handlers.js');
    vi.mocked(executeAutomationRequest).mockResolvedValue({ success: true, paths: ['/Mods'] });

    const result = await handleModdingTools(
      'configure_mod_loading_paths',
      { paths: ['/Mods'] },
      mockTools
    );

    expect(result).toEqual({ success: true, paths: ['/Mods'] });
    expect(executeAutomationRequest).toHaveBeenCalledWith(
      mockTools,
      'manage_modding',
      expect.objectContaining({ action_type: 'configure_mod_loading_paths' }),
      expect.any(String)
    );
  });

  it('should handle scan_for_mod_paks successfully', async () => {
    const { executeAutomationRequest } = await import('./common-handlers.js');
    vi.mocked(executeAutomationRequest).mockResolvedValue({ success: true, paks: ['mod1.pak', 'mod2.pak'] });

    const result = await handleModdingTools(
      'scan_for_mod_paks',
      { directory: '/Mods' },
      mockTools
    );

    expect(result).toEqual({ success: true, paks: ['mod1.pak', 'mod2.pak'] });
    expect(executeAutomationRequest).toHaveBeenCalled();
  });

  it('should handle load_mod_pak successfully', async () => {
    const { executeAutomationRequest } = await import('./common-handlers.js');
    vi.mocked(executeAutomationRequest).mockResolvedValue({ success: true, loaded: true });

    const result = await handleModdingTools(
      'load_mod_pak',
      { pakPath: '/Mods/test.pak' },
      mockTools
    );

    expect(result).toEqual({ success: true, loaded: true });
  });

  it('should handle list_installed_mods successfully', async () => {
    const { executeAutomationRequest } = await import('./common-handlers.js');
    vi.mocked(executeAutomationRequest).mockResolvedValue({ success: true, mods: ['ModA', 'ModB'] });

    const result = await handleModdingTools('list_installed_mods', {}, mockTools);

    expect(result).toEqual({ success: true, mods: ['ModA', 'ModB'] });
  });

  // =========================================
  // AUTOMATION FAILURE TESTS (2+)
  // =========================================

  it('should propagate automation failure for configure_mod_loading_paths', async () => {
    const { executeAutomationRequest } = await import('./common-handlers.js');
    vi.mocked(executeAutomationRequest).mockRejectedValue(new Error('Automation bridge not available'));

    await expect(handleModdingTools('configure_mod_loading_paths', {}, mockTools))
      .rejects.toThrow('Automation bridge not available');
  });

  it('should propagate automation failure for load_mod_pak', async () => {
    const { executeAutomationRequest } = await import('./common-handlers.js');
    vi.mocked(executeAutomationRequest).mockRejectedValue(new Error('Bridge connection lost'));

    await expect(handleModdingTools('load_mod_pak', { pakPath: '/test.pak' }, mockTools))
      .rejects.toThrow('Bridge connection lost');
  });

  it('should propagate automation failure for validate_mod_pak', async () => {
    const { executeAutomationRequest } = await import('./common-handlers.js');
    vi.mocked(executeAutomationRequest).mockRejectedValue(new Error('Timeout'));

    await expect(handleModdingTools('validate_mod_pak', {}, mockTools))
      .rejects.toThrow('Timeout');
  });

  // =========================================
  // INVALID ACTION TESTS (3+)
  // =========================================

  it('should return error for unknown action', async () => {
    const result = await handleModdingTools('invalid_action_xyz', {}, mockTools);

    expect(result).toMatchObject({
      success: false,
      error: expect.stringMatching(/unknown/i),
    });
  });

  it('should return error for empty action', async () => {
    const result = await handleModdingTools('', {}, mockTools);

    expect(result).toMatchObject({
      success: false,
      error: expect.stringMatching(/unknown/i),
    });
  });

  it('should return error for action with typo', async () => {
    const result = await handleModdingTools('load_mod_pack', {}, mockTools); // typo: pack vs pak

    expect(result).toMatchObject({
      success: false,
      error: expect.stringMatching(/unknown/i),
    });
  });

  it('should include hint in error for unknown action', async () => {
    const result = await handleModdingTools('not_a_real_action', {}, mockTools);

    expect(result).toMatchObject({
      success: false,
      hint: expect.any(String),
    });
  });
});
