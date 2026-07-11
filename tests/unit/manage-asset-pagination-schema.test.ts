import { describe, expect, it } from 'vitest';
import { manageAssetToolDefinition } from '../../src/tools/definitions/core/manage-asset-tool.js';
import { gatewayManifest } from '../../src/gateway/gateway-manifest.generated.js';

// T4 follow-up: the canonical manage_asset `list` action runtime (T4) already supports
// opaque cursors, bounded limit/offset, a nested pagination object, and includeTags, and
// emits folders/count/hasMore/nextOffset/cursor/nextCursor. These fields were left out of
// the declared contract; this test locks them into the canonical definition and the
// generated gateway manifest.

describe('manage_asset list pagination contract (T4 follow-up)', () => {
  const inputProps = (manageAssetToolDefinition.inputSchema.properties ?? {}) as Record<string, unknown>;
  const outputSchema = manageAssetToolDefinition.outputSchema as { properties?: Record<string, unknown> } | undefined;
  const outputProps = (outputSchema?.properties ?? {}) as Record<string, unknown>;

  it('declares the list pagination input fields (cursor, includeTags, nested pagination)', () => {
    expect(inputProps).toHaveProperty('cursor');
    expect(inputProps).toHaveProperty('includeTags');
    expect(inputProps).toHaveProperty('pagination');

    const pagination = inputProps.pagination as { properties?: Record<string, unknown> };
    expect(pagination.properties).toHaveProperty('limit');
    expect(pagination.properties).toHaveProperty('offset');
  });

  it('declares the list pagination output fields (count, hasMore, nextOffset, cursor, nextCursor, folders)', () => {
    for (const field of ['count', 'hasMore', 'nextOffset', 'cursor', 'nextCursor', 'folders']) {
      expect(outputProps, `output schema should declare ${field}`).toHaveProperty(field);
    }
  });

  it('propagates the new fields into the generated gateway manifest', () => {
    const tool = gatewayManifest.tools.find((candidate) => candidate.name === 'manage_asset');
    expect(tool, 'manage_asset must be present in the gateway manifest').toBeDefined();

    const manifestInput = (tool as { inputSchema: { properties: Record<string, unknown> } }).inputSchema.properties;
    expect(manifestInput).toHaveProperty('cursor');
    expect(manifestInput).toHaveProperty('includeTags');
    expect(manifestInput).toHaveProperty('pagination');

    const parameterNames = (tool as { parameterNames: string[] }).parameterNames;
    expect(parameterNames).toContain('cursor');
    expect(parameterNames).toContain('includeTags');
    expect(parameterNames).toContain('pagination');

    expect((tool as { perActionSchemas: boolean }).perActionSchemas).toBe(false);
  });
});
