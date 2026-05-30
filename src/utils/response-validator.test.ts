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

  it('emits MCP image content for base64 screenshot payloads', async () => {
    const validator = new ResponseValidator();

    const wrapped = await validator.wrapResponse('system_control', {
      success: true,
      mode: 'full_editor_window',
      imageBase64: 'iVBORw0KGgo=',
      mimeType: 'image/png',
      width: 10,
      height: 20,
      sizeBytes: 8
    });

    const content = wrapped.content;
    expect(Array.isArray(content)).toBe(true);
    const imageContent = Array.isArray(content)
      ? content.find((part): part is Record<string, unknown> => typeof part === 'object' && part !== null && 'type' in part && part.type === 'image')
      : undefined;
    const textContent = Array.isArray(content)
      ? content.find((part): part is Record<string, unknown> => typeof part === 'object' && part !== null && 'type' in part && part.type === 'text')
      : undefined;

    expect(imageContent).toMatchObject({
      type: 'image',
      data: 'iVBORw0KGgo=',
      mimeType: 'image/png'
    });
    expect(textContent?.text).not.toContain('iVBORw0KGgo=');
    expect(wrapped.structuredContent).toMatchObject({
      imageBase64: 'iVBORw0KGgo=',
      mimeType: 'image/png'
    });
  });

  it('emits MCP image content for nested automation response payloads', async () => {
    const validator = new ResponseValidator();

    const wrapped = await validator.wrapResponse('system_control', {
      type: 'automation_response',
      requestId: 'request-1',
      success: true,
      message: 'Full editor window screenshot captured',
      result: {
        success: true,
        imageBase64: 'iVBORw0KGgo=',
        mimeType: 'image/png',
        width: 10,
        height: 20
      }
    });

    const content = wrapped.content;
    expect(Array.isArray(content)).toBe(true);
    const imageContent = Array.isArray(content)
      ? content.find((part): part is Record<string, unknown> => typeof part === 'object' && part !== null && 'type' in part && part.type === 'image')
      : undefined;
    const textContent = Array.isArray(content)
      ? content.find((part): part is Record<string, unknown> => typeof part === 'object' && part !== null && 'type' in part && part.type === 'text')
      : undefined;

    expect(imageContent).toMatchObject({
      type: 'image',
      data: 'iVBORw0KGgo=',
      mimeType: 'image/png'
    });
    expect(textContent?.text).not.toContain('iVBORw0KGgo=');
    expect(wrapped.structuredContent).toMatchObject({
      result: {
        imageBase64: 'iVBORw0KGgo=',
        mimeType: 'image/png'
      }
    });
  });
});
