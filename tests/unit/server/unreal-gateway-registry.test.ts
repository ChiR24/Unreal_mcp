import { afterAll, beforeAll, describe, expect, it, vi } from 'vitest';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import { createServer } from '../../../src/server/server-factory.js';

type BuiltServer = ReturnType<typeof createServer>;

// Capture the arguments the gateway forwards to the consolidated tool handler so
// we can assert the registry did not drop or flatten them.
const lastCall: { name?: string; args?: Record<string, unknown> } = {};

vi.mock('../../../src/tools/orchestration/consolidated-tool-handlers.js', async (importOriginal) => {
    const actual = await importOriginal<Record<string, unknown>>();
    return {
        ...actual,
        handleConsolidatedToolCall: async (name: string, args: Record<string, unknown>) => {
            lastCall.name = name;
            lastCall.args = args;
            return { success: true, name, action: args.action, received: args };
        }
    };
});

function getStructuredContent(content: unknown): Record<string, unknown> {
    const response = content as { structuredContent?: unknown; content?: unknown };
    if (response.structuredContent && typeof response.structuredContent === 'object') {
        return response.structuredContent as Record<string, unknown>;
    }
    const text = (response.content as Array<{ type: string; text?: string }>) ?? [];
    if (text[0]?.text) return JSON.parse(text[0].text) as Record<string, unknown>;
    return {};
}

describe('unreal gateway registry integration', () => {
    let server: BuiltServer['server'];
    let client: Client;
    let clientTransport: ReturnType<typeof InMemoryTransport.createLinkedPair>[0];
    let bridge: BuiltServer['bridge'];
    let automationBridge: BuiltServer['automationBridge'];
    let metricsServer: BuiltServer['metricsServer'];

    beforeAll(async () => {
        process.env.MOCK_UNREAL_CONNECTION = 'true';
        process.env.NODE_ENV = 'test';
        const built = createServer();
        server = built.server;
        bridge = built.bridge;
        automationBridge = built.automationBridge;
        metricsServer = built.metricsServer;

        client = new Client({ name: 'gw-registry-test', version: '1.0.0' }, { capabilities: {} });
        const pair = InMemoryTransport.createLinkedPair();
        clientTransport = pair[0];
        await server.connect(pair[1]);
        await client.connect(pair[0], { timeout: 15000 });
    });

    afterAll(async () => {
        await clientTransport?.close();
        automationBridge?.stop();
        bridge?.dispose();
        metricsServer?.close();
    });

    it('advertises exactly one public tool: unreal', async () => {
        const list = await client.listTools(undefined, { timeout: 15000 });
        expect(list.tools).toHaveLength(1);
        expect(list.tools[0]?.name).toBe('unreal');
    });

    it('routes execute params intact to the underlying handler', async () => {
        lastCall.name = undefined;
        lastCall.args = undefined;

        const res = await client.callTool(
            {
                name: 'unreal',
                arguments: {
                    operation: 'execute',
                    tool: 'system_control',
                    action: 'get_project_settings',
                    params: { category: 'Project' }
                }
            },
            undefined,
            { timeout: 15000 }
        );

        const structured = getStructuredContent(res);
        expect(structured.success).toBe(true);
        expect(structured.operation).toBe('execute');

        // Params must reach the consolidated handler unchanged, not dissolved into
        // the top level by the registry's legacy merge step.
        expect(lastCall.name).toBe('system_control');
        expect(lastCall.args?.action).toBe('get_project_settings');
        expect(lastCall.args?.category).toBe('Project');
    });

    it('rejects a direct call to a hidden parent tool', async () => {
        const res = await client.callTool(
            { name: 'manage_tools', arguments: { action: 'get_status' } },
            undefined,
            { timeout: 15000 }
        );
        const asError = res as { isError?: boolean };
        expect(asError.isError).toBe(true);
    });
});
