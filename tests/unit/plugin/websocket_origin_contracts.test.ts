import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

describe('WebSocket server origin contract', () => {
  it('rejects browser-origin upgrade requests before accepting the handshake', () => {
    const source = readFileSync(
      resolve(
        process.cwd(),
        'plugins/EngineWire/Source/EngineWire/Private/Transport/WebSocket/McpBridgeWebSocketServerHandshake.cpp',
      ),
      'utf8',
    );
    const originRead = source.indexOf('Key.Equals(TEXT("Origin")');
    const originRejection = source.indexOf('if (!Origin.IsEmpty())');
    const switchingProtocols = source.indexOf('HTTP/1.1 101 Switching Protocols');

    expect(originRead).toBeGreaterThan(-1);
    expect(originRejection).toBeGreaterThan(originRead);
    expect(originRejection).toBeLessThan(switchingProtocols);
    expect(source).toContain('Browser-origin WebSocket requests are not allowed.');
    expect(source).toContain('4403');
  });
});
