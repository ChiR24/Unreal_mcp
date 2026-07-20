/**
 * tests/unit/canonical-registry-cpp-schema.test.ts
 *
 * Focused RED/GREEN generator tests for the Task-23 C++ schema emitter
 * (scripts/canonical-registry/cpp-schema.ts + cpp-registry.ts).
 *
 * These tests lock:
 *   - nested builder threading (object/array sub-builders receive `S`, never
 *     the outer `Schema`),
 *   - top-level and nested union nodes emit via `S.TypeUnion(...)` / the
 *     receiving builder, never `Schema.TypeUnion`,
 *   - top-level unconstrained-any emits via `AnyValue`,
 *   - the generated aggregator header carries no global AddAnyValue /
 *     AddTypeUnion helper functions.
 *
 * The tests deliberately avoid running the generator; they assert on the
 * emitted source text that the orchestrator will regenerate after merge.
 */
import { describe, expect, it } from 'vitest';
import { jsonSchemaToCppCalls } from '../../scripts/canonical-registry/cpp-schema.js';
import { buildAggregatorHeader, buildShardTargets } from '../../scripts/canonical-registry/cpp-registry.js';
import type { ToolDefinition } from '../../src/tools/definitions/shared/tool-definition.js';

const COMMON_PROP = (extra: Record<string, unknown> = {}): Record<string, unknown> => ({
  type: 'object',
  properties: { ...extra },
});

const emit = (schema: Record<string, unknown>): string[] =>
  jsonSchemaToCppCalls(schema as never).lines;

describe('cpp schema emitter — top-level any', () => {
  it('GREEN: a property with no type emits AnyValue on the outer builder', () => {
    const lines = emit({
      type: 'object',
      properties: { value: { description: 'Generic value.' } },
    });
    expect(lines.some((l) => l.includes('Schema.AnyValue(TEXT("value")'))).toBe(true);
    expect(lines.some((l) => l.includes('AddAnyValue'))).toBe(false);
  });
});

describe('cpp schema emitter — top-level union', () => {
  it('GREEN: an array-valued type emits TypeUnion on the outer builder', () => {
    const lines = emit({
      type: 'object',
      properties: {
        frameRate: {
          type: ['number', 'string'],
          description: 'Frame rate.',
        },
      },
    });
    expect(
      lines.some((l) =>
        l.includes('Schema.TypeUnion(TEXT("frameRate"), { TEXT("number"), TEXT("string") }'),
      ),
    ).toBe(true);
    expect(lines.some((l) => l.includes('AddTypeUnion'))).toBe(false);
  });
});

describe('cpp schema emitter — nested object union', () => {
  it('GREEN: a union nested inside an object sub-builder emits onto S', () => {
    const lines = emit(
      COMMON_PROP({
        settings: {
          type: 'object',
          properties: {
            rate: { type: ['number', 'string'], description: 'Rate.' },
          },
        },
      }),
    );
    // The sub-builder lambda opens with `S` and the nested union must use S.
    expect(lines.some((l) => l.includes('[](FMcpSchemaBuilder& S)'))).toBe(true);
    expect(lines.some((l) => l.trim().startsWith('S.TypeUnion(TEXT("rate")'))).toBe(true);
    // It must NOT reference the outer Schema inside the nested lambda.
    expect(lines.some((l) => l.includes('Schema.TypeUnion'))).toBe(false);
  });
});

describe('cpp schema emitter — nested array-object union', () => {
  it('GREEN: a union nested inside an array-of-objects sub-builder emits onto S', () => {
    const lines = emit(
      COMMON_PROP({
        tracks: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              kind: { type: ['string', 'number'], description: 'Kind.' },
            },
          },
        },
      }),
    );
    expect(lines.some((l) => l.includes('Schema.ArrayOfObjects(TEXT("tracks")'))).toBe(true);
    expect(lines.some((l) => l.includes('[](FMcpSchemaBuilder& S)'))).toBe(true);
    expect(lines.some((l) => l.trim().startsWith('S.TypeUnion(TEXT("kind")'))).toBe(true);
    expect(lines.some((l) => l.includes('Schema.TypeUnion'))).toBe(false);
  });

  it('GREEN: an any-value nested inside an array-of-objects emits onto S', () => {
    const lines = emit(
      COMMON_PROP({
        items: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              payload: { description: 'Free payload.' },
            },
          },
        },
      }),
    );
    expect(lines.some((l) => l.trim().startsWith('S.AnyValue(TEXT("payload")'))).toBe(true);
    expect(lines.some((l) => l.includes('Schema.AnyValue'))).toBe(false);
  });
});

describe('cpp schema emitter — manage_sequence representative fields', () => {
  it('GREEN: value (any) and frameRate (union) emit as first-class builder calls', () => {
    const lines = emit({
      type: 'object',
      properties: {
        value: { description: 'Generic keyframe or parameter value.' },
        frameRate: { type: ['number', 'string'], description: 'Frame rate.' },
      },
    });
    expect(
      lines.some((l) => l.includes('Schema.AnyValue(TEXT("value")')),
    ).toBe(true);
    expect(
      lines.some((l) =>
        l.includes('Schema.TypeUnion(TEXT("frameRate"), { TEXT("number"), TEXT("string") }'),
      ),
    ).toBe(true);
  });
});

describe('cpp schema emitter — nested union suite', () => {
  it('GREEN: nested unions inside object and nested object both use S', () => {
    const lines = emit(
      COMMON_PROP({
        outer: {
          type: 'object',
          properties: {
            inner: {
              type: 'object',
              properties: {
                mix: { type: ['boolean', 'string'], description: 'Mixed.' },
              },
            },
            top: { type: ['number', 'string'], description: 'Top.' },
          },
        },
      }),
    );
    const nested = lines.filter((l) => l.trim().startsWith('S.TypeUnion('));
    expect(nested.length).toBe(2);
    expect(nested.every((l) => l.includes('S.TypeUnion'))).toBe(true);
    expect(lines.some((l) => l.includes('Schema.TypeUnion'))).toBe(false);
  });
});

describe('cpp schema emitter — scalar vs structural union honesty', () => {
  it('GREEN: a flat scalar union keeps TypeUnion (exact type list, no loss)', () => {
    const lines = emit({
      type: 'object',
      properties: {
        frameRate: { type: ['number', 'string'], description: 'Frame rate.' },
      },
    });
    expect(
      lines.some((l) =>
        l.includes('Schema.TypeUnion(TEXT("frameRate"), { TEXT("number"), TEXT("string") }'),
      ),
    ).toBe(true);
    expect(lines.some((l) => l.includes('Schema.AnyValue(TEXT("frameRate")'))).toBe(false);
  });

  it('GREEN: a single-branch scalar oneOf keeps TypeUnion', () => {
    const lines = emit({
      type: 'object',
      properties: {
        mode: { oneOf: [{ type: 'string' }], description: 'Mode.' },
      },
    });
    expect(
      lines.some((l) => l.includes('Schema.TypeUnion(TEXT("mode"), { TEXT("string") }')),
    ).toBe(true);
  });

  it('GREEN: a structural oneOf with properties degrades to AnyValue (no lossy list)', () => {
    const lines = emit({
      type: 'object',
      properties: {
        color: {
          oneOf: [
            { type: 'object', properties: { r: { type: 'number' }, g: { type: 'number' } } },
            { type: 'string' },
          ],
          description: 'Color.',
        },
      },
    });
    expect(lines.some((l) => l.includes('Schema.AnyValue(TEXT("color")'))).toBe(true);
    expect(lines.some((l) => l.includes('Schema.TypeUnion(TEXT("color")'))).toBe(false);
  });

  it('GREEN: a multi-branch anyOf degrades to AnyValue', () => {
    const lines = emit({
      type: 'object',
      properties: {
        variant: {
          anyOf: [{ type: 'object', properties: { a: { type: 'string' } } }, { type: 'array', items: { type: 'number' } }],
          description: 'Variant.',
        },
      },
    });
    expect(lines.some((l) => l.includes('Schema.AnyValue(TEXT("variant")'))).toBe(true);
    expect(lines.some((l) => l.includes('Schema.TypeUnion(TEXT("variant")'))).toBe(false);
  });
});

describe('cpp schema emitter — color array/object unions', () => {
  it('GREEN: a color union nested in an object property degrades to AnyValue on S', () => {
    const lines = emit(
      COMMON_PROP({
        brush: {
          type: 'object',
          properties: {
            color: {
              oneOf: [
                { type: 'object', properties: { hex: { type: 'string' } } },
                { type: 'string' },
              ],
              description: 'Color.',
            },
          },
        },
      }),
    );
    expect(lines.some((l) => l.includes('[](FMcpSchemaBuilder& S)'))).toBe(true);
    expect(lines.some((l) => l.trim().startsWith('S.AnyValue(TEXT("color")'))).toBe(true);
    expect(lines.some((l) => l.includes('Schema.AnyValue'))).toBe(false);
  });

  it('GREEN: a color union inside an array-of-objects degrades to AnyValue on S', () => {
    const lines = emit(
      COMMON_PROP({
        swatches: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              fill: {
                oneOf: [
                  { type: 'object', properties: { r: { type: 'integer' } } },
                  { type: 'string' },
                ],
                description: 'Fill.',
              },
            },
          },
        },
      }),
    );
    expect(lines.some((l) => l.includes('Schema.ArrayOfObjects(TEXT("swatches")'))).toBe(true);
    expect(lines.some((l) => l.trim().startsWith('S.AnyValue(TEXT("fill")'))).toBe(true);
    expect(lines.some((l) => l.includes('Schema.AnyValue'))).toBe(false);
  });
});

describe('cpp schema emitter — nullable/type arrays', () => {
  it('GREEN: a nullable scalar (type array incl. null) keeps TypeUnion', () => {
    const lines = emit({
      type: 'object',
      properties: {
        label: { type: ['string', 'null'], description: 'Label.' },
      },
    });
    expect(
      lines.some((l) => l.includes('Schema.TypeUnion(TEXT("label"), { TEXT("string"), TEXT("null") }')),
    ).toBe(true);
  });

  it('GREEN: a scalar type array (string/number/boolean) keeps TypeUnion', () => {
    const lines = emit({
      type: 'object',
      properties: {
        value: { type: ['string', 'number', 'boolean'], description: 'Value.' },
      },
    });
    expect(
      lines.some((l) =>
        l.includes('Schema.TypeUnion(TEXT("value"), { TEXT("string"), TEXT("number"), TEXT("boolean") }'),
      ),
    ).toBe(true);
  });
});

describe('cpp schema emitter — freeform objects', () => {
  it('GREEN: a freeform object (additionalProperties only) emits FreeformObject', () => {
    const lines = emit({
      type: 'object',
      properties: {
        meta: { type: 'object', additionalProperties: true, description: 'Meta.' },
      },
    });
    expect(lines.some((l) => l.includes('Schema.FreeformObject(TEXT("meta")'))).toBe(true);
  });

  it('GREEN: a freeform object nested in an array-of-objects emits onto S', () => {
    const lines = emit(
      COMMON_PROP({
        extras: {
          type: 'array',
          items: {
            type: 'object',
            properties: {
              blob: { type: 'object', additionalProperties: true, description: 'Blob.' },
            },
          },
        },
      }),
    );
    expect(lines.some((l) => l.trim().startsWith('S.FreeformObject(TEXT("blob")'))).toBe(true);
    expect(lines.some((l) => l.includes('Schema.FreeformObject'))).toBe(false);
  });
});

describe('cpp schema emitter — union branches carrying array-valued types', () => {
  it('GREEN: a branch whose own type is an array contributes every member to TypeUnion', () => {
    const lines = emit({
      type: 'object',
      properties: {
        resolution: {
          oneOf: [{ type: ['number', 'string'] }, { type: 'string' }],
          description: 'Tick resolution or WIDTHxHEIGHT.',
        },
      },
    });
    expect(
      lines.some((l) =>
        l.includes('Schema.TypeUnion(TEXT("resolution"), { TEXT("number"), TEXT("string") }'),
      ),
    ).toBe(true);
  });

  it('RED-guard: the number member is never dropped down to a string-only union', () => {
    const lines = emit({
      type: 'object',
      properties: {
        resolution: {
          oneOf: [{ type: ['number', 'string'] }, { type: 'string' }],
          description: 'Tick resolution or WIDTHxHEIGHT.',
        },
      },
    });
    expect(
      lines.some((l) => l.includes('Schema.TypeUnion(TEXT("resolution"), { TEXT("string") })')),
    ).toBe(false);
  });

  it('GREEN: a nested branch with an array-valued type also keeps every member on S', () => {
    const lines = emit(
      COMMON_PROP({
        settings: {
          type: 'object',
          properties: {
            rate: {
              anyOf: [{ type: ['integer', 'string'] }, { type: 'number' }],
              description: 'Rate.',
            },
          },
        },
      }),
    );
    expect(
      lines.some((l) =>
        l
          .trim()
          .startsWith('S.TypeUnion(TEXT("rate"), { TEXT("integer"), TEXT("string"), TEXT("number") }'),
      ),
    ).toBe(true);
    expect(lines.some((l) => l.includes('Schema.TypeUnion'))).toBe(false);
  });
});

describe('cpp schema emitter — manage_sequence honest scalar/integer/object mapping', () => {
  it('GREEN: integer fields emit Integer, not Number', () => {
    const lines = emit({
      type: 'object',
      properties: {
        width: { type: 'integer', description: 'Output width.' },
        height: { type: 'integer', description: 'Output height.' },
        startFrame: { type: 'integer', description: 'Start frame.' },
        timeoutMs: { type: 'number', description: 'Timeout.' },
      },
    });
    for (const name of ['width', 'height', 'startFrame']) {
      expect(lines.some((l) => l.includes(`Schema.Integer(TEXT("${name}")`))).toBe(true);
      expect(lines.some((l) => l.includes(`Schema.Number(TEXT("${name}")`))).toBe(false);
    }
    expect(lines.some((l) => l.includes('Schema.Number(TEXT("timeoutMs")'))).toBe(true);
  });

  it('GREEN: a structured settings object emits a sub-builder with integer members', () => {
    const lines = emit({
      type: 'object',
      properties: {
        settings: {
          type: 'object',
          description: 'Nested MRQ output settings.',
          properties: {
            handleFrameCount: { type: 'integer', description: 'Handle frames.' },
            zeroPadFrameNumbers: { type: 'integer', description: 'Zero pad.' },
          },
        },
      },
    });
    expect(lines.some((l) => l.includes('Schema.Object(TEXT("settings")'))).toBe(true);
    expect(lines.some((l) => l.trim().startsWith('S.Integer(TEXT("handleFrameCount")'))).toBe(true);
    expect(lines.some((l) => l.trim().startsWith('S.Integer(TEXT("zeroPadFrameNumbers")'))).toBe(true);
  });

  it('GREEN: a reflection-boundary map emits FreeformObject, not a typed Object', () => {
    const lines = emit({
      type: 'object',
      properties: {
        platformSources: {
          type: 'object',
          additionalProperties: true,
          description: 'Per-platform media source paths.',
        },
      },
    });
    expect(lines.some((l) => l.includes('Schema.FreeformObject(TEXT("platformSources")'))).toBe(true);
  });
});

describe('cpp registry — manage_sequence strict native arguments', () => {
  const parent = (name: string, category: string): ToolDefinition =>
    ({
      name,
      description: `${name} description`,
      category,
      inputSchema: {
        type: 'object',
        properties: { value: { description: 'Generic value (any type).' } },
      },
    }) as ToolDefinition;

  it('GREEN: manage_sequence emits EnforceStrictArguments() -> true', () => {
    const shard = buildShardTargets([parent('manage_sequence', 'utility')]).find(
      (s) => s.shard === 'Utility_Sequence',
    );
    expect(shard).toBeDefined();
    expect(shard?.content).toContain('bool EnforceStrictArguments() const override { return true; }');
  });

  it('GREEN: a non-strict parent does not emit EnforceStrictArguments', () => {
    const shard = buildShardTargets([parent('manage_audio', 'utility')]).find(
      (s) => s.shard === 'Utility_Audio',
    );
    expect(shard).toBeDefined();
    expect(shard?.content).not.toContain('EnforceStrictArguments');
  });
});

describe('cpp aggregator header — helper-free', () => {
  it('GREEN: the generated header contains no global AddAnyValue/AddTypeUnion', () => {
    const header = buildAggregatorHeader();
    expect(header.includes('AddAnyValue')).toBe(false);
    expect(header.includes('AddTypeUnion')).toBe(false);
    expect(header.includes('GetSchemaProperties')).toBe(false);
  });

  it('GREEN: the generated header preserves class-before-registration shape', () => {
    const header = buildAggregatorHeader();
    expect(header.includes('class FMcpGeneratedParentRegistry')).toBe(true);
    const classIdx = header.indexOf('class FMcpGeneratedParentRegistry');
    const regIdx = header.indexOf('RegisterGenerated');
    expect(classIdx).toBeGreaterThanOrEqual(0);
    expect(regIdx).toBeGreaterThan(classIdx);
  });
});
