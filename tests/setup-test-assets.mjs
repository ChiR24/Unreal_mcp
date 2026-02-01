#!/usr/bin/env node
/**
 * Test Asset Setup Script
 *
 * Creates required test assets before running integration tests.
 * This script is idempotent - running it multiple times is safe.
 *
 * Usage:
 *   node tests/setup-test-assets.mjs
 *   node tests/setup-test-assets.mjs && npm test
 */

import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StdioClientTransport } from '@modelcontextprotocol/sdk/client/stdio.js';
import path from 'node:path';
import fs from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import net from 'node:net';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const repoRoot = path.resolve(__dirname, '..');

// Configuration
const ASSETS = {
  foliage: {
    path: '/Game/Foliage/TestFoliage',
    type: 'foliage_type',
    tool: 'build_environment',
    action: 'add_foliage_type',
    params: {
      foliageTypeName: 'TestFoliage',
      path: '/Game/Foliage/TestFoliage',
      meshPath: '/Engine/BasicShapes/Cube'
    }
  },
  blueprint: {
    path: '/Game/Blueprints/SplineMeshBP',
    type: 'blueprint',
    tool: 'manage_asset',
    action: 'create_blueprint',
    params: {
      path: '/Game/Blueprints/SplineMeshBP',
      parentClass: 'Actor',
      name: 'SplineMeshBP'
    }
  },
  staticMesh: {
    path: '/Game/WorldToolsTest',
    type: 'static_mesh',
    tool: 'manage_geometry',
    action: 'create_static_mesh',
    params: {
      meshName: 'WorldToolsTest',
      path: '/Game/WorldToolsTest'
    },
    optional: true
  }
};

// Defaults for spawning the MCP server
let serverCommand = process.env.UNREAL_MCP_SERVER_CMD ?? 'node';
let serverArgs = process.env.UNREAL_MCP_SERVER_ARGS ? process.env.UNREAL_MCP_SERVER_ARGS.split(',') : [path.join(repoRoot, 'dist', 'cli.js')];
const serverCwd = process.env.UNREAL_MCP_SERVER_CWD ?? repoRoot;
const serverEnv = Object.assign({}, process.env);

/**
 * Wait for automation bridge to be available
 */
async function waitForAutomationBridge(host, ports, timeoutMs = 10000) {
  const start = Date.now();
  while (Date.now() - start < timeoutMs) {
    for (const port of ports) {
      try {
        await new Promise((resolve, reject) => {
          const sock = new net.Socket();
          let settled = false;
          sock.setTimeout(1000);
          sock.once('connect', () => { settled = true; sock.destroy(); resolve(true); });
          sock.once('timeout', () => { if (!settled) { settled = true; sock.destroy(); reject(new Error('timeout')); } });
          sock.once('error', () => { if (!settled) { settled = true; sock.destroy(); reject(new Error('error')); } });
          sock.connect(port, host);
        });
        console.log(`✅ Automation bridge available on ${host}:${port}`);
        return port;
      } catch {
        // Try next port
      }
    }
    await new Promise((r) => setImmediate(r));
  }
  throw new Error(`Automation bridge not available on ports: ${ports.join(',')}`);
}

/**
 * Get latest modification time of a directory
 */
async function getLatestMtime(dir) {
  let latest = 0;
  try {
    const entries = await fs.readdir(dir, { withFileTypes: true });
    for (const e of entries) {
      const full = path.join(dir, e.name);
      if (e.isDirectory()) {
        const child = await getLatestMtime(full);
        if (child > latest) latest = child;
      } else {
        try {
          const st = await fs.stat(full);
          const m = st.mtimeMs || 0;
          if (m > latest) latest = m;
        } catch (_) { }
      }
    }
  } catch (_) { }
  return latest;
}

/**
 * Determine whether to use dist or ts-node
 */
async function determineServerMode() {
  const distPath = path.join(repoRoot, 'dist', 'cli.js');
  const srcDir = path.join(repoRoot, 'src');
  let useDist = false;
  let distExists = false;

  try {
    await fs.access(distPath);
    distExists = true;
  } catch (e) {
    distExists = false;
  }

  if (process.env.UNREAL_MCP_FORCE_DIST === '1') {
    useDist = true;
    console.log('Using dist (forced via UNREAL_MCP_FORCE_DIST=1)');
  } else if (distExists) {
    try {
      const distStat = await fs.stat(distPath);
      const srcLatest = await getLatestMtime(srcDir);
      if (srcLatest > (distStat.mtimeMs || 0)) {
        console.log('Source files newer than dist, using ts-node');
        useDist = false;
      } else {
        useDist = true;
        console.log('Using dist build');
      }
    } catch {
      useDist = false;
      console.log('Using ts-node (fallback)');
    }
  } else {
    console.log('dist not found, using ts-node');
    useDist = false;
  }

  if (!useDist) {
    return {
      command: process.env.UNREAL_MCP_SERVER_CMD ?? 'npx',
      args: ['ts-node-esm', path.join(repoRoot, 'src', 'cli.ts')]
    };
  }

  return {
    command: serverCommand,
    args: serverArgs
  };
}

/**
 * Check if an asset exists using manage_asset tool
 */
async function checkAssetExists(client, assetPath) {
  try {
    const response = await callToolWithTimeout(client, {
      name: 'manage_asset',
      arguments: {
        action: 'asset_exists',
        path: assetPath
      }
    }, 10000);

    // Parse response
    let structuredContent = response.structuredContent ?? null;
    if (structuredContent === null && response.content?.length) {
      for (const entry of response.content) {
        if (entry?.type !== 'text' || typeof entry.text !== 'string') continue;
        try {
          structuredContent = JSON.parse(entry.text);
          break;
        } catch { }
      }
    }

    return structuredContent?.exists === true || structuredContent?.success === true;
  } catch (error) {
    console.warn(`Warning: Failed to check if ${assetPath} exists:`, error.message);
    return false;
  }
}

/**
 * Create an asset using the appropriate tool
 */
async function createAsset(client, assetConfig) {
  const { tool, action, params } = assetConfig;

  try {
    const response = await callToolWithTimeout(client, {
      name: tool,
      arguments: {
        action,
        ...params
      }
    }, 30000);

    // Parse response
    let structuredContent = response.structuredContent ?? null;
    if (structuredContent === null && response.content?.length) {
      for (const entry of response.content) {
        if (entry?.type !== 'text' || typeof entry.text !== 'string') continue;
        try {
          structuredContent = JSON.parse(entry.text);
          break;
        } catch { }
      }
    }

    // Check for success or already exists
    const success = structuredContent?.success === true;
    const alreadyExists = structuredContent?.error === 'ASSET_ALREADY_EXISTS' ||
                         structuredContent?.message?.toLowerCase().includes('already exists') ||
                         structuredContent?.error?.toLowerCase().includes('already exists');

    return { success: success || alreadyExists, alreadyExists, response: structuredContent };
  } catch (error) {
    return { success: false, error: error.message };
  }
}

/**
 * Call a tool with timeout
 */
async function callToolWithTimeout(client, callOptions, timeoutMs) {
  const callPromise = client.callTool(callOptions);

  let timeoutId;
  const timeoutPromise = new Promise((_, rej) => {
    timeoutId = setTimeout(() => rej(new Error(`Timeout after ${timeoutMs}ms`)), timeoutMs);
    if (timeoutId && typeof timeoutId.unref === 'function') {
      timeoutId.unref();
    }
  });

  try {
    const result = await Promise.race([callPromise, timeoutPromise]);
    return result;
  } finally {
    if (timeoutId) {
      clearTimeout(timeoutId);
    }
  }
}

/**
 * Main setup function
 */
async function setup() {
  console.log('='.repeat(60));
  console.log('Test Asset Setup');
  console.log('='.repeat(60));
  console.log('');

  let transport;
  let client;
  const results = [];

  try {
    // Wait for automation bridge
    const bridgeHost = process.env.MCP_AUTOMATION_WS_HOST ?? '127.0.0.1';
    const envPorts = process.env.MCP_AUTOMATION_WS_PORTS
      ? process.env.MCP_AUTOMATION_WS_PORTS.split(',').map((p) => parseInt(p.trim(), 10)).filter(Boolean)
      : [8090, 8091];

    try {
      await waitForAutomationBridge(bridgeHost, envPorts, 10000);
    } catch (err) {
      console.warn('⚠️ Automation bridge not available:', err.message);
      console.log('Tests may fail if Unreal Engine is not running with the plugin enabled.');
      console.log('');
    }

    // Determine server mode
    const serverConfig = await determineServerMode();

    // Create transport and client
    transport = new StdioClientTransport({
      command: serverConfig.command,
      args: serverConfig.args,
      cwd: serverCwd,
      stderr: 'inherit',
      env: serverEnv
    });

    client = new Client({
      name: 'unreal-mcp-test-setup',
      version: '1.0.0'
    });

    await client.connect(transport);
    console.log('✅ Connected to Unreal MCP Server\n');

    // Process each asset
    for (const [key, config] of Object.entries(ASSETS)) {
      const { path: assetPath, type, optional } = config;

      console.log(`Checking ${type}: ${assetPath}`);

      // Check if asset exists
      const exists = await checkAssetExists(client, assetPath);

      if (exists) {
        console.log(`  ✅ Already exists (skipping)`);
        results.push({ asset: assetPath, status: 'exists' });
      } else {
        console.log(`  📝 Creating...`);

        if (optional && process.env.SKIP_OPTIONAL_ASSETS === '1') {
          console.log(`  ⏭️  Skipped (optional)`);
          results.push({ asset: assetPath, status: 'skipped' });
          continue;
        }

        const result = await createAsset(client, config);

        if (result.success) {
          if (result.alreadyExists) {
            console.log(`  ✅ Created (or already existed)`);
            results.push({ asset: assetPath, status: 'exists' });
          } else {
            console.log(`  ✅ Created successfully`);
            results.push({ asset: assetPath, status: 'created' });
          }
        } else {
          console.log(`  ❌ Failed: ${result.error || 'Unknown error'}`);
          if (optional) {
            console.log(`     (This is optional, continuing...)`);
            results.push({ asset: assetPath, status: 'failed_optional', error: result.error });
          } else {
            results.push({ asset: assetPath, status: 'failed', error: result.error });
          }
        }
      }
      console.log('');
    }

    // Summary
    console.log('='.repeat(60));
    console.log('Setup Summary');
    console.log('='.repeat(60));

    const created = results.filter(r => r.status === 'created').length;
    const existing = results.filter(r => r.status === 'exists').length;
    const failed = results.filter(r => r.status === 'failed').length;
    const failedOptional = results.filter(r => r.status === 'failed_optional').length;
    const skipped = results.filter(r => r.status === 'skipped').length;

    console.log(`Created:    ${created}`);
    console.log(`Existing:   ${existing}`);
    console.log(`Failed:     ${failed}`);
    if (failedOptional > 0) console.log(`Failed (optional): ${failedOptional}`);
    if (skipped > 0) console.log(`Skipped:    ${skipped}`);
    console.log('');

    if (failed > 0) {
      console.log('❌ Setup completed with errors');
      const failedAssets = results.filter(r => r.status === 'failed');
      for (const fail of failedAssets) {
        console.log(`   - ${fail.asset}: ${fail.error}`);
      }
      process.exitCode = 1;
    } else {
      console.log('✅ Setup completed successfully');
      console.log('');
      console.log('You can now run tests:');
      console.log('  npm test');
    }

  } catch (error) {
    console.error('Setup failed:', error);
    process.exit(1);
  } finally {
    if (client) {
      try {
        await client.close();
      } catch { }
    }
    if (transport) {
      try {
        await transport.close();
      } catch { }
    }
  }
}

// Run setup
setup();
