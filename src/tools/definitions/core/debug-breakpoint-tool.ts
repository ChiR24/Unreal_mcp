import type { ToolDefinition } from '../shared/tool-definition.js';
import { debugCommonProperties, debugOutputSchema } from './debug-definition-common.js';

export const debugBreakpointToolDefinition: ToolDefinition = {
  name: 'debug_breakpoint',
  category: 'core',
  description: 'Create, update, remove and enumerate source, function, exception and log breakpoints.',
  inputSchema: {
    type: 'object',
    properties: {
      action: { type: 'string', enum: ['upsert', 'remove', 'list', 'clear'], description: 'Breakpoint operation.' },
      ...debugCommonProperties,
      breakpointId: { type: 'string' },
      kind: { type: 'string', enum: ['source', 'function', 'exception', 'log'] },
      source: { type: 'string' },
      line: { type: 'integer', minimum: 1 },
      column: { type: 'integer', minimum: 1 },
      function: { type: 'string' },
      exception: { type: 'string' },
      condition: { type: 'string' },
      hitCondition: { type: 'string' },
      logMessage: { type: 'string' },
      enabled: { type: 'boolean' }
    },
    required: ['action', 'sessionId']
  },
  outputSchema: debugOutputSchema
};
