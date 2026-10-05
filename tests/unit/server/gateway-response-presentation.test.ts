import { describe, expect, it } from 'vitest';

import { isRecord } from '../../../src/utils/validation/type-guards.js';
import { connectClient, structuredPayload } from '../support/in-memory-server.js';

function contentParts(response: unknown): readonly Record<string, unknown>[] {
  if (!isRecord(response) || !Array.isArray(response.content)) return [];
  return response.content.filter(isRecord);
}

describe('gateway response presentation over MCP', () => {
  it('keeps an output-contract failure visible when the handler reports success', async () => {
    // Given: the handler ran, but returned settings in a shape the contract refuses.
    const ctx = await connectClient('response-failure-test', async () => ({
      success: true,
      message: 'Handler completed.',
      settings: 'invalid settings shape',
      warnings: ['Inspect the current state before retrying.']
    }));
    try {
      // When
      const response = await ctx.client.callTool({
        name: 'unreal',
        arguments: { operation: 'execute', tool: 'system_control', action: 'get_project_settings', params: {} }
      });

      // Then: structured and text-only consumers agree about the gateway failure.
      const structured = structuredPayload(response);
      const text = contentParts(response).find((part) => part.type === 'text')?.text;
      expect(response.isError).toBe(true);
      expect(structured).toMatchObject({ success: false, errorCode: 'OUTPUT_SCHEMA_VIOLATION' });
      const shown = String(text);
      expect(shown.split('\n')[0]).toMatch(/^Error \[OUTPUT_SCHEMA_VIOLATION\]: /u);
      expect(JSON.parse(shown.slice(shown.indexOf('{')))).toMatchObject({ success: false });
      expect(text).toContain('Inspect the current state before retrying.');
      expect(structured.result).toMatchObject({ success: true, settings: 'invalid settings shape' });
    } finally {
      await ctx.close();
    }
  });

  it('delivers screenshot bytes once while retaining capture metadata', async () => {
    // Given: enough image data to expose accidental payload duplication.
    const imageBase64 = 'QUJD'.repeat(4096);
    const capture = { success: true, imageBase64, mimeType: 'image/png', width: 320, height: 180 };
    const ctx = await connectClient('response-image-test', async () => capture);
    try {
      // When
      const response = await ctx.client.callTool({
        name: 'unreal',
        arguments: { operation: 'execute', tool: 'control_editor', action: 'screenshot', params: {} }
      });

      // Then: image content retains the original bytes; structured payloads do not repeat them.
      const parts = contentParts(response);
      expect(response.isError).not.toBe(true);
      expect(parts.filter((part) => part.type === 'image')).toEqual([
        { type: 'image', data: imageBase64, mimeType: 'image/png' }
      ]);
      expect(JSON.stringify(response).split(imageBase64)).toHaveLength(2);
      expect(JSON.stringify(response.structuredContent)).not.toContain(imageBase64);
      expect(structuredPayload(response).data).toMatchObject({ width: 320, height: 180, mimeType: 'image/png' });
      expect(capture.imageBase64).toBe(imageBase64);
    } finally {
      await ctx.close();
    }
  });
});
