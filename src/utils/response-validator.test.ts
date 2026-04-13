import { describe, expect, it } from 'vitest';

import { ResponseValidator } from './response-validator.js';

describe('ResponseValidator', () => {
  it('preserves bounded graph fields in structuredContent for wrapped responses', async () => {
    const validator = new ResponseValidator();

    const wrapped = await validator.wrapResponse('manage_blueprint', {
      success: true,
      shown: 2,
      totalRequested: 4,
      truncated: true,
      nextCursor: 'cursor:2',
      nodes: [
        { nodeId: 'NODE_2', nodeTitle: 'Print String', pins: [] },
        { nodeId: 'NODE_1', nodeTitle: 'Event BeginPlay', pins: [] }
      ]
    });

    expect(wrapped.success).toBe(true);
    expect(wrapped.structuredContent).toMatchObject({
      shown: 2,
      totalRequested: 4,
      truncated: true,
      nextCursor: 'cursor:2',
      nodes: [
        { nodeId: 'NODE_2', nodeTitle: 'Print String', pins: [] },
        { nodeId: 'NODE_1', nodeTitle: 'Event BeginPlay', pins: [] }
      ]
    });
    expect(wrapped.isError).toBeUndefined();
  });

  it('promotes obvious bounded graph failures to top-level isError without dropping structuredContent', async () => {
    const validator = new ResponseValidator();

    const wrapped = await validator.wrapResponse('manage_blueprint', {
      success: false,
      error: 'NODE_NOT_FOUND',
      shown: 0,
      totalRequested: 3,
      truncated: false,
      nextCursor: '',
      nodes: []
    });

    expect(wrapped.isError).toBe(true);
    expect(wrapped.structuredContent).toMatchObject({
      success: false,
      error: 'NODE_NOT_FOUND',
      shown: 0,
      totalRequested: 3,
      truncated: false,
      nextCursor: '',
      nodes: []
    });
  });

  it('preserves bounded graph review summary arrays in structuredContent', async () => {
    const validator = new ResponseValidator();

    const wrapped = await validator.wrapResponse('manage_blueprint', {
      success: true,
      graphName: 'ReviewFunction',
      nodeCount: 4,
      connectionCount: 6,
      truncated: true,
      entryNodes: [{ nodeId: 'NODE_ENTRY', nodeTitle: 'Function Entry' }],
      commentGroups: [{ nodeId: 'NODE_COMMENT', nodeTitle: 'Comment', nodeCount: 3 }],
      highFanOutNodes: [{ nodeId: 'NODE_BRANCH', nodeTitle: 'Branch', outgoingLinkCount: 3 }],
      reviewTargets: [
        { nodeId: 'NODE_ENTRY', nodeTitle: 'Function Entry', reason: 'entry_node' },
        { nodeId: 'NODE_BRANCH', nodeTitle: 'Branch', reason: 'high_fan_out' }
      ],
      focusedReviewTarget: { nodeId: 'NODE_BRANCH', nodeTitle: 'Branch', reason: 'high_fan_out' },
      incomingNodes: [{ nodeId: 'NODE_ENTRY', nodeTitle: 'Function Entry', linkCount: 1 }],
      outgoingNodes: [{ nodeId: 'NODE_PRINT', nodeTitle: 'Print String', linkCount: 1 }],
      containingCommentGroup: { nodeId: 'NODE_COMMENT', nodeTitle: 'Comment', nodeCount: 3 },
      focusTruncated: false
    });

    expect(wrapped.success).toBe(true);
    expect(wrapped.structuredContent).toMatchObject({
      graphName: 'ReviewFunction',
      nodeCount: 4,
      connectionCount: 6,
      truncated: true,
      entryNodes: [{ nodeId: 'NODE_ENTRY', nodeTitle: 'Function Entry' }],
      commentGroups: [{ nodeId: 'NODE_COMMENT', nodeTitle: 'Comment', nodeCount: 3 }],
      highFanOutNodes: [{ nodeId: 'NODE_BRANCH', nodeTitle: 'Branch', outgoingLinkCount: 3 }],
      reviewTargets: [
        { nodeId: 'NODE_ENTRY', nodeTitle: 'Function Entry', reason: 'entry_node' },
        { nodeId: 'NODE_BRANCH', nodeTitle: 'Branch', reason: 'high_fan_out' }
      ],
      focusedReviewTarget: { nodeId: 'NODE_BRANCH', nodeTitle: 'Branch', reason: 'high_fan_out' },
      incomingNodes: [{ nodeId: 'NODE_ENTRY', nodeTitle: 'Function Entry', linkCount: 1 }],
      outgoingNodes: [{ nodeId: 'NODE_PRINT', nodeTitle: 'Print String', linkCount: 1 }],
      containingCommentGroup: { nodeId: 'NODE_COMMENT', nodeTitle: 'Comment', nodeCount: 3 },
      focusTruncated: false
    });
  });
});