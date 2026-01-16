/**
 * Test harness factory for MCP server testing without Unreal Engine
 */
import { Server } from '@modelcontextprotocol/sdk/server/index.js';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { createLinkedTransports, createTestClient } from './in-memory-transport.js';
import { vi } from 'vitest';

export interface TestServerOptions {
  serverName?: string;
  serverVersion?: string;
  mockBridgeResponses?: Record<string, unknown>;
}

export interface TestHarness {
  server: Server;
  client: Client;
  callTool: (name: string, args: Record<string, unknown>) => Promise<unknown>;
  cleanup: () => Promise<void>;
}

/**
 * Create a test server instance with mocked Unreal bridge
 */
export async function createTestServer(options: TestServerOptions = {}): Promise<TestHarness> {
  const { serverName = 'test-server', serverVersion = '1.0.0' } = options;

  const server = new Server(
    { name: serverName, version: serverVersion },
    { capabilities: { tools: {} } }
  );

  const { clientTransport, serverTransport } = createLinkedTransports();
  const client = createTestClient();

  await Promise.all([
    client.connect(clientTransport),
    server.connect(serverTransport)
  ]);

  const callTool = async (name: string, args: Record<string, unknown>): Promise<unknown> => {
    const result = await client.callTool({ name, arguments: args });
    return result;
  };

  const cleanup = async (): Promise<void> => {
    await client.close();
    await server.close();
  };

  return { server, client, callTool, cleanup };
}

/**
 * Mock the automation bridge for isolated testing
 */
export function mockAutomationBridge(responses: Record<string, unknown> = {}) {
  return {
    isConnected: vi.fn().mockReturnValue(true),
    sendAutomationRequest: vi.fn().mockImplementation((tool: string, action: string) => {
      const key = `${tool}:${action}`;
      return Promise.resolve(responses[key] ?? { success: true });
    }),
  };
}
