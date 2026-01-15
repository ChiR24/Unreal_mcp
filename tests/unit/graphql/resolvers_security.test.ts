import { describe, it, expect, vi, beforeEach } from 'vitest';
import { resolvers } from '../../../src/graphql/resolvers.js';
import type { GraphQLContext } from '../../../src/graphql/types.js';

describe('GraphQL Resolvers Security', () => {
  let mockContext: GraphQLContext;
  let mockAutomationBridge: any;

  beforeEach(() => {
    mockAutomationBridge = {
      sendAutomationRequest: vi.fn().mockResolvedValue({ success: true, result: {} }),
      isConnected: vi.fn().mockReturnValue(true)
    };

    mockContext = {
      bridge: {} as any,
      automationBridge: mockAutomationBridge,
      loaders: {} as any
    };
  });

  describe('Mutation: duplicateAsset', () => {
    it('should prevent path traversal', async () => {
      const maliciousPath = '/Game/../../Secret/Asset';
      const newName = 'StolenAsset';

      await expect(resolvers.Mutation.duplicateAsset({}, { path: maliciousPath, newName }, mockContext))
        .rejects.toThrow('Invalid path: directory traversal (..) is not allowed');

      expect(mockAutomationBridge.sendAutomationRequest).not.toHaveBeenCalled();
    });
  });

  describe('Mutation: moveAsset', () => {
    it('should prevent path traversal in destination', async () => {
      const sourcePath = '/Game/MyAsset';
      const maliciousDest = '/Game/../../Secret/MovedAsset';

      await expect(resolvers.Mutation.moveAsset({}, { path: sourcePath, newPath: maliciousDest }, mockContext))
        .rejects.toThrow('Invalid path: directory traversal (..) is not allowed');

      expect(mockAutomationBridge.sendAutomationRequest).not.toHaveBeenCalled();
    });
  });
});
