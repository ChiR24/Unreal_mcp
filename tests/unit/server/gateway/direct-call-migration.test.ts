// Task 30 — a direct call to a canonical parent tool is removed from the public
// surface and answered with an executable migration receipt built by
// `buildDirectCallMigration`. These cases pin that receipt's contract: known
// parent + action -> execute, known parent alone -> describe, unknown -> search
// with suggestions, control fields stripped from params, and no input mutation.

import { describe, expect, it } from 'vitest';

import { buildDirectCallMigration } from '../../../../src/server/gateway/direct-call-migration.js';

describe('buildDirectCallMigration — executable direct-call migration guidance (Task 30)', () => {
    it('maps a known parent + action to an execute nextCall', () => {
        // Given a removed canonical tool called with a concrete action...
        // When migration guidance is built...
        const result = buildDirectCallMigration('manage_asset', { action: 'list_assets', assetPath: '/Game/Env' });
        // Then it refuses with a copy-paste-executable execute receipt.
        expect(result.errorCode).toBe('DIRECT_TOOL_CALL_REMOVED');
        expect(result.nextCall).toEqual({
            operation: 'execute',
            tool: 'manage_asset',
            action: 'list_assets',
            params: { assetPath: '/Game/Env' }
        });
    });

    it('maps a known parent without an action to a describe nextCall', () => {
        // Given a removed canonical tool called with no action...
        // When migration guidance is built...
        const result = buildDirectCallMigration('manage_asset', { assetPath: '/Game/Env' });
        // Then it steers the caller to describe the tool first.
        expect(result.errorCode).toBe('DIRECT_TOOL_CALL_REMOVED');
        expect(result.nextCall).toEqual({ operation: 'describe', tool: 'manage_asset' });
    });

    it('maps an unknown parent to a search nextCall with closest-match suggestions', () => {
        // Given a mistyped tool name...
        // When migration guidance is built...
        const result = buildDirectCallMigration('manage_assets', { action: 'list_assets' });
        // Then it steers the caller to search and offers the real tool name.
        expect(result.errorCode).toBe('DIRECT_TOOL_CALL_REMOVED');
        expect(result.nextCall).toMatchObject({ operation: 'search' });
        expect(Array.isArray(result.suggestions)).toBe(true);
        expect(result.suggestions as string[]).toContain('manage_asset');
    });

    it('strips action and subAction from the migrated params', () => {
        // Given a direct call that smuggles action + subAction alongside real params...
        // When migration guidance is built...
        const result = buildDirectCallMigration('manage_asset', {
            action: 'list_assets',
            subAction: 'noop',
            assetPath: '/Game/Env',
            recursive: true
        });
        // Then the execute params carry only the real parameters.
        const nextCall = result.nextCall as Record<string, unknown>;
        expect(nextCall.params).toEqual({ assetPath: '/Game/Env', recursive: true });
        const params = nextCall.params as Record<string, unknown>;
        expect(Object.hasOwn(params, 'action')).toBe(false);
        expect(Object.hasOwn(params, 'subAction')).toBe(false);
    });

    it('does not mutate the caller-supplied args object', () => {
        // Given a frozen args object...
        const args: Record<string, unknown> = { action: 'list_assets', assetPath: '/Game/Env' };
        Object.freeze(args);
        const snapshot = { ...args };
        // When migration guidance is built...
        const result = buildDirectCallMigration('manage_asset', args);
        // Then the original input is untouched and params is a fresh object.
        expect(args).toEqual(snapshot);
        const nextCall = result.nextCall as Record<string, unknown>;
        expect(nextCall.params).not.toBe(args);
    });

    it('carries the gateway envelope fields the `unreal` output schema requires', () => {
        // Given each routing outcome (execute / describe / search)...
        const receipts = [
            buildDirectCallMigration('manage_asset', { action: 'list_assets' }),
            buildDirectCallMigration('manage_asset', {}),
            buildDirectCallMigration('not_a_tool', {})
        ];
        // When the receipt is published through the `unreal` output schema...
        // Then each carries success:false and a top-level operation mirroring nextCall.
        for (const receipt of receipts) {
            expect(receipt.success).toBe(false);
            expect(receipt.operation).toBe((receipt.nextCall as { operation: string }).operation);
        }
    });
});

describe('buildDirectCallMigration — action selection edge cases', () => {
    it('treats an empty-string action as no action and returns describe', () => {
        // Given a known parent with an empty action and no subAction...
        // When migration guidance is built...
        const result = buildDirectCallMigration('manage_asset', { action: '' });
        // Then it routes to describe rather than a blank execute.
        expect(result.nextCall).toEqual({ operation: 'describe', tool: 'manage_asset' });
    });

    it('treats a whitespace-only action as no action and returns describe', () => {
        // Given a known parent with a whitespace action...
        // When migration guidance is built...
        const result = buildDirectCallMigration('manage_asset', { action: '   ', assetPath: '/Game/Env' });
        // Then it routes to describe.
        expect(result.nextCall).toEqual({ operation: 'describe', tool: 'manage_asset' });
    });

    it('ignores a non-string action and returns describe', () => {
        // Given a known parent whose action is not a string...
        // When migration guidance is built...
        const result = buildDirectCallMigration('manage_asset', { action: 42, assetPath: '/Game/Env' });
        // Then the non-string value is not treated as an action.
        expect(result.nextCall).toEqual({ operation: 'describe', tool: 'manage_asset' });
    });

    it('selects subAction as the action when no action is supplied', () => {
        // Given a known parent driven only by subAction...
        // When migration guidance is built...
        const result = buildDirectCallMigration('manage_asset', { subAction: 'apply_layout', widgetPath: '/Game/UI' });
        // Then subAction becomes the execute action and is stripped from params.
        expect(result.nextCall).toEqual({
            operation: 'execute',
            tool: 'manage_asset',
            action: 'apply_layout',
            params: { widgetPath: '/Game/UI' }
        });
    });

    it('falls back to subAction when action is empty', () => {
        // Given an empty action alongside a real subAction...
        // When migration guidance is built...
        const result = buildDirectCallMigration('manage_asset', { action: '', subAction: 'do_thing', x: 1 });
        // Then subAction drives execute and both selectors leave the params.
        expect(result.nextCall).toEqual({
            operation: 'execute',
            tool: 'manage_asset',
            action: 'do_thing',
            params: { x: 1 }
        });
    });

    it('strips a smuggled gateway operation key from the execute params', () => {
        // Given a direct call that carries a gateway `operation` control key...
        // When migration guidance is built...
        const result = buildDirectCallMigration('manage_asset', { action: 'list_assets', operation: 'execute', assetPath: '/Game/Env' });
        // Then the control key never reaches the migrated action params.
        const nextCall = result.nextCall as Record<string, unknown>;
        expect(nextCall.params).toEqual({ assetPath: '/Game/Env' });
        expect(Object.hasOwn(nextCall.params as Record<string, unknown>, 'operation')).toBe(false);
    });

    it('falls through a non-string action to a trimmed subAction and strips both selectors', () => {
        // Given a non-string action alongside a subAction that needs trimming...
        // When migration guidance is built...
        const result = buildDirectCallMigration('manage_asset', { action: 42, subAction: '  apply_layout  ', widgetPath: '/Game/UI' });
        // Then the trimmed subAction drives execute and neither selector reaches params.
        expect(result.nextCall).toEqual({
            operation: 'execute',
            tool: 'manage_asset',
            action: 'apply_layout',
            params: { widgetPath: '/Game/UI' }
        });
        const params = (result.nextCall as Record<string, unknown>).params as Record<string, unknown>;
        expect(Object.hasOwn(params, 'action')).toBe(false);
        expect(Object.hasOwn(params, 'subAction')).toBe(false);
    });
});
