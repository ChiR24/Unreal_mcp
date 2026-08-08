import type { ToolDefinition } from '../shared/tool-definition.js';
import { debugCommonProperties, debugOutputSchema } from './debug-definition-common.js';

export const debugObserveToolDefinition: ToolDefinition = {
  name: 'debug_observe',
  category: 'core',
  description: 'Query correlated events and Blueprint/probe diagnostics, and manage asynchronous tests, traces and bundles.',
  inputSchema: {
    type: 'object',
    properties: {
      action: {
        type: 'string',
        enum: ['query_events', 'blueprint_diagnostics', 'probe_snapshot', 'start_recording', 'stop_recording', 'run_tests', 'test_status', 'cancel_test', 'start_trace', 'stop_trace', 'trace_status', 'create_bundle'], description: 'Observability operation.'
      },
      ...debugCommonProperties,
      jobId: { type: 'string' },
      after: { type: 'integer', minimum: 0 },
      limit: { type: 'integer', minimum: 1, maximum: 1000 },
      event: { type: 'string' },
      category: { type: 'string' },
      severity: { type: 'string' },
      regex: { type: 'string' },
      since: { type: 'string' },
      until: { type: 'string' },
      testFilter: { type: 'string' },
      timeoutMs: { type: 'integer', minimum: 1 },
      channels: { type: 'string' }
    },
    required: ['action']
  },
  outputSchema: debugOutputSchema
};
