import type { ToolDefinition } from '../definitions/shared/tool-definition.js';

// The one public tool's description. Mirrored byte-for-byte by the native
// gateway (McpNativeGatewayDefinition.cpp; a unit test diffs the two) and written
// for the weakest model that will read it: an ordered procedure, the
// copy-the-nextCall rule, and the three mistakes behind most wrong calls
// (guessed names, action inside params, retrying without reading suggestions).
// Nothing in it is specific to one transport: search rows and describe replies
// carry a nextCall on both surfaces, so "send the nextCall" is always correct.
export const UNREAL_GATEWAY_DESCRIPTION =
  'Unreal Engine editor automation: one tool, four operations. Always in this order: '
  + "(1) search with query = 2-4 plain words naming the verb and the object, e.g. 'spawn actor', 'add variable blueprint', 'save level'; every result row carries a one-line summary, its effect and a ready-made nextCall. "
  + "(2) describe: send the chosen row's nextCall unchanged; the reply is the exact contract: parameters (name, type, required), inputSchema, a working example and, when required, consentGrant. "
  + "(3) execute: send describe's nextCall with params filled in, using ONLY the parameter names describe listed. "
  + 'Rules: never guess capability ids, tool names, action names or parameter names; never put action or subAction inside params; a failed call returns suggestions and an executable nextCall, so send that nextCall instead of retrying blindly. '
  + 'To browse instead of search, call describe with no selector and follow the nextCall on each row one level down. '
  + 'configure only enables or disables internal tool groups; it never performs editor work.';

// Served as MCP `instructions` at initialize on both transports (the native
// surface reads the same text from Resources/MCP/server-info.json; a unit test
// keeps them equal). Longer than the tool description because a client injects
// it once per session, not once per tool listing.
export const UNREAL_GATEWAY_INSTRUCTIONS = [
  'Unreal Engine MCP server. The only tool is `unreal`; it wraps 1,400+ editor capabilities behind four operations: search, describe, execute, configure.',
  '',
  'Workflow for every task:',
  '1. search: {"operation":"search","query":"spawn actor"}. Use 2-4 plain words (verb + object), not a sentence. Each result row has a one-line summary, an effect (read, write or destructive) and a nextCall. Pick the row whose summary matches the task. If none fits, retry with different words, narrow with a filter (domain, tool, effect), or browse by calling describe with no selector and following each row\'s nextCall one level down.',
  '2. describe: send the chosen row\'s nextCall unchanged. The reply is the exact contract for that one action: parameters[] (name, type, required, description), inputSchema, examples[] with a working input, consentGrant when consent is required, and the nextCall for execute.',
  '3. execute: send describe\'s nextCall with params filled in. Use only the parameter names describe listed, with the same casing. Never put action or subAction inside params. If describe returned consentGrant, pass it back as the top-level consent field.',
  '',
  'Recovery: every failed call returns errorCode, message, suggestions[] and an executable nextCall. Send that nextCall (adjusting params) instead of guessing another tool, action or capability. UNKNOWN_TOOL, UNKNOWN_ACTION and UNKNOWN_CAPABILITY mean a name was invented: go back to search. INVALID_PARAMS names the offending field in pointer and lists allowedParameters. NOT_CONNECTED means the Unreal Editor is not running with the bridge; report it instead of retrying.',
  '',
  'Conventions: asset paths start with /Game (e.g. /Game/Blueprints/BP_Door); class paths look like /Script/Engine.Actor; vectors are objects {x, y, z}. Read with inspect-style capabilities before mutating, and never run a destructive capability without the consentGrant describe returned. configure only enables or disables internal tool groups; it never performs editor work.'
].join('\n');

export const unrealGatewayToolDefinition: ToolDefinition = {
  name: 'unreal',
  description: UNREAL_GATEWAY_DESCRIPTION,
  inputSchema: {
    type: 'object',
    properties: {
      operation: {
        type: 'string',
        enum: ['search', 'describe', 'execute', 'configure'],
        description: "search finds capabilities by plain words. describe returns one capability's exact contract, or browses one level when given no selector. execute runs one validated action. configure enables or disables internal tool groups."
      },
      query: { type: 'string', description: "2-4 plain words naming the verb and the object, e.g. 'spawn actor', 'add variable blueprint', 'save level'. Matched against action names, topics, family, domain and summary; full sentences and filler words rank worse." },
      capability: { type: 'string', description: 'Exact canonical capability id (or declared alias) copied from a search row or describe response, e.g. blueprint.create. Preferred selector for describe and execute; never guessed.' },
      domain: { type: 'string', description: 'Capability domain to browse or filter by. Call describe with no selector to list domains.' },
      family: { type: 'string', description: 'Capability family inside a domain. Call describe with a domain to list its families.' },
      effect: { type: 'string', enum: ['read', 'write', 'destructive'], description: 'Filter search results by declared behavior effect.' },
      tool: { type: 'string', description: 'Exact parent tool name copied from a search row (parentTool) or a describe response. Always paired with action; never guessed.' },
      action: { type: 'string', description: 'Exact action name copied from a search row or describe response. For configure, this is a manage_tools action.' },
      param: { type: 'string', description: 'Exact parameter name to inspect on one capability. Use with describe plus capability, or plus tool and action; returns that single parameter\x27s schema.' },
      params: { type: 'object', additionalProperties: true, description: 'Parameters for execute or configure: an object whose keys are exactly the parameter names describe listed for this action, with the same casing. Never include action or subAction here.' },
      consent: {
        type: 'object',
        properties: {
          capability: { type: 'string', description: 'Exact canonical capability ID this grant authorizes, as returned by describe.' },
          acknowledge: { type: 'string', enum: ['explicit', 'elevated'], description: "Acknowledgement strength. 'explicit' satisfies an explicit policy; 'elevated' is required by a destructive policy and also satisfies explicit." }
        },
        required: ['capability', 'acknowledge'],
        additionalProperties: false,
        description: "Per-call consent grant for a capability whose policy.consent is not 'none'. Bound to one capability and one call; never persisted, inherited or reused. Read the exact grant from describe.consentGrant. Use with execute only."
      },
      options: {
        type: 'object',
        properties: {
          idempotencyKey: { type: 'string', description: 'Client-chosen key. A repeated execute with the same key and params returns the recorded receipt instead of running the action again.' },
          expectedCatalogRevision: { type: 'string', description: 'Refuse to run unless the catalog revision still equals this value; read it from any search or describe response.' },
          expectedRevisions: {
            type: 'object',
            properties: {
              selection: { type: 'integer', minimum: 1 },
              level: { type: 'integer', minimum: 1 },
              assetRegistry: { type: 'integer', minimum: 1 },
              package: { type: 'integer', minimum: 1 }
            },
            additionalProperties: false,
            description: 'Live editor-state revisions to pin; the call is refused with STALE_STATE when one has moved.'
          },
          timeoutMs: { type: 'integer', minimum: 1, maximum: 600000, description: 'Deadline for this call in milliseconds.' }
        },
        additionalProperties: false,
        description: 'Execution controls for execute only; never put these inside params. Honored keys: idempotencyKey, expectedCatalogRevision, expectedRevisions, timeoutMs.'
      },
      limit: { type: 'integer', minimum: 1, maximum: 25, description: 'Maximum rows per page: search results (default 12) or describe action rows (default 20). Defaults to 12.' },
      offset: { type: 'integer', minimum: 0, description: 'Zero-based offset into search results or into a described tool\x27s action list. Defaults to 0.' },
      cursor: { type: 'string', description: 'Opaque search cursor from a previous response nextCursor. Supersedes offset.' },
      maxBytes: { type: 'integer', minimum: 512, maximum: 262144, description: 'Serialized byte ceiling for a search response. Results are dropped from the end until the response fits.' }
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
      data: { description: "Execute payload projected against the capability's declared output schema. Bound to receipt.dataDigest, and the same location the native /mcp surface publishes." },
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
      browse: { type: 'object', description: 'On a tool-only describe: a search call filtered to that tool, whose rows carry per-action summaries.' },
      suggestions: { type: 'array', description: 'Closest-match names for an invalid tool/action/param call.' },
      nextCall: { type: 'object', description: 'Directly-invokable gateway request for guided self-correction.' },
      availableActions: { type: 'array' },
      availableParameters: { type: 'array' },
      catalogRevision: { type: 'string', description: 'Revision of the generated canonical capability catalog this response was served from.' },
      capability: { type: 'string', description: 'Canonical capability ID this response describes.' },
      parentTool: { type: 'string', description: 'Legacy parent tool that dispatches the described capability.' },
      domain: { type: 'string' },
      family: { type: 'string' },
      domains: { type: 'array', description: 'Bounded domain list on a catalog-level describe.' },
      families: { type: 'array', description: 'Bounded family list on a domain-level describe.' },
      capabilities: { type: 'array', description: 'Bounded capability list on a family-level describe.' },
      inputSchema: { type: 'object', description: 'Exact input schema of the described capability. Never a parent-tool union.' },
      outputSchema: { type: 'object', description: 'Exact output schema of the described capability.' },
      availability: { description: 'Whether the capability is available, disabled or unavailable, and why.' },
      behavior: { type: 'object' },
      policy: { type: 'object' },
      consentGrant: { type: 'object', description: "Exact consent grant this capability requires, ready to pass back as the execute call's consent sibling. Absent when policy.consent is 'none'." },
      cost: { type: 'object' },
      deprecation: { type: 'object' },
      hashes: { type: 'object', description: 'Per-record schema and content hashes from the generated catalog.' },
      runnable: { type: 'boolean', description: 'False when the capability cannot currently be executed; nextCall then points at the fix.' },
      migratedFrom: { type: 'object', description: 'Legacy tool/action pair that resolved to this capability.' },
      resolvedFromAlias: { type: 'string', description: 'Declared alias that resolved to this capability.' },
      nextCursor: { type: 'string', description: 'Cursor to pass back as cursor to continue a search.' },
      maxBytes: { type: 'integer' },
      truncated: { type: 'boolean', description: 'True when results were dropped to fit the byte budget.' },
      filters: { type: 'object', description: 'Filters applied to this search.' },
      reasons: { type: 'array' },
      outputs: { type: 'array' }
    },
    required: ['success', 'operation'],
    additionalProperties: true
  }
};
