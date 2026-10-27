import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

interface PackageJson {
  engines?: { node?: string } & Record<string, unknown>;
}

interface ServerJson {
  engines?: { node?: string } & Record<string, unknown>;
}

const readJson = <T>(rel: string): T => {
  const text = readFileSync(resolve(process.cwd(), rel), 'utf8');
  const parsed: unknown = JSON.parse(text);
  return parsed as T;
};

const DECLARED_FLOOR = '>=20.19.0';

describe('Node.js runtime floor', () => {
  it('declares the Node floor in package.json engines.node', () => {
    const pkg = readJson<PackageJson>('package.json');
    const nodeRange = pkg.engines?.node;
    expect(nodeRange, 'package.json engines.node is missing').toBeDefined();
    // The floor must not be widened: it must equal the declared floor exactly.
    expect(nodeRange).toBe(DECLARED_FLOOR);
  });

  it('declares the Node floor in server.json engines.node', () => {
    const server = readJson<ServerJson>('server.json');
    const nodeRange = server.engines?.node;
    expect(nodeRange, 'server.json engines.node is missing').toBeDefined();
    expect(nodeRange).toBe(DECLARED_FLOOR);
  });
});
