/**
 * Tests verifying the test harness works without UE connection
 */
import { describe, it, expect, afterEach } from 'vitest';
import { createTestServer, mockAutomationBridge, type TestHarness } from './test-harness.js';
import { createLinkedTransports, createTestClient } from './in-memory-transport.js';

describe('InMemoryTransport', () => {
  it('creates linked transport pair', () => {
    const { clientTransport, serverTransport } = createLinkedTransports();
    expect(clientTransport).toBeDefined();
    expect(serverTransport).toBeDefined();
  });

  it('creates test client', () => {
    const client = createTestClient('my-client', '2.0.0');
    expect(client).toBeDefined();
  });
});

describe('TestHarness', () => {
  let harness: TestHarness | null = null;

  afterEach(async () => {
    if (harness) {
      await harness.cleanup();
      harness = null;
    }
  });

  it('creates test server without UE connection', async () => {
    harness = await createTestServer();
    expect(harness.server).toBeDefined();
    expect(harness.client).toBeDefined();
    expect(harness.callTool).toBeInstanceOf(Function);
  });

  it('cleanup closes connections', async () => {
    harness = await createTestServer();
    await expect(harness.cleanup()).resolves.not.toThrow();
    harness = null;
  });
});

describe('mockAutomationBridge', () => {
  it('returns mocked responses', async () => {
    const mockBridge = mockAutomationBridge({
      'manage_asset:list': { success: true, assets: ['test'] }
    });

    expect(mockBridge.isConnected()).toBe(true);
    
    const result = await mockBridge.sendAutomationRequest('manage_asset', 'list');
    expect(result).toEqual({ success: true, assets: ['test'] });
  });

  it('returns default success for unknown actions', async () => {
    const mockBridge = mockAutomationBridge();
    
    const result = await mockBridge.sendAutomationRequest('unknown', 'action');
    expect(result).toEqual({ success: true });
  });
});
