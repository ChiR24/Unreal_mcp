
/// <reference types="node" />

import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { z } from 'zod';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const serverModulePath = path.join(__dirname, '../dist/index.js');

console.log('🚬 Running Smoke Test (Mock Mode, Static Gateway)...');
console.log(`🔌 Server Module: ${serverModulePath}`);

const GatewayResultSchema = z.object({
    success: z.boolean(),
    operation: z.string()
});

const TextContentItemsSchema = z.array(z.object({
    type: z.literal('text'),
    text: z.string()
})).nonempty();

class SmokeTestError extends Error {
    constructor(message: string) {
        super(message);
        this.name = 'SmokeTestError';
    }
}

function getStructuredContent(content: unknown): Record<string, unknown> {
    const response = content as { structuredContent?: unknown; content?: unknown };
    if (response.structuredContent && typeof response.structuredContent === 'object') {
        return response.structuredContent as Record<string, unknown>;
    }
    const parsed = TextContentItemsSchema.safeParse(response.content);
    if (!parsed.success) {
        throw new SmokeTestError('gateway did not return parseable content');
    }
    return JSON.parse(parsed.data[0].text) as Record<string, unknown>;
}

async function runSmokeTest(): Promise<void> {
    process.env.MOCK_UNREAL_CONNECTION = 'true';
    process.env.NODE_ENV = 'test';

    const client = new Client(
        { name: 'smoke-test', version: '1.0.0' },
        { capabilities: {} }
    );
    const { createServer } = await import(pathToFileURL(serverModulePath).href);
    const { server, bridge, automationBridge, metricsServer } = createServer();
    const [clientTransport, serverTransport] = InMemoryTransport.createLinkedPair();

    try {
        console.log('Sending initialize...');
        await server.connect(serverTransport);
        await client.connect(clientTransport, { timeout: 15000 });
        console.log('✅ Initialize success');

        const toolsResult = await client.listTools(undefined, { timeout: 15000 });
        if (toolsResult.tools.length !== 1) {
            throw new SmokeTestError(`Expected exactly one public tool, found ${toolsResult.tools.length}`);
        }
        if (toolsResult.tools[0]?.name !== 'unreal') {
            throw new SmokeTestError(`Expected public tool 'unreal', found '${toolsResult.tools[0]?.name}'`);
        }
        console.log('✅ Static gateway list success: exactly one tool `unreal`');

        const searchResult = await client.callTool(
            { name: 'unreal', arguments: { operation: 'search', query: 'asset' } },
            undefined,
            { timeout: 15000 }
        );
        const search = GatewayResultSchema.parse(getStructuredContent(searchResult));
        if (!search.success) throw new SmokeTestError('search operation failed');
        console.log('✅ Gateway search success');

        const describeResult = await client.callTool(
            { name: 'unreal', arguments: { operation: 'describe', tool: 'manage_tools' } },
            undefined,
            { timeout: 15000 }
        );
        const describe = getStructuredContent(describeResult) as Record<string, unknown>;
        if (describe.success !== true) throw new SmokeTestError('describe operation failed');
        if (describe.perActionSchemas !== false) throw new SmokeTestError('describe reported perActionSchemas true');
        console.log('✅ Gateway describe success: perActionSchemas=false');

        const configResult = await client.callTool(
            { name: 'unreal', arguments: { operation: 'configure', action: 'get_status' } },
            undefined,
            { timeout: 15000 }
        );
        const config = getStructuredContent(configResult) as Record<string, unknown>;
        if (config.success !== true) throw new SmokeTestError('configure operation failed');
        const configPayload = config.result as { totalTools?: number } | undefined;
        if (configPayload?.totalTools !== 23) {
            throw new SmokeTestError(`configure get_status totalTools expected 23, got ${configPayload?.totalTools}`);
        }
        console.log('✅ Gateway configure success: 23 internal tools');

        const hiddenResult = await client.callTool(
            { name: 'manage_tools', arguments: { action: 'get_status' } },
            undefined,
            { timeout: 15000 }
        );
        const hidden = hiddenResult as { isError?: boolean };
        if (!hidden.isError) {
            throw new SmokeTestError('direct call to hidden parent tool should be rejected');
        }
        console.log('✅ Hidden parent-tool rejection success');

        console.log('🎉 Smoke Test PASSED');
    } finally {
        await clientTransport.close();
        automationBridge.stop();
        bridge.dispose();
        metricsServer?.close();
    }
}

runSmokeTest().catch((error: unknown) => {
    const message = error instanceof Error ? error.message : String(error);
    console.error(`❌ ${message}`);
    process.exit(1);
});
