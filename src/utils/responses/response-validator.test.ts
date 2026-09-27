import { describe, expect, it } from 'vitest';
import { wrapGatewayResponse } from './response-validator.js';

type Part = Record<string, unknown>;
const parts = (wrapped: Record<string, unknown>): Part[] => (Array.isArray(wrapped.content) ? wrapped.content as Part[] : []);
const textOf = (wrapped: Record<string, unknown>): string => String(parts(wrapped).find((part) => part.type === 'text')?.text ?? '');
const imageOf = (wrapped: Record<string, unknown>): Part | undefined => parts(wrapped).find((part) => part.type === 'image');

describe('wrapGatewayResponse', () => {
  it('summarizes pin arrays without malformed JSON fragments', () => {
    const wrapped = wrapGatewayResponse({
      success: true,
      operation: 'execute',
      message: 'Pin details retrieved.',
      result: {
        nodeId: 'NodeA',
        pins: [{ pinName: 'InString', direction: 'Input', pinType: 'string', linkedTo: [], defaultValue: 'test' }]
      }
    });

    const text = textOf(wrapped);
    expect(text).toContain('pinName=InString');
    expect(text).toContain('pinType=string');
    expect(text).toContain('linkedTo=0');
    expect(text).not.toContain('pinType]');
  });

  it('marks a success:false envelope as an MCP error', () => {
    const wrapped = wrapGatewayResponse({ success: false, operation: 'execute', error: 'Object not found' });
    expect(wrapped.isError).toBe(true);
    expect(wrapped.success).toBe(false);
  });

  it('emits MCP image content for a base64 screenshot and keeps the bytes out of the text', () => {
    const wrapped = wrapGatewayResponse({
      success: true,
      operation: 'execute',
      result: { imageBase64: 'iVBORw0KGgo=', mimeType: 'image/png', width: 10, height: 20 }
    });

    expect(imageOf(wrapped)).toMatchObject({ type: 'image', data: 'iVBORw0KGgo=', mimeType: 'image/png' });
    expect(textOf(wrapped)).not.toContain('iVBORw0KGgo=');
  });

  it('reports an envelope that breaks the unreal output schema instead of throwing', () => {
    const wrapped = wrapGatewayResponse({ success: 'yes' });
    expect(wrapped._validation).toMatchObject({ valid: false });
  });
});
