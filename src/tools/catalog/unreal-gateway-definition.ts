import type { ToolDefinition } from '../definitions/shared/tool-definition.js';

export const unrealGatewayToolDefinition: ToolDefinition = {
  name: 'unreal',
  description: 'Unreal Engine capability gateway. Search first, describe the exact contract, then execute a validated action. Use configure only to manage internal capability availability.',
  inputSchema: {
    type: 'object',
    properties: {
      operation: {
        type: 'string',
        enum: ['search', 'describe', 'execute', 'configure'],
        description: 'search finds capabilities. describe returns an exact parent-tool contract. execute runs one validated action. configure manages internal capability availability.'
      },
      query: { type: 'string', description: 'Search words for tools, categories, descriptions, or actions.' },
      tool: { type: 'string', description: 'Exact canonical parent tool name returned by search or describe.' },
      action: { type: 'string', description: 'Exact action name returned by describe. For configure, this is a manage_tools action.' },
      param: { type: 'string', description: 'Exact parameter name (tool-union catalog) to inspect. Requires tool and action for full drill-down; resolves the single parameter schema. Use with describe only.' },
      params: { type: 'object', description: 'Parameters for execute or configure. Never include action or subAction here.' },
      limit: { type: 'integer', minimum: 1, maximum: 25, description: 'Maximum search results to return. Defaults to 12.' },
      offset: { type: 'integer', minimum: 0, description: 'Zero-based search result offset. Defaults to 0.' }
    },
    required: ['operation'],
    additionalProperties: false
  },
  outputSchema: {
    type: 'object',
    properties: {
      success: { type: 'boolean' },
      operation: { type: 'string' },
      error: { type: 'string' },
      errorCode: { type: 'string' },
      message: { type: 'string' },
      results: { type: 'array' },
      result: {},
      tool: { type: 'string' },
      action: { type: 'string' },
      param: { type: 'string' },
      total: { type: 'integer' },
      offset: { type: 'integer' },
      limit: { type: 'integer' },
      hasMore: { type: 'boolean' },
      perActionSchemas: { type: 'boolean' },
      scope: { type: 'string', description: "Catalog scope: 'tool' for a tool summary, 'union' for the tool-union parameter catalog." },
      actions: { type: 'array', description: 'Paginated/filterable action list on tool-only describe.' },
      actionCount: { type: 'integer' },
      actionOffset: { type: 'integer' },
      actionLimit: { type: 'integer' },
      actionHasMore: { type: 'boolean' },
      parameters: { type: 'array', description: 'Paginated/filterable compact parameter catalog on tool+action describe.' },
      parameterCount: { type: 'integer' },
      parameterOffset: { type: 'integer' },
      parameterLimit: { type: 'integer' },
      parameterHasMore: { type: 'boolean' },
      required: { type: 'boolean', description: 'Whether the described param is required.' },
      schema: { type: 'object', description: 'Full schema of the single described param.' },
      drillDown: { type: 'object', description: 'Example nextCall payload to drill one level deeper.' },
      suggestions: { type: 'array', description: 'Closest-match names for an invalid tool/action/param call.' },
      nextCall: { type: 'object', description: 'Directly-invokable gateway request for guided self-correction.' },
      availableActions: { type: 'array' },
      availableParameters: { type: 'array' }
    },
    required: ['success', 'operation'],
    additionalProperties: true
  }
};
