import type { ToolDefinition } from '../shared/tool-definition.js';
import { debugCommonProperties, debugOutputSchema } from './debug-definition-common.js';

export const debugInspectToolDefinition: ToolDefinition = {
  name: 'debug_inspect',
  category: 'core',
  description: 'Inspect stopped native Unreal threads, stacks, scopes, variables, expressions and memory through DAP.',
  inputSchema: {
    type: 'object',
    properties: {
      action: { type: 'string', enum: ['threads', 'stack', 'scopes', 'variables', 'evaluate', 'read_memory', 'snapshot'], description: 'Debugger inspection operation.' },
      ...debugCommonProperties,
      threadId: { type: 'integer' },
      frameId: { type: 'integer' },
      variablesReference: { type: 'integer' },
      expression: { type: 'string' },
      context: { type: 'string', enum: ['watch', 'repl', 'hover'] },
      memoryReference: { type: 'string' },
      offset: { type: 'integer' },
      count: { type: 'integer', minimum: 1, maximum: 1048576 },
      startFrame: { type: 'integer', minimum: 0 },
      levels: { type: 'integer', minimum: 1, maximum: 1000 }
    },
    required: ['action', 'sessionId']
  },
  outputSchema: debugOutputSchema
};
