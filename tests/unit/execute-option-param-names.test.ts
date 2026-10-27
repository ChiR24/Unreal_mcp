// A record parameter named like an execute option can never be sent: the gateway refuses every option name inside
// params (McpRejectReservedParams), so such a parameter is unreachable. The names are read from the C++ that refuses them.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

import { ALL_CAPABILITY_RECORDS } from '../../src/tools/catalog/capabilities/records/aggregate.js';

const SOURCE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'MCP', 'Execute', 'McpNativeGatewayExecuteRequest.cpp');

describe('record parameters and execute options', () => {
  it('no record declares a parameter the gateway refuses inside params', () => {
    const block = /McpExecutionOptionKeys\(\)\s*\{[\s\S]*?\};/u.exec(readFileSync(SOURCE, 'utf8'))?.[0] ?? '';
    const options = [...block.matchAll(/TEXT\("([A-Za-z]+)"\)/gu)].map((match) => match[1]);
    expect(options).toContain('expect');
    expect(options).toContain('timeoutMs');
    const clashes = ALL_CAPABILITY_RECORDS.flatMap((record) =>
      options.filter((name) => Object.hasOwn(record.schemas.input.properties, name)).map((name) => `${record.id}: ${name}`));
    expect(clashes).toEqual([]);
  });
});
