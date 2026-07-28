import { describe, it, expect, vi, beforeEach } from 'vitest';
import { handleAssetTools } from '../../../src/tools/handlers/asset/asset-handlers';
import type { ITools } from '../../../src/types/tools/tool-interfaces';

type SendAutomationRequest = (
    action: string,
    payload: Record<string, unknown>,
    options?: { timeoutMs?: number }
) => Promise<Record<string, unknown>>;

describe('Asset Handlers Security', () => {
    let mockTools: ITools;
    let sendAutomationRequest: ReturnType<typeof vi.fn<SendAutomationRequest>>;

    beforeEach(() => {
        sendAutomationRequest = vi.fn<SendAutomationRequest>(async () => ({ success: true }));
        mockTools = {
            systemTools: {
                executeConsoleCommand: vi.fn(async () => ({ success: true })),
                getProjectSettings: vi.fn(async () => ({}))
            },
            assetResources: {
                list: vi.fn(async () => ({}))
            },
            automationBridge: {
                isConnected: vi.fn().mockReturnValue(true),
                sendAutomationRequest,
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

        expect(sendAutomationRequest).not.toHaveBeenCalled();
    });

    it('should default path to /Game if not provided or empty', async () => {
        const args = {};

        await handleAssetTools('list', args, mockTools);

        const lastCall = sendAutomationRequest.mock.lastCall;
        expect(lastCall?.[0]).toBe('list');
        expect(lastCall?.[1].path).toBe('/Game');
    });

    it('should sanitize path by ensuring root prefix', async () => {
        const args = {
            path: 'MyFolder'
        };

        await handleAssetTools('list', args, mockTools);

        const lastCall = sendAutomationRequest.mock.lastCall;
        expect(lastCall?.[1].path).toBe('/Game/MyFolder');
    });

    it('should not wrap nested render target failures as success', async () => {
        const bridgeResponse = {
            success: true,
            result: {
                success: false,
                error: 'TEXTURE_ERROR',
                message: 'Failed to create render target'
            }
        };
        sendAutomationRequest.mockResolvedValueOnce(bridgeResponse);

        const result = await handleAssetTools('create_render_target', {
            name: 'RT_FailureRegression',
            packagePath: '/Game/MCPTests',
            save: false
        }, mockTools);

        expect(result.success).toBe(false);
        expect(result.isError).toBe(true);
        expect(result.error).toBe('TEXTURE_ERROR');
        expect(result.message).toBe('Failed to create render target');
        expect(result.data).toMatchObject({
            success: false,
            error: 'TEXTURE_ERROR',
            message: 'Failed to create render target',
            result: bridgeResponse.result
        });
    });

    it('should route get_metadata through manage_asset and normalize metadata', async () => {
        sendAutomationRequest.mockResolvedValueOnce({
            success: true,
            tags: { category: 'environment' },
            metadata: { owner: 'qa' }
        });

        const result = await handleAssetTools('get_metadata', {
            assetPath: '/Game/Props/SM_Crate'
        }, mockTools);

        expect(sendAutomationRequest).toHaveBeenCalledWith('manage_asset', {
            assetPath: '/Game/Props/SM_Crate',
            subAction: 'get_metadata'
        }, { timeoutMs: expect.any(Number) });
        expect(result).toMatchObject({
            success: true,
            message: 'Metadata retrieved (2 items)',
            data: {
                message: 'Metadata retrieved (2 items)',
                tags: { category: 'environment' },
                metadata: { owner: 'qa' }
            }
        });
    });

    it('should reject traversal paths in delete asset arrays', async () => {
        const result = await handleAssetTools('delete', {
            assetPaths: ['../../Secret/Asset']
        }, mockTools);

        expect(result).toMatchObject({
            success: false,
            error: 'SECURITY_VIOLATION'
        });
        expect(sendAutomationRequest).not.toHaveBeenCalled();
    });

    it('should forward the caller timeout for destructive asset deletion', async () => {
        await handleAssetTools('delete', {
            path: '/Game/MCPTests/SlowBlueprintCluster',
            timeoutMs: 60000
        }, mockTools);

        expect(sendAutomationRequest).toHaveBeenCalledWith('manage_asset', {
            paths: ['/Game/MCPTests/SlowBlueprintCluster'],
            subAction: 'delete'
        }, {
            timeoutMs: 60000
        });
    });

    it('should reject traversal paths in bulk_delete asset arrays', async () => {
        const result = await handleAssetTools('bulk_delete', {
            assetPaths: ['../../Secret/Asset']
        }, mockTools);

        expect(result).toMatchObject({
            success: false,
            error: 'SECURITY_VIOLATION'
        });
        expect(sendAutomationRequest).not.toHaveBeenCalled();
    });

    it('should reject traversal paths in bulk_delete folder path', async () => {
        const result = await handleAssetTools('bulk_delete', {
            folderPath: '../../Secret'
        }, mockTools);

        expect(result).toMatchObject({
            success: false,
            error: 'SECURITY_VIOLATION'
        });
        expect(sendAutomationRequest).not.toHaveBeenCalled();
    });

    it('should not wrap create_folder automation failures as success', async () => {
        sendAutomationRequest.mockResolvedValueOnce({
            success: false,
            error: 'DENIED',
            message: 'Folder write denied'
        });

        const result = await handleAssetTools('create_folder', {
            path: '/Game/Locked'
        }, mockTools);

        expect(result).toMatchObject({
            success: false,
            error: 'DENIED',
            message: 'Folder write denied',
            path: '/Game/Locked'
        });
    });

    it('should not wrap get_metadata automation failures as success', async () => {
        sendAutomationRequest.mockResolvedValueOnce({
            success: false,
            error: 'DENIED',
            message: 'Metadata read denied'
        });

        const result = await handleAssetTools('get_metadata', {
            assetPath: '/Game/Locked'
        }, mockTools);

        expect(result).toMatchObject({
            success: false,
            error: 'DENIED',
            message: 'Metadata read denied',
            assetPath: '/Game/Locked'
        });
    });

    it('should not wrap bulk_delete automation failures as success', async () => {
        sendAutomationRequest.mockResolvedValueOnce({
            success: false,
            error: 'DENIED',
            message: 'Bulk delete denied'
        });

        const result = await handleAssetTools('bulk_delete', {
            assetPaths: ['/Game/Locked']
        }, mockTools);

        expect(result).toMatchObject({
            success: false,
            error: 'DENIED',
            message: 'Bulk delete denied',
            assetPaths: ['/Game/Locked']
        });
    });
});
