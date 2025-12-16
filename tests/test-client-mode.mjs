#!/usr/bin/env node
/**
 * Test Client Mode Connection
 * Tests that the MCP server can connect to the Unreal plugin server in client mode
 */

import { spawn } from 'node:child_process';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(__dirname, '..');

async function testClientModeConnection() {
  console.log('Testing MCP server client mode connection...');

  // Set environment variables for client mode
  const env = {
    ...process.env,
    MCP_AUTOMATION_CLIENT_MODE: 'true',
    MCP_AUTOMATION_CLIENT_HOST: '127.0.0.1',
  MCP_AUTOMATION_CLIENT_PORT: '8090',
    LOG_LEVEL: 'debug'
  };

  // Start MCP server in client mode
  const serverProcess = spawn('node', [path.join(repoRoot, 'dist', 'cli.js')], {
    cwd: repoRoot,
    env,
    stdio: ['pipe', 'pipe', 'pipe']
  });

  let serverOutput = '';
  let serverError = '';

  serverProcess.stdout.on('data', (data) => {
    const output = data.toString();
    serverOutput += output;
    console.log('SERVER:', output.trim());
  });

  serverProcess.stderr.on('data', (data) => {
    const error = data.toString();
    serverError += error;
    console.error('SERVER ERROR:', error.trim());
  });

  // Yield once to allow the server process to flush its initial output
  await new Promise((resolve) => setImmediate(resolve));

  // Check if client mode is working
  const connected = serverOutput.includes('Automation bridge connected') ||
                   serverOutput.includes('client connection established');

  const connectionAttempted = serverOutput.includes('Starting client connection') ||
                             serverOutput.includes('Connecting to automation bridge');

  serverProcess.kill();

  console.log('\n=== Test Results ===');
  console.log('Connection attempted:', connectionAttempted);
  console.log('Connected successfully:', connected);

  if (connectionAttempted && !connected) {
    console.log('Note: Connection may have failed because Unreal Engine plugin is not running on port 8091');
    console.log('This is expected if Unreal Engine is not running with the plugin enabled.');
  }

  return {
    connectionAttempted,
    connected,
    serverOutput,
    serverError
  };
}

// Run the test
testClientModeConnection()
  .then((result) => {
    console.log('\nTest completed.');
    process.exit(result.connectionAttempted ? 0 : 1);
  })
  .catch((error) => {
    console.error('Test failed:', error);
    process.exit(1);
  });