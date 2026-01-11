/**
 * Unit tests for input-handlers.ts
 */
import { describe, it, expect, vi, beforeEach } from 'vitest';
import { handleInputTools } from './input-handlers.js';
import type { ITools } from '../../types/tool-interfaces.js';

// Create mock InputTools
const mockInputTools = {
  createInputAction: vi.fn(),
  createInputMappingContext: vi.fn(),
  addMapping: vi.fn(),
  removeMapping: vi.fn(),
};

// Create mock tools object
const createMockTools = (inputToolsAvailable = true): ITools => ({
  automationBridge: { isConnected: () => true },
  inputTools: inputToolsAvailable ? mockInputTools : undefined,
} as unknown as ITools);

describe('handleInputTools', () => {
  beforeEach(() => {
    vi.clearAllMocks();
  });

  // =========================================
  // SUCCESS PATH TESTS (3+)
  // =========================================

  it('should handle create_input_action successfully', async () => {
    mockInputTools.createInputAction.mockResolvedValue({
      success: true,
      actionPath: '/Game/Input/IA_Jump',
    });

    const result = await handleInputTools(
      'create_input_action',
      { name: 'IA_Jump', path: '/Game/Input' },
      createMockTools()
    );

    expect(result).toEqual({ success: true, actionPath: '/Game/Input/IA_Jump' });
    expect(mockInputTools.createInputAction).toHaveBeenCalledWith('IA_Jump', '/Game/Input');
  });

  it('should handle add_mapping successfully', async () => {
    mockInputTools.addMapping.mockResolvedValue({
      success: true,
      mapped: true,
    });

    const result = await handleInputTools(
      'add_mapping',
      { contextPath: '/Game/Input/IMC_Default', actionPath: '/Game/Input/IA_Jump', key: 'SpaceBar' },
      createMockTools()
    );

    expect(result).toEqual({ success: true, mapped: true });
    expect(mockInputTools.addMapping).toHaveBeenCalledWith(
      '/Game/Input/IMC_Default',
      '/Game/Input/IA_Jump',
      'SpaceBar'
    );
  });

  it('should handle remove_mapping successfully', async () => {
    mockInputTools.removeMapping.mockResolvedValue({
      success: true,
      removed: true,
    });

    const result = await handleInputTools(
      'remove_mapping',
      { contextPath: '/Game/Input/IMC_Default', actionPath: '/Game/Input/IA_Jump' },
      createMockTools()
    );

    expect(result).toEqual({ success: true, removed: true });
    expect(mockInputTools.removeMapping).toHaveBeenCalledWith(
      '/Game/Input/IMC_Default',
      '/Game/Input/IA_Jump'
    );
  });

  it('should handle create_input_mapping_context successfully', async () => {
    mockInputTools.createInputMappingContext.mockResolvedValue({
      success: true,
      contextPath: '/Game/Input/IMC_Default',
    });

    const result = await handleInputTools(
      'create_input_mapping_context',
      { name: 'IMC_Default', path: '/Game/Input' },
      createMockTools()
    );

    expect(result).toEqual({ success: true, contextPath: '/Game/Input/IMC_Default' });
    expect(mockInputTools.createInputMappingContext).toHaveBeenCalledWith('IMC_Default', '/Game/Input');
  });

  // =========================================
  // AUTOMATION FAILURE TESTS (2+)
  // =========================================

  it('should propagate failure from create_input_action', async () => {
    mockInputTools.createInputAction.mockRejectedValue(
      new Error('Failed to create input action')
    );

    await expect(handleInputTools('create_input_action', { name: 'Test' }, createMockTools()))
      .rejects.toThrow('Failed to create input action');
  });

  it('should propagate failure from add_mapping', async () => {
    mockInputTools.addMapping.mockRejectedValue(
      new Error('Bridge timeout')
    );

    await expect(handleInputTools('add_mapping', { contextPath: '/Test', actionPath: '/Test', key: 'A' }, createMockTools()))
      .rejects.toThrow('Bridge timeout');
  });

  it('should return error when inputTools is not available', async () => {
    const result = await handleInputTools('create_input_action', { name: 'Test' }, createMockTools(false));

    expect(result).toMatchObject({
      success: false,
      message: expect.stringMatching(/not available/i),
    });
  });

  // =========================================
  // INVALID ACTION TESTS (3+)
  // =========================================

  it('should return error for unknown action', async () => {
    const result = await handleInputTools('invalid_action_xyz', {}, createMockTools());

    expect(result).toMatchObject({
      success: false,
      message: expect.stringMatching(/unknown/i),
    });
  });

  it('should return error for empty action', async () => {
    const result = await handleInputTools('', {}, createMockTools());

    expect(result).toMatchObject({
      success: false,
      message: expect.stringMatching(/unknown/i),
    });
  });

  it('should return error for action with typo', async () => {
    const result = await handleInputTools('add_mappings', {}, createMockTools()); // typo: mappings vs mapping

    expect(result).toMatchObject({
      success: false,
      message: expect.stringMatching(/unknown/i),
    });
  });

  it('should handle missing optional parameters gracefully', async () => {
    mockInputTools.createInputAction.mockResolvedValue({ success: true });

    const result = await handleInputTools('create_input_action', {}, createMockTools());

    expect(mockInputTools.createInputAction).toHaveBeenCalledWith('', '');
    expect(result).toEqual({ success: true });
  });
});
