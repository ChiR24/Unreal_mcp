// tests/unit/cross-transport/execute-payload-location.test.ts
// Task 46 F1: the capability payload must be reachable at the DOCUMENTED
// location — `structuredContent.data` — on BOTH transports.
//
// The fixture is RAW WIRE BYTES captured from one live editor (see its
// `provenance` block), not a hand-shaped object. That distinction is the whole
// point: F3 was misjudged as a false positive because its test fed a payload
// hand-authored in the shape the code expected, a shape the transport never
// produces. A green test only licenses a verdict when its input is what
// production actually emits.

import { readFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { describe, expect, it } from 'vitest';

import { executeSuccessEnvelope } from '../../../src/server/gateway/gateway-execute-envelope.js';
import { loadRecords } from './matrix-dimensions.mjs';

const here = dirname(fileURLToPath(import.meta.url));

type WireFixture = {
  readonly provenance: Record<string, string>;
  readonly nativeSseFrame: string;
  readonly stdioJsonRpcFrame: Record<string, unknown>;
};

const fixture = JSON.parse(
  readFileSync(resolve(here, 'fixtures/execute-success-wire.json'), 'utf8')
) as WireFixture;

/** Unwrap an SSE `data:` frame or a bare JSON-RPC frame down to the tool result. */
function toolResult(raw: unknown): Record<string, unknown> {
  let value: unknown = raw;
  if (typeof value === 'string') {
    const dataLine = value.split(/\r?\n/u).find((line) => line.startsWith('data:'));
    value = JSON.parse(dataLine === undefined ? value : dataLine.slice(5).trim());
  }
  const envelope = value as Record<string, unknown>;
  return (envelope.result ?? envelope) as Record<string, unknown>;
}

function structuredContent(raw: unknown): Record<string, unknown> {
  const structured = toolResult(raw).structuredContent;
  expect(structured, 'the transport returned no structuredContent at all').toBeTruthy();
  return structured as Record<string, unknown>;
}

/** Keys a client would read from the documented payload location. */
function documentedPayloadKeys(raw: unknown): string[] {
  const data = structuredContent(raw).data;
  return typeof data === 'object' && data !== null ? Object.keys(data).sort() : [];
}

describe('Task 46 F1 — the capability payload sits at the documented location on both transports', () => {
  it('the captured fixture really is a successful execute on both sides', () => {
    // Guards the assertions below against passing over a refusal or an empty
    // capture, which would make every expectation vacuously satisfiable.
    expect(fixture.provenance.capability).toBe('manage_effect.list_debug_shapes');
    expect(toolResult(fixture.nativeSseFrame).isError).not.toBe(true);
    expect(toolResult(fixture.stdioJsonRpcFrame).isError).not.toBe(true);
  });

  it('native exposes the payload at structuredContent.data', () => {
    expect(documentedPayloadKeys(fixture.nativeSseFrame)).not.toEqual([]);
  });

  it('stdio exposes the payload at structuredContent.data', () => {
    expect(documentedPayloadKeys(fixture.stdioJsonRpcFrame)).not.toEqual([]);
  });

  it('both transports agree on what that payload contains', () => {
    expect(documentedPayloadKeys(fixture.stdioJsonRpcFrame)).toEqual(
      documentedPayloadKeys(fixture.nativeSseFrame)
    );
  });

  it('the stdio producer puts the schema-projected output at `data`, not only inside the receipt', () => {
    // Drives the real producer with the real handler result the bridge sent, so
    // this stays true of the code even if the fixture is later re-captured.
    const record = loadRecords().find(
      (candidate: { id: string }) => candidate.id === 'manage_effect.list_debug_shapes'
    );
    expect(record, 'capability record missing from the generated registry').toBeTruthy();

    const stdioStructured = structuredContent(fixture.stdioJsonRpcFrame);
    const handlerResult = stdioStructured.result as Record<string, unknown>;
    expect(handlerResult, 'fixture carried no raw handler result to replay').toBeTruthy();

    const envelope = executeSuccessEnvelope(
      {
        record: record as never,
        result: handlerResult,
        canonicalOutput: { success: true, message: 'Available debug shape types', shapes: handlerResult.shapes },
        warnings: []
      },
      { correlationId: 'test-correlation', startedAtMs: 0 } as never
    );

    expect(Object.keys(envelope)).toContain('data');
    expect(envelope.data).toEqual((envelope.receipt as { data: unknown }).data);
  });
});
