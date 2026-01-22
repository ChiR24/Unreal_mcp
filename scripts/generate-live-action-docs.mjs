#!/usr/bin/env node
/**
 * Generate action reference documentation from a LIVE MCP server
 * 
 * Unlike generate-action-docs.ts which reads static source files,
 * this script starts the actual MCP server and fetches tool definitions
 * via the MCP protocol - exactly what an LLM client sees at runtime.
 * 
 * Usage: npm run docs:generate-live
 * Output: docs/live-action-reference.md
 * 
 * Environment Variables:
 *   UNREAL_MCP_SERVER_CMD - Command to run server (default: 'node')
 *   UNREAL_MCP_SERVER_ARGS - Args (default: 'dist/cli.js')
 *   MOCK_UNREAL_CONNECTION - Set to 'true' to skip UE connection requirement
 */
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StdioClientTransport } from '@modelcontextprotocol/sdk/client/stdio.js';
import { writeFileSync, mkdirSync, existsSync } from 'fs';
import { dirname, join } from 'path';
import { fileURLToPath } from 'url';
import { spawn } from 'child_process';
import path from 'path';
import fs from 'fs/promises';

const __dirname = dirname(fileURLToPath(import.meta.url));
const repoRoot = join(__dirname, '..');
const outputPath = join(__dirname, '..', 'docs', 'live-action-reference.md');

// Server spawn configuration
const serverCommand = process.env.UNREAL_MCP_SERVER_CMD ?? 'node';
const serverArgs = process.env.UNREAL_MCP_SERVER_ARGS
  ? process.env.UNREAL_MCP_SERVER_ARGS.split(',')
  : [join(repoRoot, 'dist', 'cli.js')];
const serverCwd = repoRoot;

// Ensure dist exists, if not attempt build
async function ensureDistExists() {
  const distPath = join(repoRoot, 'dist', 'cli.js');
  try {
    await fs.access(distPath);
    return true;
  } catch {
    console.log('dist/cli.js not found, attempting build...');
    return new Promise((resolve) => {
      const npmCmd = process.platform === 'win32' ? 'npm.cmd' : 'npm';
      const build = spawn(npmCmd, ['run', 'build:core'], {
        cwd: repoRoot,
        stdio: 'inherit',
        shell: process.platform === 'win32'
      });
      build.on('close', (code) => {
        if (code === 0) {
          console.log('Build succeeded');
          resolve(true);
        } else {
          console.error('Build failed');
          resolve(false);
        }
      });
      build.on('error', (err) => {
        console.error('Build error:', err);
        resolve(false);
      });
    });
  }
}

async function generateDocsFromLiveServer() {
  console.log('='.repeat(60));
  console.log('Live Action Reference Generator');
  console.log('='.repeat(60));
  console.log('');

  // Ensure build exists
  const buildOk = await ensureDistExists();
  if (!buildOk) {
    console.error('Cannot proceed without dist/ build');
    process.exit(1);
  }

  // Set MOCK_UNREAL_CONNECTION to skip UE requirement
  const serverEnv = {
    ...process.env,
    MOCK_UNREAL_CONNECTION: process.env.MOCK_UNREAL_CONNECTION ?? 'true',
    LOG_LEVEL: 'warn' // Reduce noise
  };

  console.log(`Starting MCP server: ${serverCommand} ${serverArgs.join(' ')}`);
  console.log(`Working directory: ${serverCwd}`);
  console.log('');

  let transport;
  let client;

  try {
    // Create stdio transport that spawns the server
    transport = new StdioClientTransport({
      command: serverCommand,
      args: serverArgs,
      cwd: serverCwd,
      stderr: 'pipe', // Capture stderr to reduce noise
      env: serverEnv
    });

    // Create MCP client
    client = new Client({
      name: 'live-docs-generator',
      version: '1.0.0'
    });

    // Connect to the server
    console.log('Connecting to MCP server...');
    await client.connect(transport);
    console.log('Connected!');
    console.log('');

    // Fetch all tools via MCP protocol
    console.log('Fetching tools via tools/list...');
    const toolsResponse = await client.listTools({});
    const tools = toolsResponse.tools || [];
    console.log(`Received ${tools.length} tools from live server`);
    console.log('');

    // Count total actions
    let totalActions = 0;
    for (const tool of tools) {
      const inputSchema = tool.inputSchema;
      const actions = inputSchema?.properties?.action?.enum || [];
      totalActions += actions.length;
    }

    // Generate markdown documentation
    const lines = [
      '# Live Action Reference',
      '',
      '> **Generated from LIVE MCP server** - This documents exactly what an LLM client sees at runtime.',
      '> Unlike the static action reference, this is fetched via the actual MCP protocol.',
      '',
      `Generated: ${new Date().toISOString()}`,
      '',
      '## Summary',
      '',
      '| Metric | Value |',
      '|--------|-------|',
      `| Total Tools | ${tools.length} |`,
      `| Total Actions | ${totalActions} |`,
      `| Server Command | \`${serverCommand} ${serverArgs.join(' ')}\` |`,
      '',
      '## Table of Contents',
      '',
    ];

    // Generate TOC with unique tools
    const seenTools = new Set();
    for (const tool of tools) {
      if (seenTools.has(tool.name)) continue;
      seenTools.add(tool.name);
      const inputSchema = tool.inputSchema;
      const actions = inputSchema?.properties?.action?.enum || [];
      const anchor = tool.name.replace(/_/g, '-');
      lines.push(`- [${tool.name}](#${anchor}) (${actions.length} actions)`);
    }
    lines.push('');
    lines.push('---');
    lines.push('');

    // Generate tool sections
    seenTools.clear();
    for (const tool of tools) {
      if (seenTools.has(tool.name)) continue;
      seenTools.add(tool.name);

      lines.push(`## ${tool.name}`);
      lines.push('');
      
      // Description from MCP
      if (tool.description) {
        lines.push(tool.description);
        lines.push('');
      }

      // Get input schema details
      const inputSchema = tool.inputSchema;
      const actions = inputSchema?.properties?.action?.enum || [];
      const requiredFields = inputSchema?.required || [];

      // Show required fields
      if (requiredFields.length > 0) {
        lines.push(`**Required fields:** ${requiredFields.map(f => `\`${f}\``).join(', ')}`);
        lines.push('');
      }

      // Actions table
      if (actions.length > 0) {
        lines.push('### Actions');
        lines.push('');
        lines.push(`*${actions.length} actions available*`);
        lines.push('');
        lines.push('| # | Action |');
        lines.push('|---|--------|');
        for (let i = 0; i < actions.length; i++) {
          lines.push(`| ${i + 1} | \`${actions[i]}\` |`);
        }
        lines.push('');
      } else {
        lines.push('*No actions defined*');
        lines.push('');
      }

      // Show other input properties (excluding action)
      const otherProps = Object.keys(inputSchema?.properties || {}).filter(k => k !== 'action');
      if (otherProps.length > 0) {
        lines.push('### Input Parameters');
        lines.push('');
        lines.push('| Parameter | Type | Required |');
        lines.push('|-----------|------|----------|');
        for (const prop of otherProps) {
          const propDef = inputSchema.properties[prop];
          const type = propDef?.type || 'unknown';
          const isRequired = requiredFields.includes(prop);
          lines.push(`| \`${prop}\` | ${type} | ${isRequired ? 'Yes' : 'No'} |`);
        }
        lines.push('');
      }

      lines.push('---');
      lines.push('');
    }

    // Add generation metadata
    lines.push('## Generation Metadata');
    lines.push('');
    lines.push('```json');
    lines.push(JSON.stringify({
      generatedAt: new Date().toISOString(),
      serverCommand,
      serverArgs,
      toolCount: tools.length,
      actionCount: totalActions,
      mockedConnection: serverEnv.MOCK_UNREAL_CONNECTION === 'true'
    }, null, 2));
    lines.push('```');
    lines.push('');

    // Ensure docs directory exists
    mkdirSync(dirname(outputPath), { recursive: true });

    // Write file
    writeFileSync(outputPath, lines.join('\n'));
    
    console.log('='.repeat(60));
    console.log('Generation Complete');
    console.log('='.repeat(60));
    console.log(`Output: ${outputPath}`);
    console.log(`Tools documented: ${tools.length}`);
    console.log(`Total actions: ${totalActions}`);
    console.log('');

  } catch (error) {
    console.error('Failed to generate docs from live server:', error);
    process.exit(1);
  } finally {
    // Clean up
    if (client) {
      try {
        await client.close();
      } catch { /* ignore */ }
    }
    if (transport) {
      try {
        await transport.close();
      } catch { /* ignore */ }
    }
  }
}

// Run the generator
generateDocsFromLiveServer();
