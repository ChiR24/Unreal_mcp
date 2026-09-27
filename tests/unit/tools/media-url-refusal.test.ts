// Network media URLs are refused at the MCP boundary: the media records do not
// declare url/urls/streamUrl, so the strict schema rejects them before any
// dispatch. The plugin keeps ValidateRemoteMediaUrl for raw bridge clients.

import { describe, expect, it, vi } from 'vitest';

import { handleUnrealGatewayCall } from '../../../src/server/tool-registry-gateway.js';
import { gatewayContext } from './support/gateway-context-fixture.js';

describe('media URLs never reach the editor', () => {
  it.each([
    ['create_media_source', { name: 'MS_Stream', path: '/Game/Media', sourceType: 'stream', streamUrl: 'http://example.invalid/stream' }],
    ['create_media_playlist', { name: 'MPL_Url', path: '/Game/Media', urls: ['http://example.invalid/playlist'] }],
    ['play_media', { playerPath: '/Game/Media/MP_Test', url: 'http://example.invalid/clip' }],
  ])('%s refuses its URL parameter before dispatch', async (action, params) => {
    const sendAutomationRequest = vi.fn(async () => ({ success: true }));
    const result = await handleUnrealGatewayCall(
      { operation: 'execute', tool: 'manage_sequence', action, params },
      gatewayContext({ isConnected: () => true, sendAutomationRequest }, 'media-url'),
    );

    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('UNDECLARED_PARAMETER');
    expect(sendAutomationRequest).not.toHaveBeenCalled();
  });
});
