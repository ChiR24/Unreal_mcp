import { describe, expect, it } from 'vitest';
import { REDACTED_IMAGE_PAYLOAD, redactImagePayloadForLog, redactImagePayloadTextForLog } from './log-redaction.js';

describe('log redaction', () => {
  it('redacts image payloads from structured log previews', () => {
    const redacted = redactImagePayloadForLog({
      content: [
        { type: 'text', text: 'Screenshot captured' },
        { type: 'image', data: 'iVBORw0KGgoAAAANSUhEUgAAAAE=', mimeType: 'image/png' }
      ],
      structuredContent: {
        result: {
          imageBase64: 'iVBORw0KGgoAAAANSUhEUgAAAAE=',
          mimeType: 'image/png'
        }
      }
    });

    expect(redacted).toMatchObject({
      content: [
        { type: 'text', text: 'Screenshot captured' },
        { type: 'image', data: REDACTED_IMAGE_PAYLOAD, mimeType: 'image/png' }
      ],
      structuredContent: {
        result: {
          imageBase64: REDACTED_IMAGE_PAYLOAD,
          mimeType: 'image/png'
        }
      }
    });
  });

  it('redacts image payloads from raw bridge message previews', () => {
    const raw = JSON.stringify({
      type: 'automation_response',
      result: {
        imageBase64: 'short-image',
        mimeType: 'image/png'
      },
      content: [
        {
          type: 'image',
          data: 'short-data',
          mimeType: 'image/png'
        }
      ]
    });

    const redacted = redactImagePayloadTextForLog(raw);

    expect(redacted).toContain(`"imageBase64":"${REDACTED_IMAGE_PAYLOAD}"`);
    expect(redacted).toContain(`"data":"${REDACTED_IMAGE_PAYLOAD}"`);
    expect(redacted).not.toContain('short-image');
    expect(redacted).not.toContain('short-data');
  });
});
