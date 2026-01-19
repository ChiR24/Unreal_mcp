import { describe, it, expect, vi, beforeEach } from 'vitest';
import { resolvers } from '../../../src/graphql/resolvers';

describe('GraphQL List Assets Path Traversal Security', () => {
    let mockContext: any;
    let mockAutomationBridge: any;

    beforeEach(() => {
        mockAutomationBridge = {
            sendAutomationRequest: vi.fn().mockResolvedValue({ success: true, result: { assets: [], blueprints: [], totalCount: 0 } }),
        };
        mockContext = {
            automationBridge: mockAutomationBridge,
        };
    });

    it('should throw error when assets query called with traversal pathStartsWith', async () => {
        const maliciousPath = '../../Secret/Dir';
        const args = {
            filter: {
                pathStartsWith: maliciousPath
            }
        };

        // This should reject if we implement the fix
        await expect(resolvers.Query.assets(null, args, mockContext))
            .rejects.toThrow(/Invalid path/);

        expect(mockAutomationBridge.sendAutomationRequest).not.toHaveBeenCalled();
    });

    it('should throw error when blueprints query called with traversal pathStartsWith', async () => {
        const maliciousPath = '../../Secret/Dir';
        const args = {
            filter: {
                pathStartsWith: maliciousPath
            }
        };

        // The blueprints resolver currently catches errors and returns empty result.
        // We might want to change it to rethrow, or at least ensure it doesn't call sendAutomationRequest.
        // If it catches and returns empty, we can check that sendAutomationRequest was NOT called.
        // However, standard sanitization usually throws. The resolver catches errors and logs them.
        // So we expect the result to be empty, AND sendAutomationRequest to NOT be called.

        const result = await resolvers.Query.blueprints(null, args, mockContext);

        expect(mockAutomationBridge.sendAutomationRequest).not.toHaveBeenCalled();
        expect(result.edges).toHaveLength(0);
    });
});
