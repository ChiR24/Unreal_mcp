import { describe, expect, it } from 'vitest';
import { executeAutomationRequest } from './automation-request-dispatch.js';
import { runWithMcpRequestContext } from '../../../../automation/request-context.js';
import type { ITools } from '../../../../types/tools/tool-interfaces.js';

function fakeTools(): ITools {
    const captured: Array<{ action: string; payload: Record<string, unknown>; options: Record<string, unknown> }> = [];
    const automationBridge = {
        isConnected: () => true,
        sendAutomationRequest: (action: string, payload: Record<string, unknown>, options: Record<string, unknown> = {}) => {
            captured.push({ action, payload, options });
            return Promise.resolve({ success: true });
        },
        sendRawMessage: () => true,
        getStatus: () => ({}) as never,
    };
    (automationBridge as unknown as { captured: typeof captured }).captured = captured;
    return {
        systemTools: {
            executeConsoleCommand: () => Promise.resolve({ success: true }),
            getProjectSettings: () => Promise.resolve({}),
        },
        assetResources: {} as never,
        actorResources: {},
        levelResources: {},
        automationBridge: automationBridge as unknown as ITools['automationBridge'],
        bridge: {},
    } as unknown as ITools;
}

describe('executeAutomationRequest MCP request correlation', () => {
    it('tags the outbound automation request with the active MCP request id from context', async () => {
        const tools = fakeTools();
        const captured = (tools.automationBridge as unknown as { captured: Array<{ options: Record<string, unknown> }> }).captured;

        await runWithMcpRequestContext({ requestId: 'mcp:ctx', signal: undefined }, async () => {
            await executeAutomationRequest(tools, 'get_actor', { actor: 'Cube' }, 'bridge unavailable');
        });

        expect(captured).toHaveLength(1);
        expect(captured[0].options.mcpRequestId).toBe('mcp:ctx');
    });

    it('does not tag when no request context is active', async () => {
        const tools = fakeTools();
        const captured = (tools.automationBridge as unknown as { captured: Array<{ options: Record<string, unknown> }> }).captured;

        await executeAutomationRequest(tools, 'get_actor', { actor: 'Cube' }, 'bridge unavailable');

        expect(captured).toHaveLength(1);
        expect(captured[0].options.mcpRequestId).toBeUndefined();
    });

    it('prefers an explicit mcpRequestId option over the ambient context', async () => {
        const tools = fakeTools();
        const captured = (tools.automationBridge as unknown as { captured: Array<{ options: Record<string, unknown> }> }).captured;

        await runWithMcpRequestContext({ requestId: 'mcp:ctx', signal: undefined }, async () => {
            await executeAutomationRequest(
                tools,
                'get_actor',
                { actor: 'Cube' },
                'bridge unavailable',
                { mcpRequestId: 'mcp:explicit' },
            );
        });

        expect(captured[0].options.mcpRequestId).toBe('mcp:explicit');
    });
});
