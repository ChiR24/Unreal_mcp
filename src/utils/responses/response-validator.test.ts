import { describe, expect, it } from 'vitest';
import { wrapGatewayResponse } from './response-validator.js';

type Part = Record<string, unknown>;
const parts = (wrapped: Record<string, unknown>): Part[] => (Array.isArray(wrapped.content) ? wrapped.content as Part[] : []);
const textOf = (wrapped: Record<string, unknown>): string => String(parts(wrapped).find((part) => part.type === 'text')?.text ?? '');
const imageOf = (wrapped: Record<string, unknown>): Part | undefined => parts(wrapped).find((part) => part.type === 'image');

describe('wrapGatewayResponse', () => {
  it('carries nested results whole, as valid JSON after the message line', () => {
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
    expect(text.split('\n')[0]).toBe('Pin details retrieved.');
    const body = JSON.parse(text.slice(text.indexOf('\n\n') + 2)) as { result: { pins: unknown[] } };
    expect(body.result.pins[0]).toMatchObject({ pinName: 'InString', direction: 'Input', pinType: 'string', defaultValue: 'test' });
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
