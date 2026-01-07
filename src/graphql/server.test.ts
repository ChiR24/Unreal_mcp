import { describe, it, expect, vi, beforeEach } from 'vitest';
import { GraphQLServer } from './server';
import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';

// Mock dependencies
vi.mock('../unreal-bridge.js');
vi.mock('../automation/index.js');
vi.mock('graphql-yoga', () => ({
  createYoga: vi.fn(),
}));
vi.mock('http', () => ({
  createServer: vi.fn(),
}));

describe('GraphQLServer Security', () => {
  let mockBridge: UnrealBridge;
  let mockAutomationBridge: AutomationBridge;

  beforeEach(() => {
    mockBridge = new UnrealBridge() as any;
    mockAutomationBridge = new AutomationBridge() as any;
  });

  it('should allow origin "*" if credentials is false', () => {
    const server = new GraphQLServer(mockBridge, mockAutomationBridge, {
      cors: {
        origin: '*',
        credentials: false
      }
    });
    const config = server.getConfig();
    expect(config.cors.origin).toBe('*');
    expect(config.cors.credentials).toBe(false);
  });

  it('should disable credentials if origin is "*" to prevent vulnerability', () => {
    const server = new GraphQLServer(mockBridge, mockAutomationBridge, {
      cors: {
        origin: '*',
        credentials: true
      }
    });

    // The server should sanitize this configuration
    const config = server.getConfig();
    expect(config.cors.credentials).toBe(false);
  });

  it('should allow specific origin with credentials', () => {
    const server = new GraphQLServer(mockBridge, mockAutomationBridge, {
      cors: {
        origin: 'http://example.com',
        credentials: true
      }
    });
    const config = server.getConfig();
    expect(config.cors.origin).toBe('http://example.com');
    expect(config.cors.credentials).toBe(true);
  });
});
