import { describe, it, expect, vi, beforeEach } from 'vitest';
import { resolvers } from '../../../src/graphql/resolvers.js';

describe('GraphQL List Resolvers Security', () => {
    let mockContext: any;
    let mockAutomationBridge: any;

    beforeEach(() => {
        mockAutomationBridge = {
            sendAutomationRequest: vi.fn().mockResolvedValue({ success: true, result: { assets: [], totalCount: 0 } }),
        };
        mockContext = {
            automationBridge: mockAutomationBridge,
        };
    });

    it('should throw error when listing assets with traversal pathStartsWith', async () => {
        const maliciousPath = '../../Secret/Dir';
        const args = { filter: { pathStartsWith: maliciousPath } };

        await expect(resolvers.Query.assets(null, args, mockContext))
            .rejects.toThrow(/Invalid path/);

        expect(mockAutomationBridge.sendAutomationRequest).not.toHaveBeenCalled();
    });

    it('should throw error when listing blueprints with traversal pathStartsWith', async () => {
        const maliciousPath = '../../Secret/Dir';
        const args = { filter: { pathStartsWith: maliciousPath } };

        await expect(resolvers.Query.blueprints(null, args, mockContext))
            .rejects.toThrow(/Invalid path/);

        expect(mockAutomationBridge.sendAutomationRequest).not.toHaveBeenCalled();
    });
});
