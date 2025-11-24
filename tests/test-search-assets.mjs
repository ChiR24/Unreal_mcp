import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StdioClientTransport } from '@modelcontextprotocol/sdk/client/stdio.js';
import { spawn } from 'child_process';
import path from 'path';
import { fileURLToPath } from 'url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const SERVER_PATH = path.resolve(__dirname, '../dist/index.js');

async function runTest() {
  const serverProcess = spawn('node', [SERVER_PATH], {
    stdio: ['pipe', 'pipe', 'inherit']
  });

  const transport = new StdioClientTransport({
    command: 'node',
    args: [SERVER_PATH]
  });

  const client = new Client({
    name: 'test-search-assets',
    version: '1.0.0'
  }, {
    capabilities: {}
  });

  try {
    await client.connect(transport);
    console.log('Connected to MCP server');

    // Test 1: Search for StaticMeshes in /Engine/BasicShapes
    console.log('\nTest 1: Searching for StaticMeshes in /Engine/BasicShapes...');
    const result1 = await client.callTool({
      name: 'manage_asset',
      arguments: {
        action: 'search_assets',
        classNames: ['StaticMesh'],
        packagePaths: ['/Engine/BasicShapes'],
        recursivePaths: true,
        limit: 5
      }
    });
    console.log('Result 1:', JSON.stringify(result1, null, 2));

    // Test 2: Search for Blueprints with limit
    console.log('\nTest 2: Searching for Blueprints (limit 2)...');
    const result2 = await client.callTool({
      name: 'manage_asset',
      arguments: {
        action: 'search_assets',
        classNames: ['Blueprint'],
        packagePaths: ['/Game'],
        limit: 2
      }
    });
    console.log('Result 2:', JSON.stringify(result2, null, 2));

  } catch (error) {
    console.error('Test failed:', error);
  } finally {
    await client.close();
    process.exit(0);
  }
}

runTest();
