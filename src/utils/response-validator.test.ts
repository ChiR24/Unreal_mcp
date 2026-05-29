import { describe, expect, it } from 'vitest';
import { ResponseValidator } from './response-validator.js';

describe('ResponseValidator', () => {
  it('summarizes pin arrays without malformed JSON fragments', async () => {
    const validator = new ResponseValidator();

    const wrapped = await validator.wrapResponse('manage_blueprint', {
      success: true,
      message: 'Pin details retrieved.',
      result: {
        nodeId: 'NodeA',
        pins: [
          {
            pinName: 'InString',
            direction: 'Input',
            pinType: 'string',
            linkedTo: [],
            defaultValue: 'test'
          }
        ],
        assetPath: '/Game/Test/BP_Test',
        existsAfter: true
      }
    });

    const content = wrapped.content;
    expect(Array.isArray(content)).toBe(true);
    const firstContent = Array.isArray(content) ? content[0] : undefined;
    const text = firstContent && typeof firstContent === 'object' && 'text' in firstContent && typeof firstContent.text === 'string'
      ? firstContent.text
      : '';

    expect(text).toContain('pinName=InString');
    expect(text).toContain('pinType=string');
    expect(text).toContain('linkedTo=0');
    expect(text).not.toContain('pinType]');
  });

  it('marks already MCP-shaped failure responses as errors', async () => {
    const validator = new ResponseValidator();

    const wrapped = await validator.wrapResponse('inspect', {
      success: false,
      content: [{ type: 'text', text: 'Inspection failed' }],
      error: 'Object not found'
    });

    expect(wrapped.isError).toBe(true);
    expect(wrapped.content).toEqual([{ type: 'text', text: 'Inspection failed' }]);
  });
});
