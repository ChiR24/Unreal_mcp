import { describe, expect, it } from 'vitest';
import { consolidatedToolDefinitions } from '../../tools/catalog/consolidated-tool-definitions.js';
import { unrealGatewayToolDefinition } from '../../tools/catalog/unreal-gateway-definition.js';
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

  it('compiles a schema with the boolean x-unreal-reflection-boundary annotation', () => {
    const validator = new ResponseValidator();

    const schema = {
      type: 'object',
      properties: {
        data: {
          type: 'object',
          'x-unreal-reflection-boundary': true
        }
      }
    };

    expect(() => validator.registerSchema('ue_reflection_tool', schema)).not.toThrow();
  });

  it('passes valid freeform reflection data through an annotated schema', async () => {
    const validator = new ResponseValidator();

    const schema = {
      type: 'object',
      properties: {
        data: {
          type: 'object',
          'x-unreal-reflection-boundary': true
        }
      }
    };
    validator.registerSchema('ue_reflection_tool', schema);

    const response = {
      success: true,
      data: {
        className: 'AActor',
        properties: { health: 100, name: 'Hero' }
      }
    };

    const result = await validator.validateResponse('ue_reflection_tool', response);
    expect(result.valid).toBe(true);
  });

  it('fails malformed schema-backed response data for an annotated schema', async () => {
    const validator = new ResponseValidator();

    const schema = {
      type: 'object',
      properties: {
        data: {
          type: 'object',
          'x-unreal-reflection-boundary': true
        }
      }
    };
    validator.registerSchema('ue_reflection_tool', schema);

    const response = {
      success: true,
      data: 'not-an-object'
    };

    const result = await validator.validateResponse('ue_reflection_tool', response);
    expect(result.valid).toBe(false);
    expect(Array.isArray(result.errors)).toBe(true);
    expect(result.errors?.length).toBeGreaterThan(0);
  });

  it('registers all 23 canonical parent schemas plus the gateway (24 total)', () => {
    const validator = new ResponseValidator();

    // Mirror the exact registration order used by server-factory.createServer().
    for (const tool of consolidatedToolDefinitions) {
      if (tool.outputSchema) {
        validator.registerSchema(tool.name, tool.outputSchema);
      }
    }
    if (unrealGatewayToolDefinition.outputSchema) {
      validator.registerSchema(unrealGatewayToolDefinition.name, unrealGatewayToolDefinition.outputSchema);
    }

    const stats = validator.getStats();
    expect(stats.totalSchemas).toBe(24);
    expect(stats.tools).toContain('manage_sequence');
    expect(stats.tools).toContain('unreal');
  });
});
