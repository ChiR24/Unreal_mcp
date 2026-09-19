import { describe, expect, it, vi } from 'vitest';
import { handleSequenceCoreAction } from './sequence-core-actions.js';
import { handleSequenceTools } from './sequence-handlers.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';

const { executeAutomationRequestMock } = vi.hoisted(() => ({
  executeAutomationRequestMock: vi.fn(async (..._args: unknown[]): Promise<Record<string, unknown>> => ({ success: true }))
}));

vi.mock('../foundation/dispatch/common-handlers.js', async () => {
  const actual = await vi.importActual<typeof import('../foundation/dispatch/common-handlers.js')>('../foundation/dispatch/common-handlers.js');
  return { ...actual, executeAutomationRequest: executeAutomationRequestMock };
});

const tools = { automationBridge: { sendAutomationRequest: vi.fn(), isConnected: () => true } } as unknown as ITools;
const payload = () => executeAutomationRequestMock.mock.calls[0]?.[2] as Record<string, unknown>;

describe('add_actor gate moved to binding module', () => {
  it('throws Missing required parameter: actorName for an empty name', async () => {
    await expect(handleSequenceTools('add_actor', { action: 'add_actor', path: '/Game/S' }, tools))
      .rejects.toThrow(/Missing required parameter: actorName/);
  });

  it('dispatches a valid add_actor through manage_sequence/add_actor', async () => {
    executeAutomationRequestMock.mockClear();
    await handleSequenceTools('add_actor', { action: 'add_actor', path: '/Game/S', actorName: 'Cube' }, tools);
    expect(executeAutomationRequestMock).toHaveBeenCalledWith(tools, 'manage_sequence', expect.objectContaining({ subAction: 'add_actor', actorName: 'Cube' }));
  });
});

describe('add_actors gate moved to binding module', () => {
  it('throws for an empty actorNames array', async () => {
    await expect(handleSequenceTools('add_actors', { action: 'add_actors', path: '/Game/S' }, tools))
      .rejects.toThrow(/Missing required parameter: actorNames/);
  });

  it('throws for a non-array actorNames', async () => {
    await expect(handleSequenceTools('add_actors', { action: 'add_actors', path: '/Game/S', actorNames: 'Cube' }, tools))
      .rejects.toThrow(/Missing required parameter: actorNames/);
  });

  it('dispatches valid add_actors with the trimmed path', async () => {
    executeAutomationRequestMock.mockClear();
    await handleSequenceTools('add_actors', { action: 'add_actors', path: '/Game/S', actorNames: ['A'] }, tools);
    expect(payload()).toEqual(expect.objectContaining({ subAction: 'add_actors', path: '/Game/S', actorNames: ['A'] }));
  });
});

describe('remove_actors and get_bindings gates moved to binding module', () => {
  it('throws for remove_actors with an empty actorNames array', async () => {
    await expect(handleSequenceTools('remove_actors', { action: 'remove_actors', path: '/Game/S' }, tools))
      .rejects.toThrow(/Missing required parameter: actorNames/);
  });

  it('throws for get_bindings without a path', async () => {
    await expect(handleSequenceTools('get_bindings', {}, tools))
      .rejects.toThrow(/Missing required parameter: path/);
  });

  it('dispatches a valid remove_actors', async () => {
    executeAutomationRequestMock.mockClear();
    await handleSequenceTools('remove_actors', { action: 'remove_actors', path: '/Game/S', actorNames: ['A'] }, tools);
    expect(payload()).toEqual(expect.objectContaining({ subAction: 'remove_actors', path: '/Game/S' }));
  });
});

describe('create gate via shared validateRequiredFields', () => {
  it('throws Missing required parameter: name', async () => {
    await expect(handleSequenceCoreAction('create', {}, tools)).rejects.toThrow(/Missing required parameter: name/);
  });
});
