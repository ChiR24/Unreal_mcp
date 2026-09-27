// The MCP surface the server advertises and answers: one `unreal` tool, the
// read-only ue:// resources, the workflow prompts and argument completion.
// Driven end-to-end through an in-memory client; no Unreal connection.

import { EmptyResultSchema, ErrorCode } from '@modelcontextprotocol/sdk/types.js';
import { afterEach, describe, expect, it } from 'vitest';
import { isRecord } from '../../../src/utils/validation/type-guards.js';
import { connectClient, structuredPayload } from '../support/in-memory-server.js';

const EXPECTED_SERVER_CAPABILITIES = { tools: {}, resources: {}, prompts: {}, completions: {} };
const GATEWAY_OPERATIONS = ['search', 'describe', 'execute', 'configure'];
const WORKFLOW_PROMPT_NAMES = ['inspect-fix', 'asset-import', 'level-build', 'blueprint-edit', 'validation', 'sequence-render'];

const active: Array<Awaited<ReturnType<typeof connectClient>>> = [];

async function build() {
    const ctx = await connectClient('primitive-wiring');
    active.push(ctx);
    return ctx;
}

afterEach(async () => {
    for (const ctx of active.splice(0)) await ctx.close();
});

describe('server capability advertisement', () => {
    it('advertises exactly { tools, resources, prompts, completions }', async () => {
        const ctx = await build();
        expect(ctx.client.getServerCapabilities()).toEqual(EXPECTED_SERVER_CAPABILITIES);
    });
});

describe('single gateway tool', () => {
    it('refuses a direct canonical-tool call with an executable DIRECT_TOOL_CALL_REMOVED migration', async () => {
        const ctx = await build();
        const response = await ctx.client.callTool(
            { name: 'manage_asset', arguments: { action: 'list_assets', assetPath: '/Game/Env' } },
            undefined,
            { timeout: 15000 },
        );
        expect(response.isError).toBe(true);
        const envelope = structuredPayload(response);
        expect(envelope.errorCode).toBe('DIRECT_TOOL_CALL_REMOVED');
        expect(envelope.nextCall).toEqual({ operation: 'execute', tool: 'manage_asset', action: 'list_assets', params: { assetPath: '/Game/Env' } });
    });

    it('configure get_status reports both catalog revisions', async () => {
        const ctx = await build();
        const response = await ctx.client.callTool({ name: 'unreal', arguments: { operation: 'configure', action: 'get_status' } }, undefined, { timeout: 15000 });
        const envelope = structuredPayload(response);
        const status = isRecord(envelope.result) ? envelope.result : envelope;
        expect(typeof status.catalogRevision).toBe('string');
        expect(typeof status.catalogStateRevision).toBe('number');
    });

    it('tools/list exposes exactly one `unreal` tool with the four operations', async () => {
        const ctx = await build();
        const list = await ctx.client.listTools(undefined, { timeout: 15000 });
        expect(list.tools.map((tool) => tool.name)).toEqual(['unreal']);
        const properties = isRecord(list.tools[0]?.inputSchema.properties) ? list.tools[0].inputSchema.properties : {};
        const operation = isRecord(properties.operation) ? properties.operation : {};
        expect(operation.enum).toEqual(GATEWAY_OPERATIONS);
    });
});

describe('resources', () => {
    it('resources/list, resources/templates/list and resources/read answer', async () => {
        const ctx = await build();
        const listed = await ctx.client.listResources(undefined, { timeout: 15000 });
        const templates = await ctx.client.listResourceTemplates(undefined, { timeout: 15000 });
        const read = await ctx.client.readResource({ uri: 'ue://capability/catalog' }, { timeout: 15000 });
        expect(listed.resources.length).toBeGreaterThan(0);
        expect(Array.isArray(templates.resourceTemplates)).toBe(true);
        expect(read.contents[0]?.uri).toBe('ue://capability/catalog');
    });
});

describe('prompts', () => {
    it('prompts/list returns the workflow prompts in definition order', async () => {
        const ctx = await build();
        const list = await ctx.client.listPrompts(undefined, { timeout: 15000 });
        expect(list.prompts.map((prompt) => prompt.name)).toEqual(WORKFLOW_PROMPT_NAMES);
    });

    it('prompts/get inspect-fix returns a text message pointing at the gateway', async () => {
        const ctx = await build();
        const result = await ctx.client.getPrompt({ name: 'inspect-fix', arguments: { objectPath: '/Game/Heroes/BP_Hero' } }, { timeout: 15000 });
        const content = result.messages[0]?.content;
        expect(content?.type).toBe('text');
        expect(content?.type === 'text' ? content.text : '').toContain('unreal');
    });
});

describe('completions', () => {
    it('completion/complete for a capabilityId argument returns bounded string candidates', async () => {
        const ctx = await build();
        const result = await ctx.client.complete(
            { ref: { type: 'ref/resource', uri: 'ue://capability/{capabilityId}' }, argument: { name: 'capabilityId', value: 'asset' } },
            { timeout: 15000 },
        );
        const values = result.completion.values;
        expect(values.length).toBeGreaterThan(0);
        expect(values.length).toBeLessThanOrEqual(100);
        expect(values.every((value) => typeof value === 'string')).toBe(true);
    });
});

describe('unadvertised methods', () => {
    it.each(['tasks/list', 'resources/subscribe', 'logging/setLevel'])('%s is MethodNotFound', async (method) => {
        const ctx = await build();
        const error = await ctx.client.request({ method, params: {} }, EmptyResultSchema, { timeout: 15000 }).then(() => undefined, (caught: unknown) => caught);
        expect(isRecord(error) ? error.code : undefined).toBe(ErrorCode.MethodNotFound);
    });
});
