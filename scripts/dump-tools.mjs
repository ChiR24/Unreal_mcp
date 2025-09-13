import { writeFileSync, mkdirSync } from 'fs';
import { dirname } from 'path';
import { toolDefinitions } from '../dist/tools/tool-definitions.js';
import { consolidatedToolDefinitions } from '../dist/tools/consolidated-tool-definitions.js';

const payload = {
  generatedAt: new Date().toISOString(),
  server: '@unrealengine/mcp-server',
  version: '1.2.0',
  consolidated: consolidatedToolDefinitions.map(t => ({
    name: t.name,
    description: t.description,
    inputSchema: t.inputSchema,
    outputSchema: t.outputSchema
  })),
  individual: toolDefinitions.map(t => ({
    name: t.name,
    description: t.description,
    inputSchema: t.inputSchema,
    outputSchema: t.outputSchema
  }))
};

const target = 'X:\\GitHub\\mcp-registry\\servers\\unreal-engine-mcp\\tools.json';
mkdirSync(dirname(target), { recursive: true });
writeFileSync(target, JSON.stringify(payload, null, 2));
console.log('Wrote', target);
