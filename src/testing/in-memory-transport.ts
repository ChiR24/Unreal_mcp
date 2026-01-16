/**
 * InMemoryTransport wrapper for testing MCP tools without UE connection
 */
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import type { Transport } from '@modelcontextprotocol/sdk/shared/transport.js';

export { InMemoryTransport };

export interface LinkedTransports {
  clientTransport: Transport;
  serverTransport: Transport;
}

/**
 * Create a linked pair of transports for in-process testing
 */
export function createLinkedTransports(): LinkedTransports {
  const [clientTransport, serverTransport] = InMemoryTransport.createLinkedPair();
  return { clientTransport, serverTransport };
}

/**
 * Create a test client that can call MCP tools
 */
export function createTestClient(name = 'test-client', version = '1.0.0'): Client {
  return new Client({ name, version }, { capabilities: {} });
}
