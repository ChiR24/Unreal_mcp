export const debugContextSchema = {
  type: 'object',
  properties: {
    requestId: { type: 'string' },
    traceId: { type: 'string' },
    debugSessionId: { type: 'string' },
    targetPid: { type: 'integer' },
    worldInstance: { type: 'string' },
    frame: { type: 'integer' },
    thread: { type: 'integer' },
    timestamp: { type: 'string' },
    eventCursor: { type: 'integer' }
  },
  required: ['traceId', 'timestamp']
};

export const debugOutputSchema = {
  type: 'object',
  properties: {
    success: { type: 'boolean' },
    error: { type: 'string' },
    context: debugContextSchema,
    diagnostic: { type: 'object' },
    result: {},
    session: { type: 'object' },
    sessions: { type: 'array' },
    targets: { type: 'array' },
    events: { type: 'array' },
    snapshot: {},
    job: { type: 'object' },
    artifact: { type: 'object' },
    debugHost: { type: 'object' },
    nextCursor: { type: 'integer' },
    oldestCursor: { type: 'integer' },
    dropped: { type: 'integer' },
    recording: { type: 'boolean' },
    cursor: { type: 'integer' },
    sessionId: { type: 'string' },
    stale: { type: 'boolean' }
  },
  required: ['success', 'context']
};

export const debugCommonProperties = {
  sessionId: { type: 'string', description: 'Sidecar debug-session identifier.' },
  requestId: { type: 'string', description: 'Caller correlation identifier.' },
  traceId: { type: 'string', description: 'Trace identifier propagated through sidecar, host and Unreal.' },
  unsafe: { type: 'boolean', description: 'Explicit authorization for operations gated by UE_MCP_DEBUG_ALLOW_UNSAFE=true.' }
};
