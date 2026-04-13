import { describe, expect, it } from 'vitest';

const loadCleanup = () => import('../../lib/live-suite-cleanup.mjs');

describe('live suite cleanup', () => {
    it('restores the startup map before deleting live-suite content roots', async () => {
        const {
            DEFAULT_RETURN_LEVEL_PATH,
            buildLiveSuiteCleanupOperations
        } = await loadCleanup();

        const operations = buildLiveSuiteCleanupOperations();

        expect(operations[0]).toMatchObject({
            name: 'control_editor',
            arguments: {
                action: 'open_level',
                levelPath: DEFAULT_RETURN_LEVEL_PATH
            }
        });
        expect(operations).toContainEqual(expect.objectContaining({
            name: 'manage_asset',
            arguments: {
                action: 'delete',
                path: '/Game/IntegrationTest',
                force: true
            }
        }));
        expect(operations).toContainEqual(expect.objectContaining({
            name: 'manage_asset',
            arguments: {
                action: 'delete',
                path: '/Game/AdvancedIntegrationTest',
                force: true
            }
        }));
        expect(operations).toContainEqual(expect.objectContaining({
            name: 'manage_asset',
            arguments: {
                action: 'delete',
                path: '/Game/UI/WBP_WidgetBindings',
                force: true
            }
        }));
    });

    it('covers the managed-runner fixtures with the corrected SplineBP path', async () => {
        const { buildManagedRunnerCleanupOperations } = await loadCleanup();

        const operations = buildManagedRunnerCleanupOperations();

        expect(operations).toContainEqual(expect.objectContaining({
            name: 'manage_level',
            arguments: {
                action: 'unload',
                levelName: 'MainLevel'
            }
        }));
        expect(operations).toContainEqual(expect.objectContaining({
            name: 'manage_level',
            arguments: {
                action: 'unload',
                levelName: 'TestLevel'
            }
        }));
        expect(operations).toContainEqual(expect.objectContaining({
            name: 'manage_asset',
            arguments: {
                action: 'delete',
                path: '/Game/MCPTest',
                force: true
            }
        }));
        expect(operations).toContainEqual(expect.objectContaining({
            name: 'manage_asset',
            arguments: {
                action: 'delete',
                path: '/Game/SplineBP',
                force: true
            }
        }));
        expect(operations).not.toContainEqual(expect.objectContaining({
            name: 'manage_asset',
            arguments: {
                action: 'delete',
                path: '/Game/MCPTest/SplineBP',
                force: true
            }
        }));
    });
});