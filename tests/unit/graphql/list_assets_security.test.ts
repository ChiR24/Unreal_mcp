import { describe, it, expect, vi, beforeEach } from 'vitest';
import { resolvers } from '../../../src/graphql/resolvers';

describe('GraphQL List Assets Security', () => {
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

    it('should throw error when assets query called with traversal pathStartsWith', async () => {
        const maliciousPath = '../../Secret/Dir';

        // This is expected to fail currently (vulnerability exists)
        // Ideally it should throw or result in sanitized call

        try {
            await resolvers.Query.assets(null, { filter: { pathStartsWith: maliciousPath } }, mockContext);
            // If we reach here, it might be vulnerable, check arguments
            const lastCall = mockAutomationBridge.sendAutomationRequest.mock.lastCall;
            // lastCall[0] is command, lastCall[1] is payload
            expect(lastCall[1].filter.pathStartsWith).not.toBe(maliciousPath);
        } catch (error: any) {
            expect(error.message).toMatch(/(Invalid path|Path traversal)/);
        }
    });

    it('should suppress error and return empty list when blueprints query called with traversal pathStartsWith', async () => {
        const maliciousPath = '../../Secret/Dir';

        const result = await resolvers.Query.blueprints(null, { filter: { pathStartsWith: maliciousPath } }, mockContext);

        // Verify automation request was NOT made
        expect(mockAutomationBridge.sendAutomationRequest).not.toHaveBeenCalled();

        // Verify empty result (fail secure)
        expect(result.edges).toEqual([]);
        expect(result.totalCount).toBe(0);
    });
});
