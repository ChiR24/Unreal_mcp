import { describe, it, expect, vi, beforeEach } from 'vitest';
import { resolvers } from '../../../src/graphql/resolvers';

describe('GraphQL Resolvers Security', () => {
    let mockContext: any;
    let mockAutomationBridge: any;

    beforeEach(() => {
        mockAutomationBridge = {
            sendAutomationRequest: vi.fn().mockResolvedValue({ success: true, result: {} }),
        };
        mockContext = {
            automationBridge: mockAutomationBridge,
        };
    });

    it('should throw error when duplicateAsset called with traversal path', async () => {
        const maliciousPath = '../../Secret/File';
        const newName = 'SafeName';

        // Currently this expects failure (security check)
        // If the code is vulnerable, this test will fail (it won't throw)
        await expect(resolvers.Mutation.duplicateAsset(null, { path: maliciousPath, newName }, mockContext))
            .rejects.toThrow(/Invalid path/);

        expect(mockAutomationBridge.sendAutomationRequest).not.toHaveBeenCalled();
    });

    it('should throw error when moveAsset called with traversal destination', async () => {
        const path = '/Game/SafePath';
        const maliciousDest = '../../Secret/Dest';

        await expect(resolvers.Mutation.moveAsset(null, { path, newPath: maliciousDest }, mockContext))
            .rejects.toThrow(/Invalid path/);

        expect(mockAutomationBridge.sendAutomationRequest).not.toHaveBeenCalled();
    });

    it('should throw error when loadLevel called with traversal path', async () => {
        const maliciousPath = '../../Secret/Level';

        await expect(resolvers.Mutation.loadLevel(null, { path: maliciousPath }, mockContext))
            .rejects.toThrow(/Invalid path/);

        expect(mockAutomationBridge.sendAutomationRequest).not.toHaveBeenCalled();
    });
});
