/* eslint-disable no-console */
import { spawn } from 'child_process';
import path from 'path';
import { fileURLToPath } from 'url';

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
    }
];

let buffer = '';
let passed = false;

child.stdout.on('data', (data) => {
    const chunk = data.toString();
    buffer += chunk;

    // Try to parse JSON lines
    const lines = buffer.split('\n');
    buffer = lines.pop() || ''; // Keep the incomplete last line

    for (const line of lines) {
        if (!line.trim()) continue;
        try {
            const msg = JSON.parse(line);
            console.log('Received:', JSON.stringify(msg).substring(0, 100) + '...');

            if (msg.id === 1 && msg.result) {
                console.log('✅ Initialize success');
                // Send list tools request
                child.stdin.write(JSON.stringify(requests[1]) + '\n');
            }

            if (msg.id === 2 && msg.result) {
                console.log(`✅ Tools check success: Found ${msg.result.tools?.length || 0} tools`);
                passed = true;
                child.kill();
            }

        } catch (_e) {
            // Ignore non-JSON output (logs)
        }
    }
});

child.on('exit', (_code) => {
    if (passed) {
        console.log('🎉 Smoke Test PASSED');
        process.exit(0);
    } else {
        console.error('❌ Smoke Test FAILED - Server exited without passing checks');
        process.exit(1);
    }
});

// Start by sending initialize
console.log('Sending initialize...');
child.stdin.write(JSON.stringify(requests[0]) + '\n');

// Timeout safety
setTimeout(() => {
    if (!passed) {
        console.error('❌ Timeout waiting for smoke test');
        console.error('Buffer contents:', buffer);
        child.kill();
        process.exit(1);
    }
}, 15000);
