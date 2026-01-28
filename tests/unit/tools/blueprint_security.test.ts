import { describe, it, expect, vi, beforeEach } from 'vitest';
import { BlueprintTools } from '../../../src/tools/blueprint.js';

describe('BlueprintTools Security', () => {
    let tools: BlueprintTools;
    let mockBridge: any;
    let mockAutomation: any;

    beforeEach(() => {
        mockAutomation = {
            isConnected: vi.fn().mockReturnValue(true),
            sendAutomationRequest: vi.fn().mockResolvedValue({ success: true, result: {} }),
        };
        mockBridge = {
            getAutomationBridge: vi.fn().mockReturnValue(mockAutomation),
        };
        tools = new BlueprintTools(mockBridge);
    });

    it('modifyConstructionScript should reject path traversal', async () => {
        const maliciousPath = '../../Secret/Blueprint';
        const params = {
            blueprintPath: maliciousPath,
            operations: [{ type: 'test' }]
        };

        const result = await tools.modifyConstructionScript(params);

        expect(result.success).toBe(false);
        expect(result.error).toBe('INVALID_BLUEPRINT_PATH');
        expect(result.message).toMatch(/Path traversal/);
        expect(mockAutomation.sendAutomationRequest).not.toHaveBeenCalled();
    });

    it('getBlueprintSCS should reject path traversal', async () => {
        const maliciousPath = '../../Secret/Blueprint';
        const params = {
            blueprintPath: maliciousPath
        };

        const result = await tools.getBlueprintSCS(params);

        expect(result.success).toBe(false);
        expect(result.error).toBe('INVALID_BLUEPRINT_PATH');
        expect(result.message).toMatch(/Path traversal/);
        expect(mockAutomation.sendAutomationRequest).not.toHaveBeenCalled();
    });

    it('modifyConstructionScript should accept valid path', async () => {
        const validPath = '/Game/MyBP';
        const params = {
            blueprintPath: validPath,
            operations: [{ type: 'test' }]
        };

        await tools.modifyConstructionScript(params);

        expect(mockAutomation.sendAutomationRequest).toHaveBeenCalledWith(
            'blueprint_modify_scs',
            expect.objectContaining({ blueprintPath: validPath }),
            expect.any(Object)
        );
    });
});
