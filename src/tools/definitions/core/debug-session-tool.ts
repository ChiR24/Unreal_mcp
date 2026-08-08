import type { ToolDefinition } from '../shared/tool-definition.js';
import { debugCommonProperties, debugOutputSchema } from './debug-definition-common.js';

export const debugSessionToolDefinition: ToolDefinition = {
  name: 'debug_session',
  category: 'core',
  description: 'Manage responsive PIE observation and VS Code-hosted native Unreal debug sessions.',
  inputSchema: {
    type: 'object',
    properties: {
      action: { type: 'string', enum: ['list_targets', 'start', 'status', 'pause', 'continue', 'next', 'step_in', 'step_out', 'stop'], description: 'Debug session operation.' },
      ...debugCommonProperties,
      mode: { type: 'string', enum: ['pie_observe', 'standalone_debug', 'attach'] },
      projectPath: { type: 'string' },
      map: { type: 'string' },
      targetPid: { type: 'integer', minimum: 1 },
      terminate: { type: 'boolean', description: 'Terminate rather than detach; requires unsafe authorization.' },
      stopOnEntry: { type: 'boolean' },
      arguments: { type: 'array', items: { type: 'string' } }
    },
    required: ['action']
  },
  outputSchema: debugOutputSchema
};
