
/// <reference types="node" />

import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { z } from 'zod';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const serverModulePath = path.join(__dirname, '../dist/index.js');

console.log('🚬 Running Smoke Test (Mock Mode)...');
console.log(`🔌 Server Module: ${serverModulePath}`);

const ManageToolsStatusSchema = z.object({
    success: z.literal(true),
    totalTools: z.literal(23)
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

function getFirstTextContent(content: unknown): string {
    const parsed = TextContentItemsSchema.safeParse(content);
    if (!parsed.success) {
        throw new SmokeTestError('manage_tools did not return text content');
    }
    return parsed.data[0].text;
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
        console.log(`✅ Tools check success: Found ${toolsResult.tools.length} tools`);

        const statusResult = await client.callTool({
            name: 'manage_tools',
            arguments: {
                params: {
                    action: 'get_status'
                }
            }
        }, undefined, { timeout: 15000 });
        const payload = ManageToolsStatusSchema.parse(JSON.parse(getFirstTextContent(statusResult.content)));
        console.log(`✅ manage_tools params check success: ${payload.totalTools} tools`);
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
