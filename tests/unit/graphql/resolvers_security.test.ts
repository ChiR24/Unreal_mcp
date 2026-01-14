import { describe, it, expect, vi, beforeEach } from 'vitest';
import { resolvers } from '../../../src/graphql/resolvers';
import { AutomationBridge } from '../../../src/automation/index';

// Mock AutomationBridge
vi.mock('../../../src/automation/index', () => {
  return {
    AutomationBridge: class {
      sendAutomationRequest = vi.fn().mockResolvedValue({ success: true, result: {} });
    },
  };
});

describe('GraphQL Resolvers Security', () => {
  let mockAutomationBridge: any;
  let context: any;

  beforeEach(() => {
    mockAutomationBridge = new AutomationBridge();
    context = {
      automationBridge: mockAutomationBridge,
    };
    vi.clearAllMocks();
  });

  describe('Mutation.duplicateAsset', () => {
    it('should prevent directory traversal in path', async () => {
      const maliciousPath = '../Secrets/Data';

      await expect(
        resolvers.Mutation.duplicateAsset(
          {},
          { path: maliciousPath, newName: 'Copy' },
          context
        )
      ).rejects.toThrow(/Invalid path/);

      expect(mockAutomationBridge.sendAutomationRequest).not.toHaveBeenCalled();
    });
  });

  describe('Mutation.moveAsset', () => {
    it('should prevent directory traversal in source path', async () => {
      const maliciousPath = '../Secrets/Data';

      await expect(
        resolvers.Mutation.moveAsset(
          {},
          { path: maliciousPath, newPath: '/Game/Safe' },
          context
        )
      ).rejects.toThrow(/Invalid path/);

      expect(mockAutomationBridge.sendAutomationRequest).not.toHaveBeenCalled();
    });

    it('should prevent directory traversal in destination path', async () => {
      const maliciousDest = '../Secrets/MoveHere';

      await expect(
        resolvers.Mutation.moveAsset(
          {},
          { path: '/Game/Asset', newPath: maliciousDest },
          context
        )
      ).rejects.toThrow(/Invalid path/);

      expect(mockAutomationBridge.sendAutomationRequest).not.toHaveBeenCalled();
    });
  });

  describe('Mutation.deleteAsset', () => {
    it('should prevent directory traversal', async () => {
      const maliciousPath = '../Secrets/DeleteMe';

      const result = await resolvers.Mutation.deleteAsset(
        {},
        { path: maliciousPath },
        context
      );

      expect(result).toBe(false);
      expect(mockAutomationBridge.sendAutomationRequest).not.toHaveBeenCalled();
    });
  });

  describe('Mutation.createBlueprint', () => {
    it('should prevent directory traversal in path', async () => {
      const maliciousPath = '../Secrets/BP';

      await expect(
        resolvers.Mutation.createBlueprint(
          {},
          { input: { name: 'MyBP', path: maliciousPath } },
          context
        )
      ).rejects.toThrow(/Invalid path/);

      expect(mockAutomationBridge.sendAutomationRequest).not.toHaveBeenCalled();
    });
  });
});
