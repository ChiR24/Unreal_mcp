import { describe, expect, it } from 'vitest';

import { validateUrlArgument } from './handler-url-validation.js';

const LOOPBACK_MEDIA_PREFIX = 'http://127.0.0.1:18080/media/';
const LOOPBACK_MEDIA_FILE =
  'http://127.0.0.1:18080/media/cinematics.webm';

describe('URL argument validation', () => {
  // validateUrlArgument blocks EVERY absolute URL, `file:` included: a file URL
  // gets its own message pointing at filePath/mediaPath, everything else gets
  // the "network media URLs are disabled" message. The cases below verify that
  // no loopback/hostname/port/path/credential combination can sneak through.
  it('rejects a loopback media URL', () => {
    expect(
      validateUrlArgument(
        'streamUrl',
        `${LOOPBACK_MEDIA_PREFIX}cinematics.mp4`,
      ),
    ).toContain('network media URLs are disabled');
  });

  it.each([
    ['hostname alias', 'http://localhost:18080/media/cinematics.mp4'],
    ['different port', 'http://127.0.0.1:18081/media/cinematics.mp4'],
    ['path escape', 'http://127.0.0.1:18080/media/../private/cinematics.mp4'],
    ['encoded path escape', 'http://127.0.0.1:18080/media/%2e%2e/private.mp4'],
    ['double-encoded path escape', 'http://127.0.0.1:18080/media/%252e%252e/private.mp4'],
    ['prefix boundary mismatch', 'http://127.0.0.1:18080/media-private/cinematics.mp4'],
    ['embedded credentials', 'http://user@127.0.0.1:18080/media/cinematics.mp4'],
    ['fragment', 'http://127.0.0.1:18080/media/cinematics.mp4#redirect'],
  ])('rejects a non-file URL with a %s bypass attempt', (_label, url) => {
    expect(
      validateUrlArgument('streamUrl', url),
    ).toContain('Security violation');
  });

  it('rejects all loopback media URLs unconditionally', () => {
    expect(
      validateUrlArgument(
        'streamUrl',
        `${LOOPBACK_MEDIA_PREFIX}cinematics.mp4`,
      ),
    ).toContain('network media URLs are disabled');
  });

  it('rejects a known media file URL and variant suffixes', () => {
    expect(
      validateUrlArgument('streamUrl', LOOPBACK_MEDIA_FILE),
    ).toContain('network media URLs are disabled');
    expect(
      validateUrlArgument(
        'streamUrl',
        `${LOOPBACK_MEDIA_FILE}.evil`,
      ),
    ).toContain('Security violation');
    expect(
      validateUrlArgument(
        'streamUrl',
        `${LOOPBACK_MEDIA_FILE}/child`,
      ),
    ).toContain('Security violation');
    expect(
      validateUrlArgument(
        'streamUrl',
        `${LOOPBACK_MEDIA_FILE}?variant=other`,
      ),
    ).toContain('Security violation');
  });
});
