import { describe, it, expect, vi, beforeEach } from 'vitest';
import { handleAssetTools } from '../../../src/tools/handlers/asset-handlers';

describe('Asset Handlers Security', () => {
    let mockTools: any;

    beforeEach(() => {
        mockTools = {
            automationBridge: {
                isConnected: vi.fn().mockReturnValue(true),
                sendAutomationRequest: vi.fn().mockResolvedValue({ success: true }),
            },
            assetTools: {
                createFolder: vi.fn(),
                importAsset: vi.fn(),
                duplicateAsset: vi.fn(),
                renameAsset: vi.fn(),
                moveAsset: vi.fn(),
                deleteAssets: vi.fn(),
                generateLODs: vi.fn(),
                createThumbnail: vi.fn(),
                getMetadata: vi.fn(),
                validate: vi.fn(),
                generateReport: vi.fn(),
                searchAssets: vi.fn(),
                findByTag: vi.fn(),
                getDependencies: vi.fn(),
                getSourceControlState: vi.fn(),
            }
        };
    });

    it('should return error when list action called with traversal path', async () => {
        const maliciousPath = '../../Secret/Dir';
        const args = {
            path: maliciousPath
        };

        // handleAssetTools catches error and returns failure response
        const result = await handleAssetTools('list', args, mockTools);

        expect(result.success).toBe(false);
        expect(result.message).toMatch(/Path traversal/);

        expect(mockTools.automationBridge.sendAutomationRequest).not.toHaveBeenCalled();
    });

    it('should default path to /Game if not provided or empty', async () => {
        const args = {};

        await handleAssetTools('list', args, mockTools);

        const lastCall = mockTools.automationBridge.sendAutomationRequest.mock.lastCall;
        expect(lastCall[0]).toBe('list');
        expect(lastCall[1].path).toBe('/Game');
    });

    it('should sanitize path by ensuring root prefix', async () => {
        const args = {
            path: 'MyFolder'
        };

        await handleAssetTools('list', args, mockTools);

        const lastCall = mockTools.automationBridge.sendAutomationRequest.mock.lastCall;
        expect(lastCall[1].path).toBe('/Game/MyFolder');
    });
});
