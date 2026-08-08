import assert from 'node:assert/strict';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StdioClientTransport } from '@modelcontextprotocol/sdk/client/stdio.js';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const transport = new StdioClientTransport({
  command: process.execPath,
  args: [path.join(repoRoot, 'dist', 'cli.js')],
  cwd: repoRoot,
  env: {
    ...process.env,
    UE_PROJECT_PATH: 'C:/Users/jeehoon/UnrealEngine_Projects/Missile_demo',
    UE_ENGINE_PATH: 'C:/Program Files/Epic Games/UE_5.7'
  }
});
const client = new Client({ name: 'unreal-mcp-debug-smoke', version: '1.0.0' }, { capabilities: {} });

try {
  await client.connect(transport);

  const tools = (await client.listTools()).tools.map((tool) => tool.name);
  for (const name of ['debug_session', 'debug_breakpoint', 'debug_inspect', 'debug_observe']) {
    assert.ok(tools.includes(name), `missing MCP tool ${name}`);
  }

  const resources = (await client.listResources()).resources.map((resource) => resource.uri);
  assert.ok(resources.includes('ue://debug/sessions'));
  assert.ok(resources.includes('ue://debug/health'));

  const templates = (await client.listResourceTemplates()).resourceTemplates.map((template) => template.uriTemplate);
  for (const uri of [
    'ue://debug/session/{sessionId}',
    'ue://debug/events/{sessionId}?after={cursor}&limit={limit}',
    'ue://debug/jobs/{jobId}',
    'ue://debug/artifacts/{artifactId}'
  ]) assert.ok(templates.includes(uri), `missing resource template ${uri}`);

  const response = await client.callTool({ name: 'debug_session', arguments: { action: 'list_targets' } });
  assert.notEqual(response.isError, true);
  assert.equal(response.structuredContent?.success, true);
  assert.ok(Array.isArray(response.structuredContent?.targets));
  if (process.env.EXPECT_DEBUG_HOST === 'true') {
    assert.equal(response.structuredContent?.debugHost?.connected, true);
    assert.ok(response.structuredContent.targets.some(
      (target) => target.mode === 'standalone_debug' && target.available === true
    ));
  }

  console.log(`debug sidecar smoke passed (${tools.length} tools, ${templates.length} templates)`);
} finally {
  await client.close();
}
