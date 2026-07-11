import { describe, it, expect, vi, beforeEach } from 'vitest';
import { handleAssetTools } from '../../../src/tools/handlers/asset/asset-handlers';
import type { ITools } from '../../../src/types/tools/tool-interfaces';

type SendAutomationRequest = (
    action: string,
    payload: Record<string, unknown>,
    options?: { timeoutMs?: number }
) => Promise<Record<string, unknown>>;

const pageResponse = (overrides: Record<string, unknown> = {}) => ({
    success: true,
    assets: [
        { name: 'A', path: '/Game/Foo/A', class: 'Blueprint', packagePath: '/Game/Foo' },
        { name: 'B', path: '/Game/Foo/B', class: 'Blueprint', packagePath: '/Game/Foo' }
    ],
    folders: ['/Game/Foo/Sub'],
    totalCount: 20,
    count: 2,
    limit: 2,
    offset: 0,
    hasMore: true,
    nextOffset: 2,
    cursor: 'cur0',
    nextCursor: 'cur2',
    ...overrides
});

describe('Asset list pagination (T4)', () => {
    let mockTools: ITools;
    let sendAutomationRequest: ReturnType<typeof vi.fn<SendAutomationRequest>>;

    beforeEach(() => {
        sendAutomationRequest = vi.fn<SendAutomationRequest>(async () => pageResponse());
        mockTools = {
            systemTools: {
                executeConsoleCommand: vi.fn(async () => ({ success: true })),
                getProjectSettings: vi.fn(async () => ({}))
            },
            assetResources: { list: vi.fn(async () => ({})) },
            automationBridge: {
                isConnected: vi.fn().mockReturnValue(true),
                sendAutomationRequest
            },
            assetTools: {
                createFolder: vi.fn(), importAsset: vi.fn(), duplicateAsset: vi.fn(),
                renameAsset: vi.fn(), moveAsset: vi.fn(), deleteAssets: vi.fn(),
                generateLODs: vi.fn(), createThumbnail: vi.fn(), getMetadata: vi.fn(),
                validate: vi.fn(), generateReport: vi.fn(), searchAssets: vi.fn(),
                findByTag: vi.fn(), getDependencies: vi.fn(), getSourceControlState: vi.fn()
            }
        };
    });

    const lastPayload = (): Record<string, unknown> => {
        const call = sendAutomationRequest.mock.lastCall;
        if (!call) throw new Error('expected a bridge call');
        return call[1] as Record<string, unknown>;
    };

    it('canonicalizes the default path and forwards bounded defaults', async () => {
        await handleAssetTools('list', {}, mockTools);
        const payload = lastPayload();
        expect(payload.path).toBe('/Game');
        expect(payload.limit).toBe(50);
        expect(payload.offset).toBe(0);
        expect(payload.includeTags).toBe(false);
    });

    it('canonicalizes a bare folder name to a /Game path', async () => {
        await handleAssetTools('list', { path: 'MyFolder' }, mockTools);
        expect(lastPayload().path).toBe('/Game/MyFolder');
    });

    it('rejects traversal paths as a security error before any bridge call', async () => {
        const result = await handleAssetTools('list', { path: '../../Secret/Dir' }, mockTools);
        expect(result.success).toBe(false);
        expect(result.message).toMatch(/Path traversal/);
        expect(sendAutomationRequest).not.toHaveBeenCalled();
    });

    it('clamps limit to the hard maximum and enforces the minimum', async () => {
        await handleAssetTools('list', { limit: 10000 }, mockTools);
        expect(lastPayload().limit).toBe(500);

        sendAutomationRequest.mockClear();
        await handleAssetTools('list', { limit: 0 }, mockTools);
        expect(lastPayload().limit).toBe(1);
    });

    it('accepts numeric-string limit and clamps a negative offset to zero', async () => {
        await handleAssetTools('list', { limit: '100', offset: -5 }, mockTools);
        const payload = lastPayload();
        expect(payload.limit).toBe(100);
        expect(payload.offset).toBe(0);
    });

    it('reads limit/offset from the pagination object alias', async () => {
        await handleAssetTools('list', { pagination: { limit: 10, offset: 5 } }, mockTools);
        const payload = lastPayload();
        expect(payload.limit).toBe(10);
        expect(payload.offset).toBe(5);
    });

    it('forwards an opaque cursor verbatim to the bridge', async () => {
        await handleAssetTools('list', { cursor: 'opaque-cursor-token' }, mockTools);
        expect(lastPayload().cursor).toBe('opaque-cursor-token');
    });

    it('maps the consistent pagination envelope from the bridge response', async () => {
        const result = await handleAssetTools('list', { limit: 2 }, mockTools);
        const data = result.data as Record<string, unknown>;
        expect(result.success).toBe(true);
        expect(data.totalCount).toBe(20);
        expect(data.count).toBe(2);
        expect(data.limit).toBe(2);
        expect(data.offset).toBe(0);
        expect(data.hasMore).toBe(true);
        expect(data.nextOffset).toBe(2);
        expect(data.cursor).toBe('cur0');
        expect(data.nextCursor).toBe('cur2');
        expect((data.assets as unknown[])).toHaveLength(2);
        expect(data.folders).toEqual(['/Game/Foo/Sub']);
    });

    it('reports no nextCursor on the final page', async () => {
        sendAutomationRequest.mockResolvedValueOnce(pageResponse({ hasMore: false, nextOffset: 20, nextCursor: '' }));
        const result = await handleAssetTools('list', {}, mockTools);
        const data = result.data as Record<string, unknown>;
        expect(data.hasMore).toBe(false);
        expect(data.nextCursor).toBeNull();
        expect(data.nextOffset).toBe(20);
    });

    it('propagates a stale/invalid cursor error from the bridge', async () => {
        sendAutomationRequest.mockResolvedValueOnce({
            success: false, error: 'STALE_CURSOR', message: 'cursor is stale'
        });
        const result = await handleAssetTools('list', { cursor: 'stale' }, mockTools);
        expect(result).toMatchObject({ success: false, error: 'STALE_CURSOR' });
    });

    it('enables tags only when explicitly requested (off by default)', async () => {
        await handleAssetTools('list', { includeTags: true }, mockTools);
        expect(lastPayload().includeTags).toBe(true);
    });
});
