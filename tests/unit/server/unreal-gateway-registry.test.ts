import { afterAll, beforeAll, describe, expect, it } from 'vitest';

import { connectClient, structuredPayload } from '../support/in-memory-server.js';

describe('unreal gateway registry integration', () => {
    // The arguments the gateway forwards to the bridge, so we can assert the
    // registry did not drop or flatten them.
    const lastCall: { name?: string; args?: Record<string, unknown> } = {};
    let ctx: Awaited<ReturnType<typeof connectClient>>;

    beforeAll(async () => {
        ctx = await connectClient('gw-registry-test', async (name, args) => {
            lastCall.name = name;
            lastCall.args = args;
            return { success: true, name, action: args?.action, received: args };
        });
    });

    afterAll(async () => {
        await ctx?.close();
    });

    it('routes execute params intact to the underlying handler', async () => {
        const res = await ctx.client.callTool(
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

        const structured = structuredPayload(res);
        expect(structured.success).toBe(true);
        expect(structured.operation).toBe('execute');

        // Params must reach the handler unchanged, not dissolved into the top level.
        expect(lastCall.name).toBe('system_control');
        expect(lastCall.args?.['action']).toBe('get_project_settings');
        expect(lastCall.args?.['category']).toBe('Project');
    });
});
