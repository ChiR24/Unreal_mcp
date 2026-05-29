
/// <reference types="node" />

import { spawn } from 'node:child_process';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const serverPath = path.join(__dirname, '../dist/cli.js');

console.log('🚬 Running Smoke Test (Mock Mode)...');
console.log(`🔌 Server Path: ${serverPath}`);

const env = { ...process.env, MOCK_UNREAL_CONNECTION: 'true' };

const child = spawn('node', [serverPath], {
    env,
    stdio: ['pipe', 'pipe', 'inherit'] // pipe stdin/stdout, inherit stderr
});

const requests = [
    {
        jsonrpc: '2.0',
        id: 1,
        method: 'initialize',
        params: {
            protocolVersion: '2024-11-05',
            capabilities: {},
            clientInfo: { name: 'smoke-test', version: '1.0' }
        }
    },
    {
        jsonrpc: '2.0',
        id: 2,
        method: 'tools/list',
        params: {}
    },
    {
        jsonrpc: '2.0',
        id: 3,
        method: 'tools/call',
        params: {
            name: 'manage_tools',
            arguments: {
                params: {
                    action: 'get_status'
                }
            }
        }
    }
];

let buffer = '';
let passed = false;
const timeout = setTimeout(() => {
    if (!passed) {
        console.error('Buffer contents:', buffer);
        failSmokeTest('Timeout waiting for smoke test');
    }
}, 15000);

function failSmokeTest(message: string): never {
    clearTimeout(timeout);
    console.error(`❌ ${message}`);
    child.kill();
    process.exit(1);
}

function passSmokeTest(): void {
    clearTimeout(timeout);
    passed = true;
    child.kill();
}

function parseJsonLine(line: string): unknown | undefined {
    try {
        return JSON.parse(line);
    } catch (error) {
        if (error instanceof SyntaxError) {
            return undefined;
        }
        throw error;
    }
}

child.stdout.on('data', (data) => {
    const chunk = data.toString();
    buffer += chunk;

    // Try to parse JSON lines
    const lines = buffer.split('\n');
    buffer = lines.pop() || ''; // Keep the incomplete last line

    for (const line of lines) {
        if (!line.trim()) continue;
        try {
            const parsed = parseJsonLine(line);
            if (!parsed || typeof parsed !== 'object') continue;

            const maybeMsg = parsed as Record<string, unknown>;
            if (typeof maybeMsg.id !== 'number' && typeof maybeMsg.id !== 'undefined') continue;
            const msg = maybeMsg as { id?: number; result?: { tools?: unknown[]; content?: Array<{ text?: unknown }> } };
            console.log('Received:', JSON.stringify(msg).substring(0, 100) + '...');

            if (msg.id === 1 && msg.result) {
                console.log('✅ Initialize success');
                // Send list tools request
                child.stdin.write(JSON.stringify(requests[1]) + '\n');
            }

            if (msg.id === 2 && msg.result) {
                console.log(`✅ Tools check success: Found ${msg.result.tools?.length || 0} tools`);
                child.stdin.write(JSON.stringify(requests[2]) + '\n');
            }

            if (msg.id === 3 && msg.result) {
                const textContent = msg.result.content?.[0]?.text;
                const payload = typeof textContent === 'string' ? JSON.parse(textContent) : {};
                if (!payload.success || payload.totalTools !== 22) {
                    throw new Error('manage_tools params smoke check failed');
                }
                console.log('✅ manage_tools params check success');
                passSmokeTest();
            }

        } catch (error) {
            failSmokeTest(error instanceof Error ? error.message : String(error));
        }
    }
});

child.on('exit', (_code) => {
    if (passed) {
        console.log('🎉 Smoke Test PASSED');
        process.exit(0);
    } else {
        failSmokeTest('Smoke Test FAILED - Server exited without passing checks');
    }
});

// Start by sending initialize
console.log('Sending initialize...');
child.stdin.write(JSON.stringify(requests[0]) + '\n');
